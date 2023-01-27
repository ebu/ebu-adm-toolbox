#include "eat/process/adm_bw64.hpp"

#include <adm/common_definitions.hpp>
#include <adm/document.hpp>
#include <adm/parse.hpp>
#include <adm/write.hpp>
#include <bw64/bw64.hpp>
#include <catch2/catch_test_macros.hpp>

#include "eat/framework/evaluate.hpp"
#include "eat/process/block.hpp"
#include "eat/testing/files.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::testing;
using namespace adm;

TEST_CASE("read and write") {
  TempDir dir;
  auto in = dir / "in.wav";
  auto out = dir / "out.wav";
  size_t channels = 2;
  size_t sample_rate = 48000;

  // write an ADM BW64 file using libadm and libbw64 directly
  {
    auto file = bw64::writeFile(in.string(), channels, sample_rate, 24);

    float scale = 0.1f;
    std::vector<float> samples = {1.0f * scale, 2.0f * scale, 3.0f * scale, 4.0f * scale};
    file->write(samples.data(), 2);

    auto document = adm::getCommonDefinitions();

    {  // stereo object
      std::vector<std::string> track_uids = {"ATU_00000001", "ATU_00000002"};
      std::vector<std::string> tracks = {"AT_00010001_01", "AT_00010002_01"};
      std::string pack = "AP_00010002";

      auto object = AudioObject::create(AudioObjectName("object 1"));
      document->add(object);

      for (size_t i = 0; i < 2; i++) {
        auto track = AudioTrackUid::create(parseAudioTrackUidId(track_uids[i]));
        track->setReference(document->lookup(parseAudioTrackFormatId(tracks[i])));
        track->setReference(document->lookup(parseAudioPackFormatId(pack)));
        document->add(track);
        object->addReference(track);
      }
      object->addReference(document->lookup(parseAudioPackFormatId(pack)));
    }

    {  // silent mono object
      auto object = AudioObject::create(AudioObjectName("object 2"));
      document->add(object);
      object->addReference(AudioTrackUid::getSilent(document));
      object->addReference(document->lookup(parseAudioPackFormatId("AP_00010001")));
    }

    std::ostringstream stream;
    adm::writeXml(stream, document);

    auto axml_chunk = std::make_shared<bw64::AxmlChunk>(stream.str());
    file->setAxmlChunk(std::move(axml_chunk));

    auto chna = std::make_shared<bw64::ChnaChunk>();
    chna->addAudioId({1, "ATU_00000001", "AT_00010001_01", "AP_00010002"});
    chna->addAudioId({2, "ATU_00000002", "AT_00010002_01", "AP_00010002"});
    file->setChnaChunk(chna);
  }

  // use a processing graph to copy it
  {
    Graph g;
    auto reader = g.register_process(make_read_adm_bw64("reader", in.string(), 1024));
    auto writer = g.register_process(make_write_adm_bw64("writer", in.string()));

    g.connect(reader->get_out_port("out_samples"), writer->get_in_port("in_samples"));
    g.connect(reader->get_out_port("out_axml"), writer->get_in_port("in_axml"));

    Plan p = plan(g);
    p.run();
  }

  REQUIRE(files_equal(in.string(), out.string()));
}

template <typename T>
bool reader_specialised() {
  auto reader = MakeBuffer<T>::get_buffer_reader("reader");
  return !std::dynamic_pointer_cast<eat::framework::detail::InMemBufferRead<T>>(reader);
}

template <typename T>
bool writer_specialised() {
  auto writer = MakeBuffer<T>::get_buffer_writer("writer");
  return !std::dynamic_pointer_cast<eat::framework::detail::InMemBufferWrite<T>>(writer);
}

TEST_CASE("check MakeBuffer for block") {
  // check that buffer readers and writers are specialised for InterleavedBlockPtr
  REQUIRE(reader_specialised<InterleavedBlockPtr>());
  REQUIRE(writer_specialised<InterleavedBlockPtr>());

  // check that the above test actually works
  REQUIRE(!reader_specialised<int>());
  REQUIRE(!writer_specialised<int>());
}
