#pragma once
#include <iosfwd>

#include "eat/framework/process.hpp"

namespace eat::utilities {

/// print g in graphviz format for debugging or documentation
///
/// this can be turned into a png by piping it through dot like this:
///
///   ./example | dot -Tpng -o out.png
///
/// or writing it to a .gv file and using:
///
///   dot -Tpng -o out.png in.gv
void graph_to_dot(std::ostream &s, const framework::Graph &g, bool recursive = true);

}  // namespace eat::utilities
