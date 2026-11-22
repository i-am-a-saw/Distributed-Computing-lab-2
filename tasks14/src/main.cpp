#include "lab.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <omp.h>
#include <set>
#include <stdexcept>
#include <string>

namespace {
using lab::Word;
const char* usage = R"(lab2 pi|call [--mode seq|par] [--n N] [--threads P] [--seed S]
          [--repeat R] [--warmup W]
lab2 call additionally accepts --gen mcg59|sobol --s0 S --k K --r R --sigma V --t T
Defaults: seq, N=1000000, P=1, seed=1, R=1, W=0; mcg59, S0=K=100, r=.05, sigma=.2, T=1.
MCG59 needs an odd seed in [1,2^59); Sobol uses no seed. Output is CSV.
)";
Word number(const std::string& s) {
    Word value{};
    const auto [end, ec] = std::from_chars(s.data(), s.data()+s.size(), value);
    if (ec != std::errc{} || end != s.data()+s.size())
        throw std::invalid_argument("invalid unsigned integer: " + s);
    return value;
}
int small(const std::string& s, bool allow_zero = false) {
    const Word v = number(s);
    if ((!allow_zero && !v) || v > std::numeric_limits<int>::max())
        throw std::invalid_argument("integer out of range: " + s);
    return static_cast<int>(v);
}
double real(const std::string& s) {
    size_t end = 0;
    const double x = std::stod(s, &end);
    if (end != s.size() || !std::isfinite(x)) throw std::invalid_argument("invalid real: " + s);
    return x;
}
void run(int argc, char** argv) {
    if (argc < 2) throw std::invalid_argument(usage);
    const std::string task = argv[1];
    if (task == "--help") { std::cout << usage; return; }
    if (task != "pi" && task != "call") throw std::invalid_argument(usage);
    Word n = 1000000, seed = 1;
    int threads = 1, repeat = 1, warmup = 0;
    lab::Market market;
    std::string mode = "seq", gen = "mcg59";
    std::set<std::string> seen;
    for (int i = 2; i < argc; i += 2) {
        const std::string k = argv[i];
        if (i+1 == argc || !seen.insert(k).second) throw std::invalid_argument("missing value or duplicate: " + k);
        const std::string v = argv[i+1];
        if (k == "--n") n = number(v);
        else if (k == "--seed") seed = number(v);
        else if (k == "--threads") threads = small(v);
        else if (k == "--repeat") repeat = small(v);
        else if (k == "--warmup") warmup = small(v, true);
        else if (k == "--mode") mode = v;
        else if (task != "call") throw std::invalid_argument("unknown pi option: " + k);
        else if (k == "--gen") gen = v;
        else if (k == "--s0") market.spot = real(v);
        else if (k == "--k") market.strike = real(v);
        else if (k == "--r") market.rate = real(v);
        else if (k == "--sigma") market.sigma = real(v);
        else if (k == "--t") market.maturity = real(v);
        else throw std::invalid_argument("unknown option: " + k);
    }
    if (mode != "par" && mode != "seq") throw std::invalid_argument("mode must be seq or par");
    if (mode == "seq" && threads != 1) throw std::invalid_argument("seq requires --threads 1");
    if (gen != "mcg59" && gen != "sobol") throw std::invalid_argument("unknown generator");
    if (gen == "sobol" && seen.count("--seed")) throw std::invalid_argument("Sobol uses no seed");
    omp_set_dynamic(0);
    if (mode == "par" && threads > omp_get_thread_limit()) throw std::invalid_argument("requested threads exceed OMP_THREAD_LIMIT");
    const int workers = mode == "par" ? threads : 0;
    const double ref = task == "pi" ? std::acos(-1.0) : lab::black_scholes(market);
    const auto generator = gen == "mcg59" ? lab::Generator::mcg59 : lab::Generator::sobol;
    std::cout << std::setprecision(17)
              << "task,generator,mode,n,threads,seed,s0,k,r,sigma,t,run,hits,value,reference,abs_error,rel_error,generation_s,kernel_s,total_s\n";
    for (int i = -warmup; i < repeat; ++i) {
        const auto out = task == "pi" ? lab::pi(n, seed, workers) : lab::call(n, generator, seed, market, workers);
        if (out.threads != threads) throw std::runtime_error("OpenMP did not supply the requested team");
        std::cout << task << ',' << (task == "pi" ? "splitmix64" : gen) << ',' << mode << ','
                  << n << ',' << out.threads << ',';
        if (gen != "sobol") std::cout << seed;
        std::cout << ',';
        if (task == "call") {
            std::cout << market.spot << ',' << market.strike << ',' << market.rate << ','
                      << market.sigma << ',' << market.maturity;
        } else std::cout << ",,,,";
        // Negative run numbers identify warmups; measured runs start at 1.
        std::cout << ',' << (i < 0 ? i : i+1) << ',';
        if (task == "pi") std::cout << out.hits;
        const double error = std::abs(out.value-ref);
        std::cout << ',' << out.value << ',' << ref << ',' << error << ',';
        if (ref != 0) {
            const double relative = error / std::abs(ref);
            if (!std::isfinite(relative)) throw std::range_error("relative error exceeds double range");
            std::cout << relative;
        }
        std::cout << ',' << out.generation << ',' << out.kernel << ',' << out.total << '\n';
    }
}
}
int main(int argc, char** argv) {
    try { run(argc, argv); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
