#pragma once
#include <adm/elements/time.hpp>
#include <adm/utilities/time_conversion.hpp>

namespace eat::process {

/// convert t to a Time, while trying to match the type (and denominator for
/// FractionalTime) as to_match
///
/// the result is always exact, so may be a FractionalTime even if to_match is
/// in nanoseconds
inline adm::Time try_match_time_type(adm::Time to_match, adm::RationalTime t) {
  if (to_match.isNanoseconds()) {
    adm::RationalTime t_ns = t * 1000000000;

    if (t_ns.denominator() == 1)
      return std::chrono::nanoseconds{t_ns.numerator()};
    else
      return adm::asFractionalTime(t);
  } else {
    auto denominator = to_match.asFractional().denominator();
    adm::RationalTime t_num = t * denominator;

    if (t_num.denominator() == 1)
      return adm::FractionalTime{t_num.numerator(), denominator};
    else
      return adm::asFractionalTime(t);
  }
}

/// return a - b, trying to keep the same type (and denominator) as a
inline adm::Time time_sub(adm::Time a, adm::Time b) {
  return try_match_time_type(a, adm::asRational(a) - adm::asRational(b));
}

/// return b - a, trying to keep the same type (and denominator) as a
inline adm::Time time_rsub(adm::Time a, adm::Time b) {
  return try_match_time_type(a, adm::asRational(b) - adm::asRational(a));
}

/// return a + b, trying to keep the same type (and denominator) as a
inline adm::Time time_add(adm::Time a, adm::Time b) {
  return try_match_time_type(a, adm::asRational(a) + adm::asRational(b));
}

}  // namespace eat::process
