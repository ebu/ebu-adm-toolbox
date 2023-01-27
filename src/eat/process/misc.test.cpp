#include "eat/process/misc.hpp"

#include <adm/document.hpp>
#include <adm/utilities/object_creation.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../utilities/ostream_operators.hpp"
#include "eat/framework/evaluate.hpp"
#include "eat/framework/process.hpp"
#include "eat/framework/utility_processes.hpp"
#include "eat/process/adm_bw64.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace adm;
using namespace std::chrono;

namespace {
/// holds a graph and a process to test, and makes feeding input data and getting output data a little simpler
struct Harness {
 public:
  Harness(ProcessPtr test_process_) : test_process(std::move(test_process_)) { g.register_process(test_process); }

  template <typename T>
  void input_data(const std::string &name, T value) {
    auto in = g.add_process<DataSource<T>>(name, std::move(value));

    g.connect(in->get_out_port("out"), test_process->get_in_port(name));
  }

  template <typename T>
  std::shared_ptr<DataSink<T>> add_output(const std::string &name) {
    auto in = g.add_process<DataSink<T>>(name);
    g.connect(test_process->get_out_port(name), in->get_in_port("in"));
    return in;
  }

  void run() { evaluate(g); }

  Graph g;
  ProcessPtr test_process;
};

}  // namespace

TEST_CASE("fix_ds_frequency") {
  auto acf_lfe = AudioChannelFormat::create(AudioChannelFormatName{"LFE"}, TypeDefinition::DIRECT_SPEAKERS);
  auto acf_not_lfe =
      AudioChannelFormat::create(AudioChannelFormatName{"something else"}, TypeDefinition::DIRECT_SPEAKERS);

  auto document = Document::create();
  document->add(acf_lfe);
  document->add(acf_not_lfe);

  Harness h(make_fix_ds_frequency("fix"));
  h.input_data("in_axml", ADMData{std::move(document), {}});
  auto out = h.add_output<ADMData>("out_axml");
  h.run();

  ADMData adm_out = std::move(out->get_value());
  auto doc_out = adm_out.document.read();

  auto out_acf_lfe = doc_out->lookup(acf_lfe->get<AudioChannelFormatId>());
  REQUIRE(out_acf_lfe);
  REQUIRE(out_acf_lfe->has<Frequency>());
  REQUIRE(out_acf_lfe->get<Frequency>().has<LowPass>());
  CHECK(out_acf_lfe->get<Frequency>().get<LowPass>() == 120.0f);

  auto out_acf_not_lfe = doc_out->lookup(acf_not_lfe->get<AudioChannelFormatId>());
  REQUIRE(out_acf_not_lfe);
  CHECK(!out_acf_not_lfe->has<Frequency>());
}

TEST_CASE("fix_block_durations") {
  auto document = Document::create();

  // a full object structure is required for the channel to get picked up, and
  // to specify the full programme length
  auto ap = AudioProgramme::create(AudioProgrammeName{"ap"}, Start{milliseconds{0}}, End{milliseconds{2000}});
  document->add(ap);

  auto ac = AudioContent::create(AudioContentName{"ac"});
  ap->addReference(ac);
  document->add(ac);

  auto obj = addSimpleObjectTo(document, "obj");
  ac->addReference(obj.audioObject);

  auto acf = obj.audioChannelFormat;

  acf->add(AudioBlockFormatObjects{SphericalPosition{}, Rtime{milliseconds{0}}, Duration{milliseconds{999}}});
  acf->add(AudioBlockFormatObjects{SphericalPosition{}, Rtime{milliseconds{1000}}, Duration{milliseconds{1000}}});

  Harness h(make_fix_block_durations("fix"));
  h.input_data("in_axml", ADMData{std::move(document), {}});
  auto out = h.add_output<ADMData>("out_axml");
  h.run();

  ADMData adm_out = std::move(out->get_value());
  auto doc_out = adm_out.document.read();

  auto out_acf = doc_out->lookup(acf->get<AudioChannelFormatId>());
  REQUIRE(out_acf);
  auto blocks = out_acf->getElements<AudioBlockFormatObjects>();
  REQUIRE(blocks.size() == 2);

  REQUIRE(blocks[0].has<Rtime>());
  CHECK(blocks[0].get<Rtime>() == milliseconds{0});

  REQUIRE(blocks[0].has<Duration>());
  CHECK(blocks[0].get<Duration>() == milliseconds{1000});

  REQUIRE(blocks[1].has<Rtime>());
  CHECK(blocks[1].get<Rtime>() == milliseconds{1000});

  REQUIRE(blocks[1].has<Duration>());
  CHECK(blocks[1].get<Duration>() == milliseconds{1000});
}

TEST_CASE("convert_track_stream_to_channel") {
  auto document = Document::create();

  auto obj = addSimpleObjectTo(document, "obj");

  // just in case libadm changes to make audioTrackUid->audioChannelFormat references
  REQUIRE(obj.audioTrackUid->getReference<adm::AudioTrackFormat>());

  Harness h(make_convert_track_stream_to_channel("convert"));
  h.input_data("in_axml", ADMData{std::move(document), {}});
  auto out = h.add_output<ADMData>("out_axml");
  h.run();

  ADMData adm_out = std::move(out->get_value());
  auto doc_out = adm_out.document.read();

  auto out_acf = doc_out->lookup(obj.audioChannelFormat->get<AudioChannelFormatId>());
  auto out_atu = doc_out->lookup(obj.audioTrackUid->get<AudioTrackUidId>());

  REQUIRE(!out_atu->getReference<adm::AudioTrackFormat>());
  REQUIRE(out_atu->getReference<adm::AudioChannelFormat>() == out_acf);
}
