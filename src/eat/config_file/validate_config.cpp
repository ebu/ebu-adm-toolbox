//
// Created by Richard Bailey on 22/11/2022.
//

#include "eat/config_file/validate_config.hpp"

#include <nlohmann/json.hpp>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

#include "eat/config_file/schemas/all_schemas.hpp"

namespace eat::process {
void validate_config(nlohmann::json const &config, std::ostream &err_stream) {
  valijson::Schema schema;
  valijson::SchemaParser parser;
  valijson::adapters::NlohmannJsonAdapter adapter(eat::config_file::config);
  parser.populateSchema(adapter, schema, eat::config_file::fetch_doc, eat::config_file::free_doc);
  valijson::Validator validator;
  valijson::adapters::NlohmannJsonAdapter config_adapter(config);
  valijson::ValidationResults results;
  validator.validate(schema, config_adapter, &results);
  for (auto const &result : results) {
    err_stream << result.description << "\n\n";
    for (auto const &c : result.context) {
      err_stream << c << '\n';
    }
    if (!result.context.empty()) {
      err_stream << '\n';
    }
    err_stream.flush();
  }
  if (results.numErrors() > 0) {
    throw std::runtime_error("Config file failed schema validation.");
  }
}
}  // namespace eat::process
