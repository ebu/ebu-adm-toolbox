#include "eat/process/block_subelement_dropper.hpp"

#include <adm/document.hpp>
#include <adm/elements/audio_block_format_objects.hpp>
#include <adm/utilities/object_creation.hpp>
#include <catch2/catch_test_macros.hpp>

#include "eat/framework/evaluate.hpp"
#include "eat/framework/utility_processes.hpp"

namespace {
using namespace eat;

struct DropperHarness {
  using result_t = process::ADMData;
  using sink_t = framework::DataSink<result_t>;
  using source_t = framework::DataSource<result_t>;
  explicit DropperHarness(std::shared_ptr<adm::Document> input,
                          std::vector<process::BlockSubElementDropper::Droppable> to_drop)
      : source{graph.add_process<source_t>("source", process::ADMData{framework::ValuePtr(std::move(input)), {}})},
        sink{graph.add_process<sink_t>("sink")},
        dropper{graph.add_process<process::BlockSubElementDropper>("block_subelement_dropper", std::move(to_drop))} {
    graph.connect(dropper->get_out_port("out_axml"), sink->get_in_port("in"));
    graph.connect(source->get_out_port("out"), dropper->get_in_port("in_axml"));
  }

  std::shared_ptr<adm::Document> run() {
    auto p = framework::plan(graph);
    p.run();
    return sink->get_value().document.move_or_copy();
  }

  framework::Graph graph;
  std::shared_ptr<framework::DataSource<process::ADMData>> source;
  std::shared_ptr<framework::DataSink<process::ADMData>> sink;
  framework::ProcessPtr dropper;
};

template <typename ParamT, typename ParentT>
bool set_not_default(ParentT const &parent) {
  return parent.template has<ParamT>() && !parent.template isDefault<ParamT>();
}
}  // namespace

TEST_CASE("Check all provided dropped") {
  auto doc = adm::Document::create();
  auto simple = addSimpleObjectTo(doc, "test");
  auto acf = simple.audioChannelFormat;
  auto abf =
      adm::AudioBlockFormatObjects{adm::SphericalPosition(adm::Azimuth(0.5f), adm::Elevation(0.5f), adm::Distance{1.0}),
                                   adm::Diffuse{1},
                                   adm::ChannelLock{},
                                   adm::ObjectDivergence{},
                                   adm::JumpPosition{},
                                   adm::ScreenRef{},
                                   adm::Width{},
                                   adm::Depth{},
                                   adm::Height{},
                                   adm::Gain{1.0},
                                   adm::Importance{},
                                   adm::HeadLocked{},
                                   adm::HeadphoneVirtualise{}};

  CHECK(set_not_default<adm::Diffuse>(abf));
  CHECK(set_not_default<adm::ChannelLock>(abf));
  CHECK(set_not_default<adm::ObjectDivergence>(abf));
  CHECK(set_not_default<adm::JumpPosition>(abf));
  CHECK(set_not_default<adm::ScreenRef>(abf));
  CHECK(set_not_default<adm::Width>(abf));
  CHECK(set_not_default<adm::Depth>(abf));
  CHECK(set_not_default<adm::Height>(abf));
  CHECK(set_not_default<adm::Gain>(abf));
  CHECK(set_not_default<adm::Importance>(abf));
  CHECK(set_not_default<adm::HeadLocked>(abf));
  CHECK(set_not_default<adm::HeadphoneVirtualise>(abf));

  acf->add(abf);

  {
    using enum process::BlockSubElementDropper::Droppable;
    DropperHarness dropper(doc, {Diffuse, ChannelLock, ObjectDivergence, JumpPosition, ScreenRef, Width, Depth, Height,
                                 Gain, Importance, Headlocked, HeadphoneVirtualise});
    auto result = dropper.run();
    auto channel_formats = result->getElements<adm::AudioChannelFormat>();
    REQUIRE(channel_formats.size() == 1);
    auto blocks = channel_formats.front()->getElements<adm::AudioBlockFormatObjects>();
    REQUIRE(blocks.size() == 1);
    auto dropped_block = blocks.front();

    CHECK(!set_not_default<adm::Diffuse>(dropped_block));
    CHECK(!set_not_default<adm::ChannelLock>(dropped_block));
    CHECK(!set_not_default<adm::ObjectDivergence>(dropped_block));
    CHECK(!set_not_default<adm::JumpPosition>(dropped_block));
    CHECK(!set_not_default<adm::ScreenRef>(dropped_block));
    CHECK(!set_not_default<adm::Width>(dropped_block));
    CHECK(!set_not_default<adm::Depth>(dropped_block));
    CHECK(!set_not_default<adm::Height>(dropped_block));
    CHECK(!set_not_default<adm::Gain>(dropped_block));
    CHECK(!set_not_default<adm::Importance>(dropped_block));
    CHECK(!set_not_default<adm::HeadLocked>(dropped_block));
    CHECK(!set_not_default<adm::HeadphoneVirtualise>(dropped_block));
  }
}
