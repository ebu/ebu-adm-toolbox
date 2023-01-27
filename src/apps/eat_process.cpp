#include <fstream>
#include <iostream>
#include <sstream>

#include "../eat/config_file/make_graph.hpp"
#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"
#include "eat/config_file/validate_config.hpp"
#include "eat/framework/evaluate.hpp"

using namespace eat::framework;
using namespace eat::config_file;

namespace {
template <typename T>
nlohmann::json parse_jaon(T &&value, bool allow_exceptions = true) {
  return nlohmann::json::parse(std::forward<T>(value), /*parser_callback_t*/ nullptr,
                               /*allow_exceptions*/ allow_exceptions, /*ignore_comments*/ true);
}

void set_value(nlohmann::json &config, const std::string &loc, nlohmann::json value) {
  size_t dot_pos = loc.find('.');
  if (dot_pos == std::string::npos) throw std::runtime_error("options must have the form process_name.option_name ");

  std::string process_name = loc.substr(0, dot_pos);
  std::string path_str = loc.substr(dot_pos, loc.size());
  for (size_t i = 0; i < path_str.size(); i++)
    if (path_str[i] == '.') path_str[i] = '/';

  nlohmann::json::json_pointer path{path_str};

  auto &processes = config.at("processes");
  if (!processes.is_array()) throw std::runtime_error("expected processes to be object");

  bool found = false;
  for (auto &process : processes)
    if (process.at("name").get<std::string>() == process_name) {
      // add parameters object if there is not already one
      if (!process.contains("parameters")) process["parameters"] = nlohmann::json::value_t::object;
      auto &parameters = process.at("parameters");

      parameters[path] = std::move(value);

      found = true;
      break;
    }
  if (!found) throw std::runtime_error("could not find process named " + process_name);
}
}  // namespace

int main(int argc, char **argv) {
  CLI::App app{"process ADM files with a graph defined in a configuration file; part of the EBU ADM Toolbox"};

  std::string config_file;
  app.add_option("config", config_file, "json config file path")->required()->check(CLI::ExistingFile);

  std::vector<std::pair<std::string, std::string>> options;
  app.add_option("--option,-o", options, "options to set or override in the config file");

  std::vector<std::pair<std::string, std::string>> strict_options;
  app.add_option("--strict-option,-s", strict_options,
                 "options to set or override in the config file, interpreted directly as json");

  bool progress;
  app.add_flag("--progress,-p", progress, "show progress bars");

  CLI11_PARSE(app, argc, argv);

  nlohmann::json config_json;
  {
    std::ifstream f(config_file);
    config_json = parse_jaon(f);
  }

  for (auto &[path, value] : strict_options) set_value(config_json, path, parse_jaon(value));

  for (auto &[path, value] : options) {
    auto value_json = parse_jaon(value, false);
    if (value_json.is_discarded()) value_json = value;

    set_value(config_json, path, value_json);
  }

  {
    std::stringstream ss;
    try {
      eat::process::validate_config(config_json, ss);
    } catch (std::runtime_error const &e) {
      std::cerr << e.what();
      std::cerr << ss.str();
      return 65;  // user data error, EX_DATAERR
    }
  }

  Graph g = make_graph(config_json);

  Plan p = plan(g);
  if (progress)
    run_with_progress(p);
  else
    p.run();

  return 0;
}
