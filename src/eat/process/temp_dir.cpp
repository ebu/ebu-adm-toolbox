#include "eat/process/temp_dir.hpp"

#include <random>

#include "eat/framework/exceptions.hpp"

using namespace eat::framework;

namespace eat::process {

class TempDirImpl {
 public:
  TempDirImpl() {
    auto base_path = std::filesystem::temp_directory_path();

    std::random_device rd;
    random_engine = std::default_random_engine{rd()};
    std::uniform_int_distribution<int> dist(0, 100000);

    while (true) {
      int random = dist(random_engine);
      path = base_path / ("eat." + std::to_string(random));

      // returns true if this creates a directory
      if (std::filesystem::create_directory(path)) break;
    }
  }

  ~TempDirImpl() {
    auto n_removed = std::filesystem::remove_all(path);
    always_assert(n_removed > 0, "temp directory disappeared");
  }

  std::filesystem::path get_temp_file(const std::string &extension) {
    std::uniform_int_distribution<int> dist(0, 100000);
    while (true) {
      int random = dist(random_engine);
      // there's no clean way to make sure this is actually unique
      auto file_path = path / (std::to_string(random) + "." + extension);
      if (!std::filesystem::exists(file_path)) return file_path;
    }
  }

 private:
  std::default_random_engine random_engine;
  std::filesystem::path path;
};

TempDir::TempDir() {
  static std::shared_ptr<TempDirImpl> instance = {};

  if (!instance) instance = std::make_shared<TempDirImpl>();

  impl = instance;
}

std::filesystem::path TempDir::get_temp_file(const std::string &extension) { return impl->get_temp_file(extension); }

}  // namespace eat::process
