#pragma once
#include <cstdint>
#include <set>
#include <string>

#include "adm/detail/enum_bitmask.hpp"

namespace eat::process {
extern const std::set<std::string> language_codes;

enum class LanguageCodeType : uint8_t {
  UNKNOWN = 0x01,       // some string that's not a language code
  REGULAR = 0x02,       // a known registered language code
  RESERVED = 0x04,      // qaa-qtz - reserved for local use
  UNCODED = 0x08,       // mis - uncoded languages
  MULTIPLE = 0x10,      // mul - multiple languages
  UNDETERMINED = 0x20,  // und - undetermined
  NO_CONTENT = 0x40,    // zxx - no linguistic content; not applicable

  SPECIAL = 0x7c,  // anything other than regular or unknown
  ANY = 0x7e,      // any valid language code

  NONE = 0x00,  // for bitwise testing
};

LanguageCodeType parse_language_code(const std::string &code);

std::string format_language_code_types(LanguageCodeType type);
}  // namespace eat::process

ENABLE_ENUM_BITMASK_OPERATORS(eat::process::LanguageCodeType)
