#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace eat::process {

class TempDirImpl;

/// a uniquely-named temporary directory in which temporary files can be created
///
/// currently this will be cleaned up at program exit, however it may be
/// changed to clean up once all instances have gone out of scope, so keep a
/// reference to this while you're using it
class TempDir {
 public:
  TempDir();
  std::filesystem::path get_temp_file(const std::string &extension);

 private:
  std::shared_ptr<TempDirImpl> impl;
};

}  // namespace eat::process
