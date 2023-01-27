#include "eat/utilities/element_visitor.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace eat::utilities::element_visitor;

TEST_CASE("element_visitor programme-content-object-name") {
  auto document = adm::Document::create();

  auto programme = adm::AudioProgramme::create(adm::AudioProgrammeName{"programme"});
  document->add(programme);

  auto content = adm::AudioContent::create(adm::AudioContentName{"content"});
  programme->addReference(content);
  document->add(content);

  auto object1 = adm::AudioObject::create(adm::AudioObjectName{"object1"});
  content->addReference(object1);
  document->add(object1);

  auto object2 = adm::AudioObject::create(adm::AudioObjectName{"object2"});
  content->addReference(object2);
  document->add(object2);

  std::vector<std::string> names;

  visit(document, {"audioProgramme", "audioContent", "audioObject", "name"},
        [&names](const Path &path) { names.push_back(path.back()->as_t<std::string>()); });

  REQUIRE(names.size() == 2);
  REQUIRE(names[0] == "object1");
  REQUIRE(names[1] == "object2");
}

TEST_CASE("element_visitor empty") {
  auto document = adm::Document::create();
  size_t count = 0;
  visit(document, {}, [&](const Path &path) { count++; });

  REQUIRE(count == 1);
}
