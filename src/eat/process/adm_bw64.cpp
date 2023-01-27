#include "eat/process/adm_bw64.hpp"

#include <adm/document.hpp>
#include <adm/parse.hpp>
#include <adm/write.hpp>
#include <bw64/bw64.hpp>

#include "eat/framework/exceptions.hpp"
#include "eat/process/block.hpp"
#include "eat/process/chna.hpp"

using namespace eat::framework;
using namespace eat::process;

namespace {

class ADMReader : public FunctionalAtomicProcess {
 public:
  ADMReader(const std::string &name, const std::string &path_)
      : FunctionalAtomicProcess(name), path(path_), out_axml(add_out_port<DataPort<ADMData>>("out_axml")) {}

  void process() override {
    auto file = bw64::readFile(path);
    std::stringstream axml;
    file->axmlChunk()->write(axml);
    axml.seekg(0);

    ADMData adm;

    auto doc = adm::parseXml(axml);
    load_chna(*doc, adm.channel_map, *file->chnaChunk());
    adm.document = std::move(doc);

    out_axml->set_value(std::move(adm));
  }

 private:
  std::string path;
  DataPortPtr<ADMData> out_axml;
};

class AudioReader : public StreamingAtomicProcess {
 public:
  AudioReader(const std::string &name, const std::string &path_, size_t block_size_)
      : StreamingAtomicProcess(name),
        path(path_),
        block_size(block_size_),
        out_samples(add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples")) {
    always_assert(block_size > 0, "block size must be > 0");
  }

  void initialise() override { file = bw64::readFile(path); }

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

  void finalise() override { file.reset(); }

  std::optional<float> get_progress() override {
    if (file && file->numberOfFrames())
      return static_cast<float>(file->tell()) / static_cast<float>(file->numberOfFrames());
    else
      return std::nullopt;
  }

 private:
  std::string path;
  size_t block_size;
  StreamPortPtr<InterleavedBlockPtr> out_samples;

  std::shared_ptr<bw64::Bw64Reader> file;
};

class AudioWriter : public StreamingAtomicProcess {
 public:
  AudioWriter(const std::string &name, const std::string &path_, bool has_out_file)
      : StreamingAtomicProcess(name),
        path(path_),
        in_samples(add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples")),
        out_file(has_out_file ? add_out_port<DataPort<std::shared_ptr<bw64::Bw64Writer>>>("out_file") : nullptr) {}

  void process() override {
    while (in_samples->available()) {
      auto samples = in_samples->pop().read();
      auto &frame_info = samples->info();

      if (!file) {
        file = bw64::writeFile(path, frame_info.channel_count, frame_info.sample_rate, 24);
      }

      file->write(samples->data(), frame_info.sample_count);
    }
  }

  void finalise() override {
    if (!file) {
      // TODO: issue warning that we had to guess number of channels and sample rate
      file = bw64::writeFile(path, 0, 48000, 24);
    }
    if (out_file)
      out_file->set_value(std::move(file));
    else
      file = nullptr;
  }

 private:
  std::string path;
  StreamPortPtr<InterleavedBlockPtr> in_samples;
  DataPortPtr<std::shared_ptr<bw64::Bw64Writer>> out_file;

  std::shared_ptr<bw64::Bw64Writer> file;
};

class ADMWriter : public FunctionalAtomicProcess {
 public:
  ADMWriter(const std::string &name)
      : FunctionalAtomicProcess(name),
        in_file(add_in_port<DataPort<std::shared_ptr<bw64::Bw64Writer>>>("in_file")),
        in_axml(add_in_port<DataPort<ADMData>>("in_axml")) {}

  void process() override {
    auto file = std::move(in_file->get_value());

    auto adm = std::move(in_axml->get_value());

    std::ostringstream axml_stream;
    adm::writeXml(axml_stream, adm.document.read());

    auto axml_chunk = std::make_shared<bw64::AxmlChunk>(axml_stream.str());
    file->setAxmlChunk(std::move(axml_chunk));

    file->setChnaChunk(std::make_shared<bw64::ChnaChunk>(make_chna(*adm.document.read(), adm.channel_map)));
  }

 private:
  DataPortPtr<std::shared_ptr<bw64::Bw64Writer>> in_file;
  DataPortPtr<ADMData> in_axml;
};

class ADMWavWriter : public CompositeProcess {
 public:
  ADMWavWriter(const std::string &name, const std::string &path) : CompositeProcess(name) {
    auto in_axml = add_in_port<DataPort<ADMData>>("in_axml");
    auto in_samples = add_in_port<StreamPort<InterleavedBlockPtr>>("in_samples");

    auto audio_writer = add_process<AudioWriter>("audio writer", path, true);
    auto adm_writer = add_process<ADMWriter>("adm writer");

    connect(in_samples, audio_writer->get_in_port("in_samples"));

    connect(audio_writer->get_out_port("out_file"), adm_writer->get_in_port("in_file"));
    connect(in_axml, adm_writer->get_in_port("in_axml"));
  }
};

class ADMWavReader : public CompositeProcess {
 public:
  ADMWavReader(const std::string &name, const std::string &path, size_t block_size) : CompositeProcess(name) {
    auto out_axml = add_out_port<DataPort<ADMData>>("out_axml");
    auto out_samples = add_out_port<StreamPort<InterleavedBlockPtr>>("out_samples");

    auto adm_reader = add_process<ADMReader>("adm reader", path);
    auto audio_reader = add_process<AudioReader>("audio reader", path, block_size);

    connect(audio_reader->get_out_port("out_samples"), out_samples);
    connect(adm_reader->get_out_port("out_axml"), out_axml);
  }
};

}  // namespace

namespace eat::process {

ProcessPtr make_read_bw64(const std::string &name, const std::string &path, size_t block_size) {
  return std::make_shared<AudioReader>(name, path, block_size);
}

ProcessPtr make_write_bw64(const std::string &name, const std::string &path) {
  return std::make_shared<AudioWriter>(name, path, false);
}

ProcessPtr make_read_adm(const std::string &name, const std::string &path) {
  return std::make_shared<ADMReader>(name, path);
}

ProcessPtr make_read_adm_bw64(const std::string &name, const std::string &path, size_t block_size) {
  return std::make_shared<ADMWavReader>(name, path, block_size);
}

ProcessPtr make_write_adm_bw64(const std::string &name, const std::string &path) {
  return std::make_shared<ADMWavWriter>(name, path);
}

}  // namespace eat::process

namespace eat::framework {

template <>
std::shared_ptr<adm::Document> copy_shared_ptr<adm::Document>(const std::shared_ptr<adm::Document> &value) {
  return value->deepCopy();
}

}  // namespace eat::framework
