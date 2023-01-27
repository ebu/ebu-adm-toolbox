#pragma once
#include <string>

#define EAT_STRINGIFY_HELPER(x) #x
#define EAT_STRINGIFY(x) EAT_STRINGIFY_HELPER(x)

namespace eat::utilities {

/// get the path to the source dir
/// only available in tests
inline std::string src_dir() {
  return EAT_STRINGIFY(EAT_SRC_DIR);  // defined by cmake
}

/// get a path to a file within test_files
/// path should have no leading /
/// only available in tests
inline std::string test_file_path(const std::string &path) { return src_dir() + "/test_files/" + path; }

}  // namespace eat::utilities

#undef EAT_STRINGIFY_HELPER
#undef EAT_STRINGIFY
