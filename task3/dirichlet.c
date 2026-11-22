/* Lab 2, task 3: five-point Poisson stencil and block-wavefront Gauss-Seidel. */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double PI = 3.14159265358979323846;

typedef enum { SOURCE_SIN, SOURCE_POLY, SOURCE_ZERO, SOURCE_FILE } Source;
static const char *const SOURCE_NAMES[] = {"sin", "poly", "zero", "file"};

typedef struct {
    int n;                  /* Interior nodes per axis. */
    int block;
    int threads;
    int max_iters;
    double eps;
    double boundary;
    bool serial;
    Source source;
    const char *rhs_path;
    const char *output_path;
} Options;

typedef struct {
    int n;
    size_t stride;          /* Full row width, including both boundaries. */
    double h;
    double *u;
    double *rhs;            /* h^2 * f at each interior node. */
} Grid;

typedef struct {
    int iterations;
    int threads;
    bool converged;
    double max_update;
} Result;

typedef struct {
    double max_residual;
    double max_exact_error;
} Diagnostics;

static int min_int(int a, int b)
{
    return a < b ? a : b;
}

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "Usage: %s [options]\n"
            "  --n N           Interior nodes per axis (default 63)\n"
            "  --block B       Block side in nodes (default 16)\n"
            "  --threads P     OpenMP threads (default min(4, available CPUs))\n"
            "  --eps EPS       Stop at max absolute update <= EPS (default 1e-9)\n"
            "  --max-iters K   Sweep limit (default 1000000)\n"
            "  --boundary C    Temperature on all four edges (default 20)\n"
            "  --case NAME     sin, poly, or zero (default sin)\n"
            "  --rhs PATH      Read N*N whitespace-separated f values in row order\n"
            "                  Specify either --case or --rhs\n"
            "  --output PATH   Write x,y,u to a CSV file, including the edges\n"
            "  --serial        Use the reference row-by-row Gauss-Seidel solver\n"
            "  --help          Show this help\n"
            "Exit codes: 0 converged, 1 error, 2 sweep limit reached.\n",
            program);
}

static bool parse_int(const char *text, int limit, int *value)
{
    char *end;
    errno = 0;
    const long number = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || number < 1 || number > limit) {
        return false;
    }
    *value = (int)number;
    return true;
}

static bool parse_real(const char *text, double *value)
{
    char *end;
    errno = 0;
    const double number = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(number)) {
        return false;
    }
    *value = number;
    return true;
}

static bool parse_options(int argc, char **argv, Options *options)
{
    bool explicit_case = false;
    for (int index = 1; index < argc; ++index) {
        const char *flag = argv[index];
        if (strcmp(flag, "--serial") == 0) {
            options->serial = true;
            continue;
        }
        if (index == argc - 1) {
            fprintf(stderr, "Missing value or unknown option: %s\n", flag);
            return false;
        }
        const char *value = argv[++index];
        bool valid = true;
        if (strcmp(flag, "--n") == 0) {
            /* Leave room for 2*n-1 wave indices and for the boundary nodes. */
            valid = parse_int(value, INT_MAX / 2, &options->n);
        } else if (strcmp(flag, "--block") == 0) {
            valid = parse_int(value, INT_MAX, &options->block);
        } else if (strcmp(flag, "--threads") == 0) {
            valid = parse_int(value, INT_MAX, &options->threads);
        } else if (strcmp(flag, "--max-iters") == 0) {
            valid = parse_int(value, INT_MAX, &options->max_iters);
        } else if (strcmp(flag, "--eps") == 0) {
            valid = parse_real(value, &options->eps) && options->eps > 0.0;
        } else if (strcmp(flag, "--boundary") == 0) {
            valid = parse_real(value, &options->boundary);
        } else if (strcmp(flag, "--case") == 0) {
            valid = false;
            for (int source = 0; source < SOURCE_FILE; ++source) {
                if (strcmp(value, SOURCE_NAMES[source]) == 0) {
                    options->source = (Source)source;
                    explicit_case = true;
                    valid = true;
                    break;
                }
            }
        } else if (strcmp(flag, "--rhs") == 0) {
            options->rhs_path = value;
            valid = *value != '\0';
        } else if (strcmp(flag, "--output") == 0) {
            options->output_path = value;
            valid = *value != '\0';
        } else {
            fprintf(stderr, "Unknown option: %s\n", flag);
            return false;
        }
        if (!valid) {
            fprintf(stderr, "Invalid value for %s: %s\n", flag, value);
            return false;
        }
    }
    if (options->rhs_path != NULL) {
        if (explicit_case) {
            fprintf(stderr, "Specify either --case or --rhs.\n");
            return false;
        }
        options->source = SOURCE_FILE;
    }
    return true;
}

static double exact_offset(Source source, double x, double y)
{
    if (source == SOURCE_SIN) {
        return sin(PI * x) * sin(PI * y);
    }
    if (source == SOURCE_POLY) {
        return 16.0 * x * (1.0 - x) * y * (1.0 - y);
    }
    return 0.0;
}

static double source_value(Source source, double x, double y)
{
    if (source == SOURCE_SIN) {
        return -2.0 * PI * PI * exact_offset(source, x, y);
    }
    if (source == SOURCE_POLY) {
        return -32.0 * (x * (1.0 - x) + y * (1.0 - y));
    }
    return 0.0;
}

static bool read_source(Grid *grid, const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror(path);
        return false;
    }
    const double h2 = grid->h * grid->h;
    char token[128];
    bool ok = true;
    for (int row = 1; row <= grid->n && ok; ++row) {
        for (int col = 1; col <= grid->n; ++col) {
            double value;
            if (fscanf(file, "%127s", token) != 1 || strlen(token) == sizeof token - 1 ||
                !parse_real(token, &value)) {
                fprintf(stderr, "Expected finite f at row %d, column %d in %s.\n",
                        row, col, path);
                ok = false;
                break;
            }
            grid->rhs[(size_t)row * grid->stride + (size_t)col] = h2 * value;
        }
    }
    if (ok && (fscanf(file, "%127s", token) != EOF || ferror(file))) {
        fprintf(stderr, "Extra data or read error in %s.\n", path);
        ok = false;
    }
    if (fclose(file) != 0) {
        perror(path);
        ok = false;
    }
    return ok;
}

static bool init_grid(Grid *grid, const Options *options)
{
    grid->n = options->n;
    grid->stride = (size_t)grid->n + 2;
    grid->h = 1.0 / (grid->n + 1);
    const size_t stride = grid->stride;
    if (stride > SIZE_MAX / sizeof(double) / stride) {
        fprintf(stderr, "Grid size exceeds the addressable array size.\n");
        return false;
    }
    const size_t count = stride * stride;
    grid->u = malloc(count * sizeof *grid->u);
    grid->rhs = malloc(count * sizeof *grid->rhs);
    if (grid->u == NULL || grid->rhs == NULL) {
        fprintf(stderr, "Could not allocate the two grid arrays.\n");
        return false;
    }
    const double h2 = grid->h * grid->h;
    for (size_t row = 0; row < stride; ++row) {
        for (size_t col = 0; col < stride; ++col) {
            const size_t pos = row * stride + col;
            grid->u[pos] = options->boundary;
            grid->rhs[pos] = 0.0;
            if (row > 0 && row + 1 < stride && col > 0 && col + 1 < stride &&
                options->source != SOURCE_FILE) {
                grid->rhs[pos] = h2 * source_value(options->source,
                                                  (double)col * grid->h,
                                                  (double)row * grid->h);
            }
        }
    }
    return options->source != SOURCE_FILE || read_source(grid, options->rhs_path);
}

/* Both upper bounds are exclusive. Only interior nodes may be updated. */
static double update_block(Grid *grid, int row_first, int row_limit,
                           int col_first, int col_limit)
{
    const size_t stride = grid->stride;
    double *u = grid->u;
    const double *rhs = grid->rhs;
    double largest = 0.0;

    for (int row = row_first; row < row_limit; ++row) {
        size_t pos = (size_t)row * stride + (size_t)col_first;
        for (int col = col_first; col < col_limit; ++col, ++pos) {
            const double old = u[pos];
            const double next = 0.25 * (u[pos - stride] + u[pos + stride] +
                                        u[pos - 1] + u[pos + 1] - rhs[pos]);
            const double change = fabs(next - old);
            if (!isfinite(change)) {
                return INFINITY;
            }
            u[pos] = next;
            if (change > largest) {
                largest = change;
            }
        }
    }
    return largest;
}

static Result solve_serial(Grid *grid, const Options *options)
{
    Result result = {.threads = 1};
    const int limit = grid->n + 1;
    for (int k = 0; k < options->max_iters; ++k) {
        result.max_update = update_block(grid, 1, limit, 1, limit);
        result.iterations = k + 1;
        result.converged = result.max_update <= options->eps;
        if (result.converged || !isfinite(result.max_update)) {
            break;
        }
    }
    return result;
}

static Result solve_wave(Grid *grid, const Options *options)
{
    const int n = grid->n;
    const int block = options->block;
    const int blocks = 1 + (n - 1) / block;
    const int waves = 2 * blocks - 1;
    Result result = {0};
    double max_update = 0.0;
    bool stop = false;

    #pragma omp parallel default(none) num_threads(options->threads) \
        shared(grid, options, n, block, blocks, waves, result, max_update, stop)
    {
        #pragma omp single
        {
            result.threads = omp_get_num_threads();
        }

        for (int k = 0; k < options->max_iters; ++k) {
            for (int wave = 0; wave < waves; ++wave) {
                const int first = wave < blocks ? 0 : wave - (blocks - 1);
                const int last = min_int(wave, blocks - 1);

                #pragma omp for schedule(static) reduction(max:max_update)
                for (int br = first; br <= last; ++br) {
                    const int bc = wave - br;
                    const int row_first = 1 + br * block;
                    const int col_first = 1 + bc * block;
                    const int row_limit = row_first + min_int(block, n + 1 - row_first);
                    const int col_limit = col_first + min_int(block, n + 1 - col_first);
                    const double change = update_block(grid, row_first, row_limit,
                                                       col_first, col_limit);
                    if (change > max_update) {
                        max_update = change;
                    }
                }
                /* The omp for barrier publishes this diagonal to the next. */
            }

            #pragma omp single
            {
                result.iterations = k + 1;
                result.max_update = max_update;
                result.converged = max_update <= options->eps;
                stop = result.converged || !isfinite(max_update);
                max_update = 0.0;
            }
            /* The single barrier publishes the decision and the reset. */
            if (stop) {
                break;
            }
        }
    }
    return result;
}

static Diagnostics analyze_grid(const Grid *grid, const Options *options)
{
    Diagnostics diagnostics = {0};
    const size_t stride = grid->stride;
    const double h2 = grid->h * grid->h;
    const double *u = grid->u;
    for (int row = 1; row <= grid->n; ++row) {
        for (int col = 1; col <= grid->n; ++col) {
            const size_t pos = (size_t)row * stride + (size_t)col;
            const double center = u[pos];
            const double sum = (u[pos - stride] - center) + (u[pos + stride] - center) +
                               (u[pos - 1] - center) + (u[pos + 1] - center);
            const double residual = fabs((sum - grid->rhs[pos]) / h2);
            diagnostics.max_residual = fmax(diagnostics.max_residual,
                                            isfinite(residual) ? residual : INFINITY);
            if (options->source != SOURCE_FILE) {
                const double exact = options->boundary +
                                     exact_offset(options->source, col * grid->h, row * grid->h);
                diagnostics.max_exact_error = fmax(diagnostics.max_exact_error,
                                                   fabs(center - exact));
            }
        }
    }
    return diagnostics;
}

static bool write_csv(const Grid *grid, const char *path)
{
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror(path);
        return false;
    }
    fprintf(file, "x,y,u\n");
    for (size_t row = 0; row < grid->stride && !ferror(file); ++row) {
        for (size_t col = 0; col < grid->stride; ++col) {
            const double x = col + 1 == grid->stride ? 1.0 : (double)col * grid->h;
            const double y = row + 1 == grid->stride ? 1.0 : (double)row * grid->h;
            fprintf(file, "%.17g,%.17g,%.17g\n", x, y,
                    grid->u[row * grid->stride + col]);
        }
    }
    bool ok = !ferror(file);
    if (fclose(file) != 0) {
        ok = false;
    }
    if (!ok) {
        fprintf(stderr, "Could not finish writing %s.\n", path);
    }
    return ok;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }
    Options options = {.n = 63, .block = 16, .threads = min_int(4, omp_get_num_procs()),
                       .max_iters = 1000000, .eps = 1e-9, .boundary = 20.0,
                       .source = SOURCE_SIN};
    if (!parse_options(argc, argv, &options)) {
        return EXIT_FAILURE;
    }

    Grid grid = {0};
    int status = EXIT_FAILURE;
    if (!init_grid(&grid, &options)) {
        goto cleanup;
    }

    omp_set_dynamic(0);
    const double start = omp_get_wtime();
    const Result result = options.serial ? solve_serial(&grid, &options)
                                         : solve_wave(&grid, &options);
    const double elapsed = omp_get_wtime() - start;
    if (!isfinite(result.max_update)) {
        fprintf(stderr, "Non-finite update at sweep %d.\n", result.iterations);
        goto cleanup;
    }

    const Diagnostics diagnostics = analyze_grid(&grid, &options);
    printf("mode=%s case=%s n=%d block=%d threads=%d iterations=%d converged=%s seconds=%.9f\n",
           options.serial ? "serial" : "wave", SOURCE_NAMES[options.source], options.n,
           options.block, result.threads, result.iterations, result.converged ? "yes" : "no",
           elapsed);
    printf("h=%.17g max_update=%.17g max_residual=%.17g\n",
           grid.h, result.max_update, diagnostics.max_residual);
    if (options.source != SOURCE_FILE) {
        printf("max_exact_error=%.17g\n", diagnostics.max_exact_error);
    }
    if (options.output_path != NULL && !write_csv(&grid, options.output_path)) {
        goto cleanup;
    }
    status = result.converged ? EXIT_SUCCESS : 2;

cleanup:
    free(grid.rhs);
    free(grid.u);
    return status;
}
