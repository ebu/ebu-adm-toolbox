#include "eat/render/rendering_items.hpp"

#include <adm/common_definitions.hpp>
#include <adm/elements.hpp>
#include <adm/utilities/object_creation.hpp>
#include <catch2/catch_test_macros.hpp>

#include "rendering_items_common.hpp"

using namespace eat::render;
using namespace adm;

using AudioPackFormats = std::vector<PackFmtPointer>;
using AudioObjects = std::vector<ObjectPtr>;

TEST_CASE("rendering_items_empty") {
  auto adm = adm::Document::create();

  auto result = select_items(adm);
  REQUIRE(result.items.size() == 0);
}

/// check that a rendering item corresponds with a SimpleObjectHolder and programme/content/objects structure
void check_ri_simple_object(const std::shared_ptr<ObjectRenderingItem> &ri, const ProgrammePtr &programme,
                            const ContentPtr &content, const std::vector<ObjectPtr> &objects,
                            const SimpleObjectHolder &obj) {
  REQUIRE(std::get<DirectTrackSpec>(ri->track_spec).track == obj.audioTrackUid);
  REQUIRE(ri->adm_path.audioProgramme == programme);
  REQUIRE(ri->adm_path.audioContent == content);
  REQUIRE(ri->adm_path.audioObjects == objects);
  REQUIRE(ri->adm_path.audioPackFormats == AudioPackFormats{obj.audioPackFormat});
  REQUIRE(ri->adm_path.audioChannelFormat == obj.audioChannelFormat);
}

TEST_CASE("rendering_items_one_object_simple_programme") {
  auto adm = adm::Document::create();

  auto programme = AudioProgramme::create(AudioProgrammeName("programme"));
  adm->add(programme);

  auto content = AudioContent::create(AudioContentName("content"));
  programme->addReference(content);

  auto obj = createSimpleObject("test");
  content->addReference(obj.audioObject);

  auto result = select_items(adm);

  REQUIRE(result.items.size() == 1);

  const std::shared_ptr<RenderingItem> &ri = result.items.at(0);
  auto obj_ri = std::dynamic_pointer_cast<ObjectRenderingItem>(ri);
  REQUIRE(obj_ri);

  check_ri_simple_object(obj_ri, programme, content, {obj.audioObject}, obj);
}

TEST_CASE("rendering_items_one_object_simple_chna") {
  auto adm = adm::Document::create();

  auto obj = createSimpleObject("test");
  adm->add(obj.audioTrackUid);

  auto result = select_items(adm);

  REQUIRE(result.items.size() == 1);

  const std::shared_ptr<RenderingItem> &ri = result.items.at(0);
  auto obj_ri = std::dynamic_pointer_cast<ObjectRenderingItem>(ri);
  REQUIRE(obj_ri);

  check_ri_simple_object(obj_ri, {}, {}, {}, obj);
}

TEST_CASE("rendering_items_one_object_simple_multi_programme") {
  auto adm = adm::Document::create();

  auto programme1 = AudioProgramme::create(AudioProgrammeName("programme1"));
  adm->add(programme1);

  auto content1 = AudioContent::create(AudioContentName("content1"));
  programme1->addReference(content1);

  auto obj1 = createSimpleObject("test1");
  content1->addReference(obj1.audioObject);

  auto programme2 = AudioProgramme::create(AudioProgrammeName("programme2"));
  adm->add(programme2);

  auto content2 = AudioContent::create(AudioContentName("content2"));
  programme2->addReference(content2);

  auto obj2 = createSimpleObject("test2");
  content2->addReference(obj2.audioObject);

  SECTION("no programme selected") {
    auto result = select_items(adm);

    REQUIRE(result.items.size() == 1);
    REQUIRE(result.warnings.size() == 1);
    REQUIRE(result.warnings.at(0).find("more than one audioProgramme") == 0);

    const std::shared_ptr<RenderingItem> &ri = result.items.at(0);
    auto obj_ri = std::dynamic_pointer_cast<ObjectRenderingItem>(ri);
    REQUIRE(obj_ri);

    check_ri_simple_object(obj_ri, programme1, content1, {obj1.audioObject}, obj1);
  }

  SECTION("second programme selected") {
    auto result = select_items(adm, {ProgrammeStart{programme2}});

    REQUIRE(result.items.size() == 1);
    REQUIRE(result.warnings.size() == 0);

    const std::shared_ptr<RenderingItem> &ri = result.items.at(0);
    auto obj_ri = std::dynamic_pointer_cast<ObjectRenderingItem>(ri);
    REQUIRE(obj_ri);

    check_ri_simple_object(obj_ri, programme2, content2, {obj2.audioObject}, obj2);
  }
}

TEST_CASE("rendering_items_one_object_directspeakers") {
  auto adm = getCommonDefinitions();

  auto programme = AudioProgramme::create(AudioProgrammeName("programme"));
  adm->add(programme);

  auto content = AudioContent::create(AudioContentName("content"));
  programme->addReference(content);

  auto obj = addSimpleCommonDefinitionsObjectTo(adm, "test", "0+2+0");
  content->addReference(obj.audioObject);

  auto result = select_items(adm);

  REQUIRE(result.items.size() == 2);
  REQUIRE(result.warnings.size() == 0);

  std::vector<ObjectPtr> objects{obj.audioObject};
  std::vector<PackFmtPointer> pack_formats{adm->lookup(parseAudioPackFormatId("AP_00010002"))};
  std::vector<ChannelFmtPointer> channel_formats{adm->lookup(parseAudioChannelFormatId("AC_00010001")),
                                                 adm->lookup(parseAudioChannelFormatId("AC_00010002"))};
  std::vector<std::string> speaker_labels{"M+030", "M-030"};

  for (size_t i = 0; i < 2; i++) {
    const std::shared_ptr<RenderingItem> &ri = result.items.at(i);
    auto ds_ri = std::dynamic_pointer_cast<DirectSpeakersRenderingItem>(ri);
    REQUIRE(ds_ri);

    REQUIRE(std::get<DirectTrackSpec>(ds_ri->track_spec).track == obj.audioTrackUids.at(speaker_labels.at(i)));
    REQUIRE(ds_ri->adm_path.audioProgramme == programme);
    REQUIRE(ds_ri->adm_path.audioContent == content);
    REQUIRE(ds_ri->adm_path.audioObjects == objects);
    REQUIRE(ds_ri->adm_path.audioPackFormats == pack_formats);
    REQUIRE(ds_ri->adm_path.audioChannelFormat == channel_formats.at(i));
  }
}

TEST_CASE("rendering_items_one_object_directspeakers_silent") {
  auto adm = getCommonDefinitions();

  auto programme = AudioProgramme::create(AudioProgrammeName("programme"));
  adm->add(programme);

  auto content = AudioContent::create(AudioContentName("content"));
  programme->addReference(content);

  auto obj = AudioObject::create(AudioObjectName{"object"});
  content->addReference(obj);
  obj->addReference(adm->lookup(parseAudioPackFormatId("AP_00010002")));

  auto atu = AudioTrackUid::create();
  atu->setReference(adm->lookup(parseAudioPackFormatId("AP_00010002")));
  atu->setReference(adm->lookup(parseAudioChannelFormatId("AC_00010001")));
  obj->addReference(atu);

  obj->addReference(AudioTrackUid::getSilent(adm));

  auto result = select_items(adm);

  REQUIRE(result.items.size() == 2);
  REQUIRE(result.warnings.size() == 0);

  std::vector<ObjectPtr> objects{obj};
  std::vector<PackFmtPointer> pack_formats{adm->lookup(parseAudioPackFormatId("AP_00010002"))};
  std::vector<ChannelFmtPointer> channel_formats{adm->lookup(parseAudioChannelFormatId("AC_00010001")),
                                                 adm->lookup(parseAudioChannelFormatId("AC_00010002"))};

  for (size_t i = 0; i < 2; i++) {
    const std::shared_ptr<RenderingItem> &ri = result.items.at(i);
    auto ds_ri = std::dynamic_pointer_cast<DirectSpeakersRenderingItem>(ri);
    REQUIRE(ds_ri);

    if (i == 0) REQUIRE(ds_ri->track_spec == TrackSpec{DirectTrackSpec{atu}});
    if (i == 1) REQUIRE(ds_ri->track_spec == TrackSpec{SilentTrackSpec{}});
    REQUIRE(ds_ri->adm_path.audioProgramme == programme);
    REQUIRE(ds_ri->adm_path.audioContent == content);
    REQUIRE(ds_ri->adm_path.audioObjects == objects);
    REQUIRE(ds_ri->adm_path.audioPackFormats == pack_formats);
    REQUIRE(ds_ri->adm_path.audioChannelFormat == channel_formats.at(i));
  }
}

TEST_CASE("rendering_items_hoa") {
  auto adm = getCommonDefinitions();

  auto programme = AudioProgramme::create(AudioProgrammeName("programme"));
  adm->add(programme);

  auto content = AudioContent::create(AudioContentName("content"));
  programme->addReference(content);

  auto object = AudioObject::create(AudioObjectName("object"));
  content->addReference(object);

  auto root_pack = std::dynamic_pointer_cast<AudioPackFormatHoa>(adm->lookup(parseAudioPackFormatId("AP_00040002")));
  object->addReference(root_pack);

  std::vector<ChannelFmtPointer> channels = {
      adm->lookup(parseAudioChannelFormatId("AC_00040001")), adm->lookup(parseAudioChannelFormatId("AC_00040002")),
      adm->lookup(parseAudioChannelFormatId("AC_00040003")), adm->lookup(parseAudioChannelFormatId("AC_00040004")),
      adm->lookup(parseAudioChannelFormatId("AC_00040005")), adm->lookup(parseAudioChannelFormatId("AC_00040006")),
      adm->lookup(parseAudioChannelFormatId("AC_00040007")), adm->lookup(parseAudioChannelFormatId("AC_00040008")),
      adm->lookup(parseAudioChannelFormatId("AC_00040009")),
  };
  std::vector<TrackUIDPointer> track_uids;

  for (auto &channel : channels) {
    auto track_uid = AudioTrackUid::create();
    track_uids.push_back(track_uid);
    auto track = AudioTrackFormat::create(AudioTrackFormatName("track"), FormatDefinition::PCM);
    auto stream = AudioStreamFormat::create(AudioStreamFormatName("stream"), FormatDefinition::PCM);

    object->addReference(track_uid);

    track_uid->setReference(root_pack);
    track_uid->setReference(track);

    track->setReference(stream);

    stream->setReference(channel);
  }

  struct Components {
    SelectionResult result;
    std::shared_ptr<HOARenderingItem> ri;
    HOATypeMetadata tm;
  };

  // extract the audioBlockFormat from a channel
  auto block = [](const ChannelFmtPointer &c) -> AudioBlockFormatHoa & {
    auto blocks = c->getElements<AudioBlockFormatHoa>();
    REQUIRE(blocks.size() == 1);
    return blocks[0];
  };

  auto basic_test = [&]() {
    Components c;
    c.result = select_items(adm);

    REQUIRE(c.result.items.size() == 1);
    REQUIRE(c.result.warnings.size() == 0);

    const std::shared_ptr<RenderingItem> &ri = c.result.items.at(0);
    c.ri = std::dynamic_pointer_cast<HOARenderingItem>(ri);
    REQUIRE(c.ri);

    REQUIRE(c.ri->type_metadata.size() == 1);
    c.tm = c.ri->type_metadata.at(0);

    REQUIRE(c.ri->tracks.size() == channels.size());
    REQUIRE(c.ri->adm_paths.size() == channels.size());

    REQUIRE(c.tm.orders.size() == channels.size());
    REQUIRE(c.tm.degrees.size() == channels.size());

    for (size_t i = 0; i < channels.size(); i++) {
      REQUIRE(c.tm.orders.at(i) == block(channels.at(i)).get<Order>().get());
      REQUIRE(c.tm.degrees.at(i) == block(channels.at(i)).get<Degree>().get());

      REQUIRE(std::get<DirectTrackSpec>(c.ri->tracks.at(i)).track == track_uids.at(i));
    }

    return c;
  };

  SECTION("basic") {
    Components c = basic_test();
    REQUIRE(c.tm.normalization == "SN3D");
    REQUIRE(!c.tm.nfcRefDist);
    REQUIRE(!c.tm.screenRef);
  }

  SECTION("normalization in pack") {
    for (auto &channel : channels) block(channel).unset<Normalization>();
    root_pack->set(Normalization{"N3D"});

    Components c = basic_test();
    REQUIRE(c.tm.normalization == "N3D");
  }

  SECTION("redundant normalisation") {
    root_pack->set(Normalization{"SN3D"});

    Components c = basic_test();
    REQUIRE(c.tm.normalization == "SN3D");
  }

  SECTION("times") {
    for (auto &channel : channels) {
      block(channel).set(Rtime{std::chrono::seconds{1}});
      block(channel).set(Duration{std::chrono::seconds{2}});
    }

    Components c = basic_test();

    REQUIRE(c.tm.rtime == std::chrono::seconds{1});
    REQUIRE(c.tm.duration == std::chrono::seconds{2});
  }

  SECTION("screenRef pack") {
    root_pack->set(ScreenRef{true});

    Components c = basic_test();
    REQUIRE(c.tm.screenRef);
  }

  SECTION("screenRef channels") {
    for (auto &channel : channels) block(channel).set(ScreenRef{true});

    Components c = basic_test();
    REQUIRE(c.tm.screenRef);
  }

  SECTION("nfcRefDist zero") {
    // zero is treated as not present
    block(channels.at(0)).set(NfcRefDist{0.0});

    Components c = basic_test();
    REQUIRE(!c.tm.nfcRefDist);
  }

  SECTION("nfcRefDist pack") {
    root_pack->set(NfcRefDist{1.0});

    Components c = basic_test();
    REQUIRE(c.tm.nfcRefDist);
    REQUIRE(*c.tm.nfcRefDist == 1.0);
  }

  SECTION("nfcRefDist channels") {
    for (auto &channel : channels) block(channel).set(NfcRefDist{1.0});

    Components c = basic_test();
    REQUIRE(c.tm.nfcRefDist);
    REQUIRE(*c.tm.nfcRefDist == 1.0);
  }
}

TEST_CASE("rendering_items_one_object_simple_programme_channel_ref") {
  auto adm = adm::Document::create();

  auto programme = AudioProgramme::create(AudioProgrammeName("programme"));
  adm->add(programme);

  auto content = AudioContent::create(AudioContentName("content"));
  programme->addReference(content);

  auto obj = createSimpleObject("test");
  content->addReference(obj.audioObject);

  // libadm only supports having a track ref or a channel ref, not both, so we
  // can't test the 'both' case
  obj.audioTrackUid->removeReference<AudioTrackFormat>();
  obj.audioTrackUid->setReference(obj.audioChannelFormat);

  auto result = select_items(adm);

  REQUIRE(result.items.size() == 1);

  const std::shared_ptr<RenderingItem> &ri = result.items.at(0);
  auto obj_ri = std::dynamic_pointer_cast<ObjectRenderingItem>(ri);
  REQUIRE(obj_ri);

  check_ri_simple_object(obj_ri, programme, content, {obj.audioObject}, obj);
}
