#include "eat/process/language_codes.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace eat::process;

TEST_CASE("language_codes parse") {
  REQUIRE(parse_language_code("eng") == LanguageCodeType::REGULAR);
  REQUIRE(parse_language_code("qaz") == LanguageCodeType::RESERVED);
  REQUIRE(parse_language_code("mis") == LanguageCodeType::UNCODED);
  REQUIRE(parse_language_code("mul") == LanguageCodeType::MULTIPLE);
  REQUIRE(parse_language_code("und") == LanguageCodeType::UNDETERMINED);
  REQUIRE(parse_language_code("zxx") == LanguageCodeType::NO_CONTENT);

  REQUIRE(parse_language_code("foo") == LanguageCodeType::UNKNOWN);
  REQUIRE(parse_language_code("quux") == LanguageCodeType::UNKNOWN);
}

TEST_CASE("language_codes format") {
  REQUIRE(format_language_code_types(LanguageCodeType::REGULAR) == "a regular language code");
  REQUIRE(format_language_code_types(LanguageCodeType::REGULAR | LanguageCodeType::RESERVED) ==
          "a regular or reserved language code");
  REQUIRE(format_language_code_types(LanguageCodeType::RESERVED) == "a reserved language code");
  REQUIRE(format_language_code_types(LanguageCodeType::ANY) == "a language code");
  REQUIRE(format_language_code_types(LanguageCodeType::REGULAR | LanguageCodeType::UNCODED) ==
          "a regular language code or mis");
  REQUIRE(format_language_code_types(LanguageCodeType::REGULAR | LanguageCodeType::MULTIPLE) ==
          "a regular language code or mul");
  REQUIRE(format_language_code_types(LanguageCodeType::REGULAR | LanguageCodeType::UNDETERMINED) ==
          "a regular language code or und");
  REQUIRE(format_language_code_types(LanguageCodeType::REGULAR | LanguageCodeType::NO_CONTENT) ==
          "a regular language code or zxx");

  REQUIRE(format_language_code_types(LanguageCodeType::REGULAR | LanguageCodeType::UNCODED |
                                     LanguageCodeType::MULTIPLE) == "a regular language code, mis or mul");
}
