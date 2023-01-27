#include "make_graph.hpp"

#include <catch2/catch_test_macros.hpp>

#include "eat/framework/evaluate.hpp"

using nlohmann::json;
using namespace eat::framework;
using namespace eat::config_file;

TEST_CASE("make_graph basic") {
  // basic test with two processes connected together, using both in_ports and out_ports, and connections
  auto config = json::parse(R"(
    {
      "version": 0,
      "processes": [
        {
          "name": "input",
          "type": "read_adm_bw64",
          "parameters": {
            "path": "/tmp/in.wav"
          },
          "out_ports": ["out_axml"]
        },
        {
          "name": "output",
          "type": "write_adm_bw64",
          "parameters": {
            "path": "/tmp/out.wav"
          },
          "in_ports": ["in_axml"]
        }
      ],
      "connections": [
        ["input.out_samples", "output.in_samples"]
      ]
    }
  )");

  Graph g = make_graph(config);
  REQUIRE(g.get_processes().size() == 2);

  // checks that everything is connected properly
  plan(g);
}

TEST_CASE("make_graph construct all") {
  // make sure all known processes are constructable
  auto config = json::parse(R"(
    {
      "version": 0,
      "processes": [
        {
          "name": "read_adm",
          "type": "read_adm",
          "parameters": {
            "path": "/tmp/in.wav"
          }
        },
        {
          "name": "read_bw64",
          "type": "read_bw64",
          "parameters": {
            "path": "/tmp/in.wav"
          }
        },
        {
          "name": "read_adm_bw64",
          "type": "read_adm_bw64",
          "parameters": {
            "path": "/tmp/in.wav"
          }
        },
        {
          "name": "write_adm_bw64",
          "type": "write_adm_bw64",
          "parameters": {
            "path": "/tmp/in.wav"
          }
        },
        {
          "name": "remove_unused",
          "type": "remove_unused"
        },
        {
          "name": "remove_elements",
          "type": "remove_elements",
          "parameters": {
            "ids": ["AP_00010001"]
          }
        },
        {
          "name": "validate",
          "type": "validate",
          "parameters": {
            "profile": {
              "type": "itu_emission",
              "level": 2
            }
          }
        },
        {
          "name": "fix_ds_frequency",
          "type": "fix_ds_frequency"
        },
        {
          "name": "fix_block_durations",
          "type": "fix_block_durations"
        },
        {
          "name": "fix_stream_pack_refs",
          "type": "fix_stream_pack_refs"
        },
        {
          "name": "set_profiles",
          "type": "set_profiles",
          "parameters": {
            "profiles": [
              {
                "type": "itu_emission",
                "level": 2
              }
            ]
          }
        },
        {
          "name": "set_position_defaults",
          "type": "set_position_defaults"
        },
        {
          "name": "remove_silent_atu",
          "type": "remove_silent_atu"
        }
      ]
    }
  )");

  Graph g = make_graph(config);
}
