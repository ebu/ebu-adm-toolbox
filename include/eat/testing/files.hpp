#pragma once

#include <algorithm>
#include <catch2/internal/catch_context.hpp>
#include <filesystem>
#include <fstream>

namespace eat::testing {

/// are the contents of two files equal?
inline bool files_equal(const std::string &fname_a, const std::string &fname_b) {
  std::fstream file_a(fname_a, std::ios_base::in | std::ios_base::binary);
  std::fstream file_b(fname_a, std::ios_base::in | std::ios_base::binary);

  if (!file_a.is_open()) throw std::runtime_error("could not open " + fname_a);
  if (!file_b.is_open()) throw std::runtime_error("could not open " + fname_b);

  std::istreambuf_iterator<char> it_a(file_a), it_b(file_b), end;

  return std::equal(it_a, end, it_b, end);
}

namespace detail {

/// get the current catch test name
///
/// this is supposed to be internal to catch, but hasn't changed in a long time
inline std::string current_test_name() { return Catch::getResultCapture().getCurrentTestName(); }

}  // namespace detail

/// a temporary directory for use while testing
///
/// currently `tmpdir / "file.wav"` when running a test called `test_name`
/// returns test_tmp/test_name/file.wav, and these files are not deleted
class TempDir {
 public:
  TempDir() : path(std::filesystem::path{"test_tmp"} / detail::current_test_name()) {
    std::filesystem::create_directories(path);
  }

  /// get a file in the directory
  std::filesystem::path operator/(const std::string &fname) { return path / fname; }

 private:
  std::filesystem::path path;
};

};  // namespace eat::testing
