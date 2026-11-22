#include "lab.hpp"
#include <algorithm>
#include <cmath>
#include <omp.h>
#include <stdexcept>
#include <vector>

namespace lab {
namespace {
Word mix(Word x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
double coordinate(Word counter, Word seed) {
    constexpr Word gamma = 0x9e3779b97f4a7c15ULL;
    const Word bits = mix(seed + gamma * (counter + 1));
    return 2.0 * static_cast<double>(bits >> 11) * 0x1p-53 - 1.0;
}
bool inside(Word i, Word seed) {
    const double x = coordinate(2*i, seed), y = coordinate(2*i+1, seed);
    return x*x + y*y <= 1;
}
void validate_run(Word n, int workers) {
    if (!n || n > max_samples || workers < 0)
        throw std::invalid_argument("N must be in [1, 2^53-1]; invalid N or thread count");
}
void validate_market(const Market& m) {
    if (!std::isfinite(m.spot) || !std::isfinite(m.strike) || !std::isfinite(m.rate) ||
        !std::isfinite(m.sigma) || !std::isfinite(m.maturity) ||
        m.spot < 0 || m.strike < 0 || m.sigma < 0 || m.maturity < 0)
        throw std::invalid_argument("finite market inputs required; S0, K, sigma and T must be >= 0");
}
double checked(double v) {
    if (!std::isfinite(v)) throw std::range_error("model exceeds floating-point range");
    return v;
}
double cdf(double z) { return 0.5 * std::erfc(-z / std::sqrt(2.0)); }

template<class Sequence>
Result simulate(Word n, Sequence sequence, const Market& m, int workers) {
    Result out;
    const double start = omp_get_wtime();
    const double vol = checked(m.sigma * std::sqrt(m.maturity));
    const double drift = checked(m.rate*m.maturity - 0.5*vol*vol);
    const double discount = checked(std::exp(-m.rate*m.maturity));
    if (discount == 0) throw std::range_error("discount underflows double");
    const auto payoff = [&](double z) {
        if (m.spot == 0) return 0.0;
        return std::max(m.spot * std::exp(drift + vol*z) - m.strike, 0.0);
    };
    double sum = 0;
    {
        std::vector<double> z(std::min(n, block_size));
        for (Word done = 0; done < n;) {
            const Word count = std::min(n-done, block_size);
            const double gen_start = omp_get_wtime();
            for (Word i = 0; i < count; ++i) z[i] = inv_normal(sequence.next());
            const double kernel_start = omp_get_wtime();
            out.generation += kernel_start - gen_start;
            double part = 0;
            if (workers == 0) {
                for (Word i = 0; i < count; ++i) part += payoff(z[i]);
            } else {
                #pragma omp parallel num_threads(workers)
                {
                    #pragma omp master
                    out.threads = omp_get_num_threads();
                    #pragma omp for schedule(static) reduction(+:part)
                    for (Word i = 0; i < count; ++i) part += payoff(z[i]);
                }
            }
            sum += part;
            out.kernel += omp_get_wtime() - kernel_start;
            done += count;
        }
    }
    out.value = checked((sum / static_cast<double>(n)) * discount);
    out.total = omp_get_wtime() - start;
    return out;
}
}

double black_scholes(const Market& m) {
    validate_market(m);
    if (m.maturity == 0) return std::max(m.spot-m.strike, 0.0);
    if (m.spot == 0 || m.strike == 0) return m.spot;
    const double rt = checked(m.rate*m.maturity);
    const double pv = checked(m.strike * checked(std::exp(-rt)));
    if (m.sigma == 0) return std::max(m.spot-pv, 0.0);
    const double vol = checked(m.sigma*std::sqrt(m.maturity));
    if (vol == 0) return std::max(m.spot-pv, 0.0);
    const double center = (std::log(m.spot) - std::log(m.strike) + rt) / vol;
    return std::max(checked(m.spot*cdf(center+0.5*vol) - pv*cdf(center-0.5*vol)), 0.0);
}

Result pi(Word n, Word seed, int workers) {
    validate_run(n, workers);
    Result out;
    const double start = omp_get_wtime();
    Word hits = 0;
    if (workers == 0) {
        for (Word i = 0; i < n; ++i) hits += inside(i, seed);
    } else {
        #pragma omp parallel num_threads(workers)
        {
            #pragma omp master
            out.threads = omp_get_num_threads();
            #pragma omp for schedule(static) reduction(+:hits)
            for (Word i = 0; i < n; ++i) hits += inside(i, seed);
        }
    }
    out.hits = hits;
    out.value = 4.0*hits/n;
    out.total = out.kernel = omp_get_wtime() - start;
    return out;
}

Result call(Word n, Generator gen, Word seed, const Market& market, int workers) {
    validate_run(n, workers);
    validate_market(market);
    if (gen == Generator::sobol) return simulate(n, Sobol1D{}, market, workers);
    return simulate(n, Mcg59(seed), market, workers);
}
}
