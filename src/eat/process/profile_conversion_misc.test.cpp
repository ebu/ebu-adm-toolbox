#include "eat/process/profile_conversion_misc.hpp"

#include <adm/common_definitions.hpp>
#include <adm/document.hpp>
#include <adm/utilities/object_creation.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../utilities/ostream_operators.hpp"
#include "eat/framework/evaluate.hpp"
#include "eat/framework/process.hpp"
#include "eat/framework/utility_processes.hpp"
#include "eat/process/adm_bw64.hpp"
#include "eat/process/block.hpp"
#include "eat/render/rendering_items.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::render;
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

  void input_samples(const std::string &name, std::vector<float> samples, size_t n_channels, unsigned int fs = 48000) {
    BlockDescription description{samples.size() / n_channels, n_channels, fs};

    auto in = g.add_process<InterleavedStreamingAudioSource>(name, std::move(samples), description);

    g.connect(in->get_out_port("out_samples"), test_process->get_in_port(name));
  }

  template <typename T>
  std::shared_ptr<DataSink<T>> add_output(const std::string &name) {
    auto in = g.add_process<DataSink<T>>(name);
    g.connect(test_process->get_out_port(name), in->get_in_port("in"));
    return in;
  }

  std::shared_ptr<InterleavedStreamingAudioSink> add_output_samples(const std::string &name) {
    auto out = g.add_process<InterleavedStreamingAudioSink>(name);
    g.connect(test_process->get_out_port(name), out->get_in_port("in_samples"));
    return out;
  }

  void run() { evaluate(g); }

  Graph g;
  ProcessPtr test_process;
};

}  // namespace

TEST_CASE("set_profile") {
  auto document = Document::create();

  Harness h(make_set_profiles("set_profiles", {profiles::ITUEmissionProfile{2}}));
  h.input_data("in_axml", ADMData{std::move(document), {}});
  auto out = h.add_output<ADMData>("out_axml");
  h.run();

  ADMData adm_out = std::move(out->get_value());
  auto doc_out = adm_out.document.read();

  REQUIRE(doc_out->has<adm::ProfileList>());
  auto profile_list = doc_out->get<adm::ProfileList>();
  auto profiles = profile_list.get<adm::Profiles>();
  REQUIRE(profiles.size() == 1);
  auto profile = profiles.at(0);

  REQUIRE(profile.get<adm::ProfileValue>() == "ITU-R BS.[ADM-NGA-Emission]-0");
  REQUIRE(profile.get<adm::ProfileName>() == "AdvSS Emission ADM and S-ADM Profile");
  REQUIRE(profile.get<adm::ProfileVersion>() == "1");
  REQUIRE(profile.get<adm::ProfileLevel>() == "2");
}

template <typename Position, typename Coordinate, typename Block>
void check_coord(const Block &block, float value) {
  REQUIRE(block.template has<Position>());
  REQUIRE(block.template get<Position>().template has<Coordinate>());
  REQUIRE(block.template get<Position>().template get<Coordinate>() == value);
}

TEST_CASE("set_position_defaults") {
  auto document = Document::create();

  auto acf_obj = adm::AudioChannelFormat::create(adm::AudioChannelFormatName("obj"), adm::TypeDefinition::OBJECTS);
  acf_obj->add(adm::AudioBlockFormatObjects{adm::SphericalPosition{adm::Azimuth{1.0f}, adm::Elevation{2.0f}}});
  acf_obj->add(adm::AudioBlockFormatObjects{
      adm::SphericalPosition{adm::Azimuth{1.0f}, adm::Elevation{2.0f}, adm::Distance{3.0f}}});
  acf_obj->add(adm::AudioBlockFormatObjects{adm::CartesianPosition{adm::X{1.0f}, adm::Y{2.0f}}});
  acf_obj->add(adm::AudioBlockFormatObjects{adm::CartesianPosition{adm::X{1.0f}, adm::Y{2.0f}, adm::Z{3.0f}}});
  document->add(acf_obj);

  auto acf_ds =
      adm::AudioChannelFormat::create(adm::AudioChannelFormatName("ds"), adm::TypeDefinition::DIRECT_SPEAKERS);
  acf_ds->add(
      adm::AudioBlockFormatDirectSpeakers{adm::SphericalSpeakerPosition{adm::Azimuth{1.0f}, adm::Elevation{2.0f}}});
  acf_ds->add(adm::AudioBlockFormatDirectSpeakers{
      adm::SphericalSpeakerPosition{adm::Azimuth{1.0f}, adm::Elevation{2.0f}, adm::Distance{3.0f}}});
  acf_ds->add(adm::AudioBlockFormatDirectSpeakers{adm::CartesianSpeakerPosition{adm::X{1.0f}, adm::Y{2.0f}}});
  acf_ds->add(
      adm::AudioBlockFormatDirectSpeakers{adm::CartesianSpeakerPosition{adm::X{1.0f}, adm::Y{2.0f}, adm::Z{3.0f}}});
  document->add(acf_ds);

  Harness h(make_set_position_defaults("set_profiles"));
  h.input_data("in_axml", ADMData{std::move(document), {}});
  auto out = h.add_output<ADMData>("out_axml");
  h.run();

  ADMData adm_out = std::move(out->get_value());
  auto doc_out = adm_out.document.read();

  {
    auto acf_obj_out = doc_out->lookup(acf_obj->get<AudioChannelFormatId>());
    REQUIRE(acf_obj_out);
    auto blocks_obj_out = acf_obj_out->getElements<adm::AudioBlockFormatObjects>();
    REQUIRE(blocks_obj_out.size() == 4);

    check_coord<adm::SphericalPosition, adm::Distance>(blocks_obj_out[0], 1.0f);
    check_coord<adm::SphericalPosition, adm::Distance>(blocks_obj_out[1], 3.0f);
    check_coord<adm::CartesianPosition, adm::Z>(blocks_obj_out[2], 0.0f);
    check_coord<adm::CartesianPosition, adm::Z>(blocks_obj_out[3], 3.0f);
  }

  {
    auto acf_ds_out = doc_out->lookup(acf_ds->get<AudioChannelFormatId>());
    REQUIRE(acf_ds_out);
    auto blocks_ds_out = acf_ds_out->getElements<adm::AudioBlockFormatDirectSpeakers>();
    REQUIRE(blocks_ds_out.size() == 4);

    check_coord<adm::SphericalSpeakerPosition, adm::Distance>(blocks_ds_out[0], 1.0f);
    check_coord<adm::SphericalSpeakerPosition, adm::Distance>(blocks_ds_out[1], 3.0f);
    check_coord<adm::CartesianSpeakerPosition, adm::Z>(blocks_ds_out[2], 0.0f);
    check_coord<adm::CartesianSpeakerPosition, adm::Z>(blocks_ds_out[3], 3.0f);
  }
}

TEST_CASE("remove_silent_atu") {
  // construct an ADM with a stereo object with one real and one silent track
  auto document = adm::getCommonDefinitions();

  auto programme = AudioProgramme::create(AudioProgrammeName("programme"));
  document->add(programme);

  auto content = AudioContent::create(AudioContentName("content"));
  document->add(content);
  programme->addReference(content);

  auto object = AudioObject::create(AudioObjectName("object"));
  document->add(object);
  content->addReference(object);

  object->addReference(document->lookup(parseAudioPackFormatId("AP_00010002")));

  std::shared_ptr<AudioTrackUid> track;

  std::function<void(const std::shared_ptr<adm::AudioTrackUid> &track)> extra_checks;

  SECTION("common defs track format reference") {
    track = AudioTrackUid::create();
    track->setReference(document->lookup(parseAudioTrackFormatId("AT_00010001_01")));
    track->setReference(document->lookup(parseAudioPackFormatId("AP_00010002")));
    object->addReference(track);

    extra_checks = [](auto atu) {
      auto atf = atu->template getReference<AudioTrackFormat>();
      REQUIRE(atf);
      REQUIRE(atf->template get<AudioTrackFormatId>() == parseAudioTrackFormatId("AT_00010002_01"));
    };
  }

  SECTION("common defs channel format reference") {
    track = AudioTrackUid::create();
    track->setReference(document->lookup(parseAudioChannelFormatId("AC_00010001")));
    track->setReference(document->lookup(parseAudioPackFormatId("AP_00010002")));
    object->addReference(track);

    extra_checks = [](auto atu) {
      auto acf = atu->template getReference<AudioChannelFormat>();
      REQUIRE(acf);
      REQUIRE(acf->template get<AudioChannelFormatId>() == parseAudioChannelFormatId("AC_00010002"));
    };
  }

  SECTION("no common defs track") {
    track = AudioTrackUid::create();
    track->setReference(document->lookup(parseAudioTrackFormatId("AT_00010001_01")));
    track->setReference(document->lookup(parseAudioPackFormatId("AP_00010002")));
    object->addReference(track);
    // just delete the track that would be use; this is easier than making the
    // whole structure from scratch
    document->remove(document->lookup(parseAudioTrackFormatId("AT_00010002_01")));
    REQUIRE(!document->lookup(parseAudioTrackFormatId("AT_00010002_01")));

    extra_checks = [](auto atu) { REQUIRE(atu->template getReference<AudioTrackFormat>()); };
  }

  object->addReference(adm::AudioTrackUid::getSilent(document));

  // run the process with one channel of audio in
  Harness h(make_remove_silent_atu("remove_silent_atu"));

  h.input_data("in_axml", ADMData{std::move(document),
                                  {
                                      {track->get<AudioTrackUidId>(), 0},
                                  }});

  float scale = 0.1f;
  std::vector<float> samples_in = {1.0f * scale, 2.0f * scale, 3.0f * scale, 4.0f * scale};
  h.input_samples("in_samples", samples_in, 1, 48000);

  auto out_axml = h.add_output<ADMData>("out_axml");
  auto out_samples = h.add_output_samples("out_samples");
  h.run();

  ADMData adm_out = std::move(out_axml->get_value());
  auto doc_out = adm_out.document.move_or_copy();

  // check that the output samples are the right shape and values (one silent
  // channel added)
  auto out_block = out_samples->get_block();
  REQUIRE(out_block.info().sample_count == samples_in.size());
  REQUIRE(out_block.info().channel_count == 2);
  REQUIRE(out_block.info().sample_rate == 48000);

  for (size_t i = 0; i < samples_in.size(); i++) {
    REQUIRE(out_block.sample(0, i) == samples_in[i]);
    REQUIRE(out_block.sample(1, i) == 0.0f);
  }

  // check that there are now two real ATUs which reference the real channels
  auto out_atu_1 = doc_out->lookup(AudioTrackUidId{AudioTrackUidIdValue{1}});
  REQUIRE(out_atu_1);
  auto out_atu_2 = doc_out->lookup(AudioTrackUidId{AudioTrackUidIdValue{2}});
  REQUIRE(out_atu_2);

  extra_checks(out_atu_2);

  REQUIRE(adm_out.channel_map.at(out_atu_1->get<AudioTrackUidId>()) == 0);
  REQUIRE(adm_out.channel_map.at(out_atu_2->get<AudioTrackUidId>()) == 1);

  // check the other structure by item selection
  {
    auto result = select_items(doc_out);
    REQUIRE(result.warnings.size() == 0);
    REQUIRE(result.items.size() == 2);

    for (size_t i = 0; i < 2; i++) {
      const std::shared_ptr<RenderingItem> &ri = result.items.at(i);
      auto ds_ri = std::dynamic_pointer_cast<DirectSpeakersRenderingItem>(ri);
      REQUIRE(ds_ri);

      if (i == 0) REQUIRE(ds_ri->track_spec == TrackSpec{DirectTrackSpec{out_atu_1}});
      if (i == 1) REQUIRE(ds_ri->track_spec == TrackSpec{DirectTrackSpec{out_atu_2}});
    }
  }
}

TEST_CASE("remove_object_times_data_safe") {
  std::shared_ptr<adm::Document> document = adm::getCommonDefinitions();
  std::shared_ptr<const adm::Document> document_out;

  auto run = [&]() {
    Harness h(make_remove_object_times_data_safe("remove_object_times_data_safe"));
    h.input_data("in_axml", ADMData{document, {}});
    auto out = h.add_output<ADMData>("out_axml");
    h.run();

    ADMData adm_out = std::move(out->get_value());
    document_out = adm_out.document.read();
  };

  SECTION("no change") {
    auto obj = adm::addSimpleObjectTo(document, "test");
    document->add(obj.audioObject);

    obj.audioChannelFormat->add(adm::AudioBlockFormatObjects{
        adm::SphericalPosition{}, adm::Rtime{adm::FractionalTime{0, 1}}, adm::Duration{adm::FractionalTime{1, 1}}});
    obj.audioChannelFormat->add(adm::AudioBlockFormatObjects{
        adm::SphericalPosition{}, adm::Rtime{adm::FractionalTime{1, 1}}, adm::Duration{adm::FractionalTime{1, 1}}});

    run();

    auto acf_out = document_out->lookup(obj.audioChannelFormat->get<adm::AudioChannelFormatId>());
    REQUIRE(acf_out);
    auto blocks = acf_out->getElements<adm::AudioBlockFormatObjects>();
    REQUIRE(blocks.size() == 2);
    REQUIRE(blocks[0].get<adm::Rtime>().get() == adm::FractionalTime{0, 1});
    REQUIRE(blocks[1].get<adm::Rtime>().get() == adm::FractionalTime{1, 1});
  }

  SECTION("one channel") {
    auto obj = adm::addSimpleObjectTo(document, "test");
    document->add(obj.audioObject);

    obj.audioChannelFormat->add(adm::AudioBlockFormatObjects{
        adm::SphericalPosition{}, adm::Rtime{adm::FractionalTime{0, 1}}, adm::Duration{adm::FractionalTime{1, 1}}});
    obj.audioChannelFormat->add(adm::AudioBlockFormatObjects{
        adm::SphericalPosition{}, adm::Rtime{adm::FractionalTime{1, 1}}, adm::Duration{adm::FractionalTime{1, 1}}});

    obj.audioObject->set(adm::Start{adm::FractionalTime{1, 1}});
    obj.audioObject->set(adm::Duration{adm::FractionalTime{2, 1}});

    run();

    auto acf_out = document_out->lookup(obj.audioChannelFormat->get<adm::AudioChannelFormatId>());
    REQUIRE(acf_out);
    auto blocks = acf_out->getElements<adm::AudioBlockFormatObjects>();
    REQUIRE(blocks.size() == 2);
    REQUIRE(blocks[0].get<adm::Rtime>().get() == adm::FractionalTime{1, 1});
    REQUIRE(blocks[1].get<adm::Rtime>().get() == adm::FractionalTime{2, 1});

    auto ao_out = document_out->lookup(obj.audioObject->get<adm::AudioObjectId>());
    REQUIRE(ao_out);

    REQUIRE(ao_out->isDefault<adm::Start>());
    REQUIRE(!ao_out->has<adm::Duration>());
  }

  SECTION("audioObject conflict") {
    auto obj = adm::addSimpleObjectTo(document, "test");
    document->add(obj.audioObject);

    obj.audioChannelFormat->add(adm::AudioBlockFormatObjects{
        adm::SphericalPosition{}, adm::Rtime{adm::FractionalTime{0, 1}}, adm::Duration{adm::FractionalTime{1, 1}}});
    obj.audioChannelFormat->add(adm::AudioBlockFormatObjects{
        adm::SphericalPosition{}, adm::Rtime{adm::FractionalTime{1, 1}}, adm::Duration{adm::FractionalTime{1, 1}}});

    obj.audioObject->set(adm::Start{adm::FractionalTime{1, 1}});
    obj.audioObject->set(adm::Duration{adm::FractionalTime{2, 1}});

    // with a second audioObject reference to the same channel, no changes are made
    auto obj2 = adm::AudioObject::create(adm::AudioObjectName{"test2"});
    obj2->addReference(obj.audioPackFormat);
    obj2->addReference(obj.audioTrackUid);
    document->add(obj2);

    run();

    auto acf_out = document_out->lookup(obj.audioChannelFormat->get<adm::AudioChannelFormatId>());
    REQUIRE(acf_out);
    auto blocks = acf_out->getElements<adm::AudioBlockFormatObjects>();
    REQUIRE(blocks.size() == 2);
    REQUIRE(blocks[0].get<adm::Rtime>().get() == adm::FractionalTime{0, 1});
    REQUIRE(blocks[1].get<adm::Rtime>().get() == adm::FractionalTime{1, 1});

    auto ao_out = document_out->lookup(obj.audioObject->get<adm::AudioObjectId>());
    REQUIRE(ao_out);

    REQUIRE(!ao_out->isDefault<adm::Start>());
    REQUIRE(ao_out->has<adm::Duration>());
  }

  SECTION("no block times") {
    auto obj = adm::addSimpleObjectTo(document, "test");
    document->add(obj.audioObject);

    obj.audioChannelFormat->add(adm::AudioBlockFormatObjects{
        adm::SphericalPosition{},
    });

    obj.audioObject->set(adm::Start{adm::FractionalTime{1, 1}});
    obj.audioObject->set(adm::Duration{adm::FractionalTime{2, 1}});

    run();

    auto acf_out = document_out->lookup(obj.audioChannelFormat->get<adm::AudioChannelFormatId>());
    REQUIRE(acf_out);
    auto blocks = acf_out->getElements<adm::AudioBlockFormatObjects>();
    REQUIRE(blocks.size() == 1);
    REQUIRE(blocks[0].get<adm::Rtime>().get() == adm::FractionalTime{1, 1});
    REQUIRE(blocks[0].get<adm::Duration>().get() == adm::FractionalTime{2, 1});

    auto ao_out = document_out->lookup(obj.audioObject->get<adm::AudioObjectId>());
    REQUIRE(ao_out);

    REQUIRE(ao_out->isDefault<adm::Start>());
    REQUIRE(!ao_out->has<adm::Duration>());
  }
}
