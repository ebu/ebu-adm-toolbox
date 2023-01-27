// from libadm, license: Apache 2

#include "ostream_operators.hpp"

#include <adm/elements/gain.hpp>
#include <adm/elements/label.hpp>
#include <adm/elements/time.hpp>
#include <chrono>
#include <iostream>

namespace adm {
std::ostream &operator<<(std::ostream &stream, const FractionalTime &time) {
  return stream << "FractionalTime{" << time.numerator() << ", " << time.denominator() << "}";
}

std::ostream &operator<<(std::ostream &stream, const std::chrono::nanoseconds &time) {
  return stream << "nanoseconds{" << time.count() << "}";
}

std::ostream &operator<<(std::ostream &stream, const Time &time) {
  return boost::apply_visitor([&](const auto &t) -> std::ostream & { return stream << t; }, time.asVariant());
}

std::ostream &operator<<(std::ostream &os, const Label &label) {
  label.print(os);
  return os;
}

std::ostream &operator<<(std::ostream &stream, const Gain &gain) {
  if (gain.isDb()) {
    stream << gain.asDb() << " dB";
  } else {
    stream << gain.asLinear();
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const InterpolationLength &interpolationLength) {
  return stream << interpolationLength.get();
}

}  // namespace adm
