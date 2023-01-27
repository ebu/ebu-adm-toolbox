#include "eat/process/validate.hpp"

#include <adm/document.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "eat/framework/evaluate.hpp"
#include "eat/framework/utility_processes.hpp"
#include "eat/process/validate_process.hpp"

using namespace eat::framework;
using namespace eat::process;
using namespace eat::process::validation;

TEST_CASE("validate Range") {
  SECTION("between") {
    CountRange r = CountRange::between(1, 5);
    CHECK(!r(0));
    CHECK(r(1));
    CHECK(r(5));
    CHECK(!r(6));
    CHECK(r.format() == "between 1 and 5");
  }

  SECTION("at_least") {
    CountRange r = CountRange::at_least(1);
    CHECK(!r(0));
    CHECK(r(1));
    CHECK(r(2));
    CHECK(r.format() == "at least 1");
  }

  SECTION("up_to") {
    CountRange r = CountRange::up_to(1);
    CHECK(r(0));
    CHECK(r(1));
    CHECK(!r(2));
    CHECK(r.format() == "up to 1");
  }

  SECTION("exactly") {
    CountRange r = CountRange::exactly(1);
    CHECK(!r(0));
    CHECK(r(1));
    CHECK(!r(2));
    CHECK(r.format() == "1");
  }
}

TEST_CASE("validate NumElements") {
  NumElements check{{"audioProgramme"}, "audioContent", CountRange::at_least(1), "references"};
  CHECK(format_check(check) == "audioProgramme elements must have at least 1 audioContent references");

  auto document = adm::Document::create();

  auto programme = adm::AudioProgramme::create(adm::AudioProgrammeName{"foo"});
  document->add(programme);
  auto content = adm::AudioContent::create(adm::AudioContentName{"bar"});
  document->add(content);

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 1);
    CHECK(messages[0].n == 0);
    CHECK(messages[0].path == std::vector<std::string>{"APR_1001"});

    CHECK(format_message(messages[0]) == "APR_1001 has 0 audioContent references");
  }

  programme->addReference(content);

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 0);
  }
}

TEST_CASE("validate StringLength") {
  StringLength check{{"audioProgramme", "name"}, CountRange::up_to(2)};
  CHECK(format_check(check) == "audioProgramme.name must be up to 2 characters long");

  auto document = adm::Document::create();

  auto programme = adm::AudioProgramme::create(adm::AudioProgrammeName{"foo"});
  document->add(programme);

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 1);

    CHECK(format_message(messages[0]) == "name in APR_1001 is 3 characters long");
  }

  programme->set(adm::AudioProgrammeName{"x"});

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 0);
  }
}

TEST_CASE("validate ValidLanguage") {
  ValidLanguage check{{"audioProgramme", "language"}, LanguageCodeType::REGULAR};
  CHECK(format_check(check) == "audioProgramme.language must be a regular language code");

  auto document = adm::Document::create();

  auto programme = adm::AudioProgramme::create(adm::AudioProgrammeName{"foo"}, adm::AudioProgrammeLanguage{"zzz"});
  document->add(programme);

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 1);
    CHECK(format_message(messages[0]) == "language in APR_1001 is zzz");
  }

  programme->set(adm::AudioProgrammeLanguage{"eng"});

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 0);
  }
}

TEST_CASE("validate ElementPresent") {
  ElementPresent check{{"audioProgramme", "label"}, "language", true};
  CHECK(format_check(check) == "audioProgramme.label elements must have language attributes");

  auto document = adm::Document::create();

  auto programme =
      adm::AudioProgramme::create(adm::AudioProgrammeName{"foo"}, adm::Labels{adm::Label{adm::LabelValue{"foo"}}});
  document->add(programme);

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 1);

    CHECK(format_message(messages[0]) == "label \"foo\" in APR_1001 has no language attribute");
  }

  programme->set(adm::Labels{adm::Label{adm::LabelValue{"foo"}, adm::LabelLanguage{"eng"}}});

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 0);
  }
}

TEST_CASE("validate ElementInRange") {
  ElementInRange<float> check{
      {"audioChannelFormat", "audioBlockFormat[objects,polar]", "sphericalPosition", "distance"},
      Range<float>::between(0.0f, 1.0f)};
  CHECK(format_check(check) ==
        "audioChannelFormat.audioBlockFormat[objects,polar].sphericalPosition.distance must be between 0.000000 and "
        "1.000000");

  auto document = adm::Document::create();

  auto channel_format =
      adm::AudioChannelFormat::create(adm::AudioChannelFormatName("foo"), adm::TypeDefinition::OBJECTS);
  document->add(channel_format);

  channel_format->add(adm::AudioBlockFormatObjects{adm::SphericalPosition{}});

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 0);
  }

  channel_format->add(adm::AudioBlockFormatObjects{
      adm::SphericalPosition{adm::Azimuth{0.0f}, adm::Elevation{0.0f}, adm::Distance{2.0f}}});

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 1);

    CHECK(format_message(messages[0]) ==
          "distance in sphericalPosition in AB_00031001_00000002 in AC_00031001 is 2.000000");
  }
}

TEST_CASE("validate UniqueElements") {
  UniqueElements<std::string> check{{"audioProgramme"}, {"label", "language"}};
  CHECK(format_check(check) == "audioProgramme elements must have unique label.language attributes");

  auto document = adm::Document::create();

  auto labels = adm::Labels{adm::Label{adm::LabelValue{"foo"}, adm::LabelLanguage{"en"}}};
  auto programme = adm::AudioProgramme::create(adm::AudioProgrammeName{"foo"}, labels);
  document->add(programme);

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 0);
  }

  programme->add(adm::Label{adm::LabelValue{"bar"}, adm::LabelLanguage{"en"}});

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 1);

    CHECK(format_message(messages[0]) ==
          "in APR_1001, language in label \"foo\" and language in label \"bar\" are both en");
  }
}

TEST_CASE("validate ElementInList") {
  ElementInList<std::string> check{{"version"}, {"ITU-R_BS.2076-3"}};
  CHECK(format_check(check) == "version must be ITU-R_BS.2076-3");

  auto document = adm::Document::create();
  document->set(adm::Version{"foo"});

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 1);

    CHECK(format_message(messages[0]) == "version is foo");
  }
}

TEST_CASE("validate ObjectContentOrNested") {
  ObjectContentOrNested check;
  CHECK(format_check(check) ==
        "audioObjects must have either audioObjectIdRef or audioPackFormatIdRef and audioTrackUidRef elements");

  auto document = adm::Document::create();

  auto track_uid = adm::AudioTrackUid::create();
  document->add(track_uid);

  auto content = adm::AudioObject::create(adm::AudioObjectName{"content"});
  content->addReference(track_uid);
  document->add(content);

  auto nested = adm::AudioObject::create(adm::AudioObjectName{"nested"});
  nested->addReference(content);
  document->add(nested);

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 0);
  }

  auto neither = adm::AudioObject::create(adm::AudioObjectName{"neither"});
  document->add(neither);

  auto both = adm::AudioObject::create(adm::AudioObjectName{"both"});
  both->addReference(content);
  both->addReference(track_uid);
  document->add(both);

  {
    auto messages = check.run({document, {}});
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].object_id == neither->get<adm::AudioObjectId>());
    CHECK(!messages[0].both);
    CHECK(format_message(messages[0]) == "AO_1003 has neither");

    CHECK(messages[1].object_id == both->get<adm::AudioObjectId>());
    CHECK(messages[1].both);
    CHECK(format_message(messages[1]) == "AO_1004 has both");
  }
}

TEST_CASE("validate empty") {
  std::vector<Check> checks;
  checks.push_back(NumElements{{}, "audioObject", CountRange::at_least(1), "elements"});
  checks.push_back(NumElements{{}, "audioProgramme", CountRange::at_least(1), "elements"});
  ProfileValidator validator{checks};

  auto document = adm::Document::create();
  ADMData adm{document, {}};
  ValidationResults results = validator.run(adm);

  REQUIRE(results.size() == 2);

  REQUIRE(results.at(0).messages.size() == 1);
  CHECK(std::holds_alternative<NumElements>(results.at(0).check));
  CHECK(std::get<NumElements>(results.at(0).check).element == "audioObject");
  CHECK(std::holds_alternative<NumElementsMessage>(results.at(0).messages.at(0)));
  CHECK(std::get<NumElementsMessage>(results.at(0).messages.at(0)).element == "audioObject");

  REQUIRE(results.at(1).messages.size() == 1);
  CHECK(std::holds_alternative<NumElements>(results.at(1).check));
  CHECK(std::get<NumElements>(results.at(1).check).element == "audioProgramme");
  CHECK(std::holds_alternative<NumElementsMessage>(results.at(1).messages.at(0)));
  CHECK(std::get<NumElementsMessage>(results.at(1).messages.at(0)).element == "audioProgramme");
}

TEST_CASE("validate from profile") {
  ProfileValidator validator = make_profile_validator(profiles::ITUEmissionProfile{0});

  auto document = adm::Document::create();

  {  // add a few top-level elements to make sure the paths are valid
    auto labels = adm::Labels{adm::Label{adm::LabelValue{"foo"}, adm::LabelLanguage{"en"}}};
    document->add(adm::AudioProgramme::create(adm::AudioProgrammeName{""}, labels));

    document->add(adm::AudioContent::create(adm::AudioContentName{"foo"}, labels));

    auto groupLabels = adm::AudioComplementaryObjectGroupLabels{
        adm::AudioComplementaryObjectGroupLabel{adm::Label{adm::LabelValue{"foo"}, adm::LabelLanguage{"en"}}}};
    document->add(adm::AudioObject::create(adm::AudioObjectName{"foo"}, labels, groupLabels));

    document->add(adm::AudioPackFormat::create(adm::AudioPackFormatName{"foo"}, adm::TypeDefinition::OBJECTS));

    auto channel_format =
        adm::AudioChannelFormat::create(adm::AudioChannelFormatName("foo"), adm::TypeDefinition::OBJECTS);
    document->add(channel_format);

    channel_format->add(adm::AudioBlockFormatObjects{
        adm::SphericalPosition{}, adm::ObjectDivergence{adm::Divergence{0.5f}, adm::AzimuthRange{30.0f}}});
    channel_format->add(adm::AudioBlockFormatObjects{adm::CartesianPosition{}});
  }

  ADMData adm{document, {}};
  ValidationResults results = validator.run(adm);

  // just check that one of the results got through alright. it doesn't seem worth checking
  // that the list of tests is correct (it's fairly obvious), and the tests for
  // individual checks should should cover the rest
  bool found = false;
  for (auto &check : results) {
    if (std::holds_alternative<StringLength>(check.check) &&
        std::get<StringLength>(check.check).path == std::vector<std::string>{"audioProgramme", "name"}) {
      REQUIRE(check.messages.size() == 1);
      CHECK(std::holds_alternative<StringLengthMessage>(check.messages.at(0)));
      found = true;
      break;
    }
  }
  CHECK(found);
}

TEST_CASE("validate process") {
  Graph g;

  auto document = adm::Document::create();
  ADMData adm{document, {}};
  auto in = g.add_process<DataSource<ADMData>>("in", std::move(adm));

  auto p = make_validate("validate", profiles::ITUEmissionProfile{0});
  g.register_process(p);
  g.connect(in->get_out_port("out"), p->get_in_port("in_axml"));

  validate(g);

  // TODO: run this?
  // it prints to the console and throws exceptions
  // evaluate(g);
}
