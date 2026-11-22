#pragma once
#include <cstdint>

namespace lab {
using Word = std::uint64_t;
constexpr Word mcg_modulus = Word{1} << 59;
constexpr Word max_samples = (Word{1} << 53) - 1;
constexpr Word block_size = Word{1} << 18;

enum class Generator { mcg59, sobol };

class Mcg59 {
    Word state_;
public:
    explicit Mcg59(Word seed);
    Word next_word();
    double next();
};

class Sobol1D {
    Word index_ = 0, bits_ = 0;
public:
    double next(); // first nonzero point
};

double inv_normal(double u);
struct Market {
    double spot = 100, strike = 100, rate = 0.05, sigma = 0.2, maturity = 1;
};
double black_scholes(const Market& market);

// workers=0 selects the ordinary serial loop; workers>=1 selects OpenMP.
struct Result {
    Word hits = 0;
    double value = 0, generation = 0, kernel = 0, total = 0;
    int threads = 1;
};
Result pi(Word n, Word seed, int workers);
Result call(Word n, Generator generator, Word seed, const Market& market, int workers);
}
