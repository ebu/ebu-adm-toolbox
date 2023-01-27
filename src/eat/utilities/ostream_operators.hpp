// from libadm, license: Apache 2

#pragma once
#include <adm/elements/jump_position.hpp>
#include <iosfwd>

namespace adm {
class FractionalTime;
class Gain;
class Label;
class Time;

std::ostream &operator<<(std::ostream &stream, const FractionalTime &time);

// required because catch2 stringify doesn't know about boost::variants
std::ostream &operator<<(std::ostream &stream, const std::chrono::nanoseconds &time);

std::ostream &operator<<(std::ostream &stream, const Time &time);

std::ostream &operator<<(std::ostream &stream, const InterpolationLength &interpolationLength);

std::ostream &operator<<(std::ostream &stream, const Gain &gain);

std::ostream &operator<<(std::ostream &os, const Label &label);
}  // namespace adm
