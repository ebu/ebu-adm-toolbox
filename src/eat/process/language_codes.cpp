#include "eat/process/language_codes.hpp"

#include <cassert>
#include <sstream>
#include <vector>

namespace eat::process {
const std::set<std::string> language_codes = {
// generated with
// curl -s https://www.loc.gov/standards/iso639-2/ISO-639-2_8859-1.txt | awk -F '|' $'!/^(qaa|mis|mul|und|zxx)/ {printf
// "\\"%s\\",\\n", $1}'
#include "language_codes_data.hpp"
};

LanguageCodeType parse_language_code(const std::string &code) {
  if (code.size() != 3)
    return LanguageCodeType::UNKNOWN;
  else if (code >= "qaa" && code <= "qtz")
    return LanguageCodeType::RESERVED;
  else if (code == "mis")
    return LanguageCodeType::UNCODED;
  else if (code == "mul")
    return LanguageCodeType::MULTIPLE;
  else if (code == "und")
    return LanguageCodeType::UNDETERMINED;
  else if (code == "zxx")
    return LanguageCodeType::NO_CONTENT;
  else if (language_codes.find(code) != language_codes.end())
    return LanguageCodeType::REGULAR;
  else
    return LanguageCodeType::UNKNOWN;
}

std::string format_language_code_types(LanguageCodeType type) {
  assert((type & LanguageCodeType::UNKNOWN) == LanguageCodeType::NONE);
  assert(type != LanguageCodeType::NONE);

  std::vector<std::string> parts;
  if (type == LanguageCodeType::ANY) {
    return "a language code";
  } else if ((type & LanguageCodeType::REGULAR) != LanguageCodeType::NONE &&
             (type & LanguageCodeType::RESERVED) != LanguageCodeType::NONE)
    parts.push_back("a regular or reserved language code");
  else if ((type & LanguageCodeType::REGULAR) != LanguageCodeType::NONE)
    parts.push_back("a regular language code");
  else if ((type & LanguageCodeType::RESERVED) != LanguageCodeType::NONE)
    parts.push_back("a reserved language code");

  if ((type & LanguageCodeType::UNCODED) != LanguageCodeType::NONE) parts.push_back("mis");
  if ((type & LanguageCodeType::MULTIPLE) != LanguageCodeType::NONE) parts.push_back("mul");
  if ((type & LanguageCodeType::UNDETERMINED) != LanguageCodeType::NONE) parts.push_back("und");
  if ((type & LanguageCodeType::NO_CONTENT) != LanguageCodeType::NONE) parts.push_back("zxx");

  std::ostringstream s;
  assert(parts.size());

  for (size_t i = 0; i < parts.size(); i++) {
    if (i > 0) {
      if (parts.size() > 1 && i == parts.size() - 1)
        s << " or ";
      else
        s << ", ";
    }
    s << parts.at(i);
  }

  return s.str();
}

}  // namespace eat::process
