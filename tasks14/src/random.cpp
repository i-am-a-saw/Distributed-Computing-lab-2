#include "lab.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lab {
Mcg59::Mcg59(Word seed) : state_(seed) {
    if (seed == 0 || seed >= mcg_modulus || seed % 2 == 0)
        throw std::invalid_argument("MCG59 needs an odd seed in [1, 2^59)");
}

Word Mcg59::next_word() {
    // Unsigned multiplication is defined modulo 2^64. Its low 59 bits
    // equal the low 59 bits of the exact (up to 108-bit) product.
    constexpr Word a = 302875106592253ULL; // 13^13
    state_ = (a * state_) & (mcg_modulus - 1);
    return state_;
}

double Mcg59::next() {
    return std::ldexp(static_cast<double>(next_word()), -59);
}

double Sobol1D::next() {
    // g(i)=i^(i>>1) changes bit ctz(i) from g(i-1). The corresponding
    // direction number in dimension 1 is 1/2^(ctz(i)+1).
    ++index_;
    if (index_ == 0) throw std::overflow_error("Sobol period exhausted");
    bits_ ^= Word{1} << (63 - __builtin_ctzll(index_));
    return std::ldexp(static_cast<double>(bits_), -64);
}

double inv_normal(double u) {
    if (!(u >= 0 && u <= 1)) throw std::domain_error("u must be in [0, 1]");
    u = std::clamp(u, std::nextafter(0.0, 1.0), std::nextafter(1.0, 0.0));
    // P. J. Acklam's three-region rational approximation to Phi^{-1}.
    constexpr double a[] = {-39.69683028665376, 220.9460984245205, -275.9285104469687,
        138.3577518672690, -30.66479806614716, 2.506628277459239};
    constexpr double b[] = {-54.47609879822406, 161.5858368580409, -155.6989798598866,
        66.80131188771972, -13.28068155288572};
    constexpr double c[] = {-0.007784894002430293, -0.3223964580411365, -2.400758277161838,
        -2.549732539343734, 4.374664141464968, 2.938163982698783};
    constexpr double d[] = {0.007784695709041462, 0.3224671290700398,
        2.445134137142996, 3.754408661907416};
    constexpr double split = 0.02425;
    if (u < split || u > 1 - split) {
        const double q = std::sqrt(-2 * (u < split ? std::log(u) : std::log1p(-u)));
        const double num = (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]);
        const double den = ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
        return (u < split ? num : -num) / den;
    }
    const double q = u - 0.5;
    const double r = q*q;
    const double num = (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q;
    const double den = (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
    return num / den;
}
}
