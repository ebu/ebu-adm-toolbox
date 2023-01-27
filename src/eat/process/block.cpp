#include "eat/process/block.hpp"

#include <bw64/bw64.hpp>

#include "eat/framework/process.hpp"
#include "eat/process/temp_dir.hpp"

using namespace eat::framework;
using namespace eat::process;

// audio buffering implementation
namespace {

/// a temporary file which will be removed when this is destructed
class TempFile {
 public:
  explicit TempFile(const std::string &extension) : path_(dir.get_temp_file(extension).string()) {}

  const std::string &path() const { return path_; }

  virtual ~TempFile() { std::filesystem::remove(std::filesystem::path{path()}); }

 private:
  TempDir dir;
  std::string path_;
};

using TempFilePtr = std::shared_ptr<const TempFile>;

/// write samples to a temporary wav file and return the path
class TempWavWriter : public StreamingAtomicProcess {
 public:
  TempWavWriter(const std::string &name)
      : StreamingAtomicProcess(name),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in")),
        out_path(add_out_port<DataPort<TempFilePtr>>("out")) {}

  void initialise() override { file_path = std::make_shared<TempFile>("wav"); }

  void process() override {
    while (in_samples->available()) {
      auto samples = in_samples->pop().read();
      auto &frame_info = samples->info();

      if (!file) {
        file = bw64::writeFile(file_path->path(), frame_info.channel_count, frame_info.sample_rate, 24);
      }

      file->write(samples->data(), frame_info.sample_count);
    }
  }

  void finalise() override {
    // make sure that in_path is consumed
    if (!file) {
      // if we write zero samples, the number of channels and FS do not matter
      // as the reader will produce no blocks
      file = bw64::writeFile(file_path->path(), 0, 48000, 24);
    }

    file.reset();
    out_path->set_value(std::move(file_path));
  }

 private:
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  DataPortPtr<TempFilePtr> out_path;

  TempFilePtr file_path;
  std::shared_ptr<bw64::Bw64Writer> file;
};

/// read samples from a temporary wav file given a path
class TempWavReader : public StreamingAtomicProcess {
 public:
  TempWavReader(const std::string &name, size_t block_size_)
      : StreamingAtomicProcess(name),
        block_size(block_size_),
        in_path(add_in_port<DataPort<TempFilePtr>>("in")),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out")) {
    always_assert(block_size > 0, "block size must be > 0");
  }

  void initialise() override {
    file_path = std::move(in_path->get_value());
    file = bw64::readFile(file_path->path());
  }

  void process() override {
    std::vector<float> buffer(block_size * file->channels());
    size_t n_frames = file->read(buffer.data(), block_size);

    if (n_frames > 0) {
      buffer.resize(n_frames * file->channels());
      auto samples = std::make_shared<InterleavedSampleBlock>(
          std::move(buffer), BlockDescription{n_frames, file->channels(), file->sampleRate()});
      out_samples->push(std::move(samples));
    } else
      out_samples->close();
  }

  void finalise() override {
    file.reset();
    file_path.reset();
  }

  std::optional<float> get_progress() override {
    if (file && file->numberOfFrames())
      return static_cast<float>(file->tell()) / static_cast<float>(file->numberOfFrames());
    else
      return std::nullopt;
  }

 private:
  size_t block_size;
  DataPortPtr<TempFilePtr> in_path;
  StreamPortPtr<InterleavedBlockPtr> out_samples;

  TempFilePtr file_path;
  std::shared_ptr<bw64::Bw64Reader> file;
};

}  // namespace

namespace eat::framework {

template <>
ProcessPtr MakeBuffer<InterleavedBlockPtr>::get_buffer_reader(const std::string &name) {
  return std::make_shared<TempWavReader>(name, 1024);
}

template <>
ProcessPtr MakeBuffer<InterleavedBlockPtr>::get_buffer_writer(const std::string &name) {
  return std::make_shared<TempWavWriter>(name);
}

}  // namespace eat::framework
