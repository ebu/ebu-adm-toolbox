#pragma once
#include <adm/elements/time.hpp>
#include <cmath>
#include <numeric>

namespace eat {
namespace admx {

inline adm::FractionalTime add_with_same_denominators(adm::FractionalTime const &lhs, adm::FractionalTime const &rhs) {
  return adm::FractionalTime{lhs.numerator() + rhs.numerator(), lhs.denominator()};
}

inline adm::FractionalTime add_with_different_denominators(adm::FractionalTime const &lhs,
                                                           adm::FractionalTime const &rhs) {
  auto const lhs_n = lhs.normalised();
  auto const rhs_n = rhs.normalised();
  auto const denominator = std::lcm(lhs_n.denominator(), rhs_n.denominator());
  auto const l_numerator = lhs_n.numerator() * denominator / lhs_n.denominator();
  auto const r_numerator = rhs_n.numerator() * denominator / rhs_n.denominator();
  return adm::FractionalTime{l_numerator + r_numerator, denominator}.normalised();
}

inline adm::FractionalTime plus(adm::FractionalTime const &lhs, adm::FractionalTime const &rhs) {
  if (lhs.denominator() == rhs.denominator()) {
    return add_with_same_denominators(lhs, rhs);
  } else {
    return add_with_different_denominators(lhs, rhs);
  }
}

inline adm::FractionalTime negate(adm::FractionalTime const &time) { return {-time.numerator(), time.denominator()}; }

inline adm::FractionalTime minus(adm::FractionalTime const &lhs, adm::FractionalTime const &rhs) {
  return plus(lhs, negate(rhs));
}

template <typename Rep, typename Period>
adm::FractionalTime roundToFractional(std::chrono::duration<Rep, Period> rhs, int64_t target_denominator) {
  auto multiplier = static_cast<double>(target_denominator) / Period::den;
  auto numerator = std::llround(multiplier * rhs.count());
  return adm::FractionalTime{numerator, target_denominator};
}

// Adds two times, preserving denominator if first is fractional and second is not
inline adm::Time plus(adm::Time const &first, adm::Time const &second) {
  if (first.isNanoseconds()) {
    return {first.asNanoseconds() + second.asNanoseconds()};
  } else if (second.isNanoseconds()) {
    return plus(first.asFractional(), roundToFractional(second.asNanoseconds(), first.asFractional().denominator()));
  } else {
    return plus(first.asFractional(), second.asFractional());
  }
}

// Subtracts second time from first, preserving denominator if first is fractional and second is not
inline adm::Time minus(adm::Time const &first, adm::Time const &second) {
  if (first.isNanoseconds()) {
    return first.asNanoseconds() - second.asNanoseconds();
  } else {
    if (second.isNanoseconds()) {
      return minus(first.asFractional(), roundToFractional(second.asNanoseconds(), first.asFractional().denominator()));
    } else {
      return minus(first.asFractional(), second.asFractional());
    }
  }
}

}  // namespace admx
}  // namespace eat
