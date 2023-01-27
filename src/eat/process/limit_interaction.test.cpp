#include "eat/process/limit_interaction.hpp"

#include <adm/document.hpp>
#include <adm/utilities/object_creation.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "eat/framework/evaluate.hpp"
#include "eat/framework/process.hpp"
#include "eat/framework/utility_processes.hpp"

using namespace adm;
using namespace eat::process;
using namespace eat::framework;

struct LimiterHarness {
  using result_t = ADMData;
  using sink_t = DataSink<result_t>;
  using source_t = DataSource<result_t>;
  explicit LimiterHarness(std::shared_ptr<adm::Document> input, InteractionLimiter::Config config)

      : source{graph.add_process<source_t>("source", ADMData{ValuePtr(std::move(input)), {}})},
        sink{graph.add_process<sink_t>("sink")},
        limiter{graph.add_process<InteractionLimiter>("interaction_limiter", config)} {
    graph.connect(limiter->get_out_port("out_axml"), sink->get_in_port("in"));
    graph.connect(source->get_out_port("out"), limiter->get_in_port("in_axml"));
  }

  std::shared_ptr<adm::Document> run() {
    auto p = plan(graph);
    p.run();
    return sink->get_value().document.move_or_copy();
  }

  Graph graph;
  std::shared_ptr<DataSource<ADMData>> source;
  std::shared_ptr<DataSink<ADMData>> sink;
  ProcessPtr limiter;
};

TEST_CASE("Limiting position ranges, Spherical") {
  auto doc = adm::Document::create();
  auto holder = addSimpleObjectTo(doc, "Test");
  auto MINIMUM_ANGLE = -30.0f;
  auto BELOW_MIN_ANGLE = -40.0f;

  auto MAXIMUM_ANGLE = 30.0f;
  auto ABOVE_MAX_ANGLE = 40.0f;

  auto MINIMUM_DISTANCE = 0.5f;
  auto BELOW_MIN_DISTANCE = 0.1f;

  auto MAXIMUM_DISTANCE = 1.0f;
  auto ABOVE_MAX_DISTANCE = 1.5f;

  auto input_position_interaction_ranges =
      PositionInteractionRange(AzimuthInteractionMin(BELOW_MIN_ANGLE), ElevationInteractionMin(BELOW_MIN_ANGLE),
                               DistanceInteractionMin(BELOW_MIN_DISTANCE), AzimuthInteractionMax(ABOVE_MAX_ANGLE),
                               ElevationInteractionMax(ABOVE_MAX_ANGLE), DistanceInteractionMax(ABOVE_MAX_DISTANCE));

  PositionInteractionConstraint position_interaction_range_limits;
  position_interaction_range_limits.azimuth.min = {MINIMUM_ANGLE, MINIMUM_ANGLE};
  position_interaction_range_limits.azimuth.max = {MAXIMUM_ANGLE, MAXIMUM_ANGLE};
  position_interaction_range_limits.elevation.min = {MINIMUM_ANGLE, MINIMUM_ANGLE};
  position_interaction_range_limits.elevation.max = {MAXIMUM_ANGLE, MAXIMUM_ANGLE};
  position_interaction_range_limits.distance.min = {MINIMUM_DISTANCE, MINIMUM_DISTANCE};
  position_interaction_range_limits.distance.max = {MAXIMUM_DISTANCE, MAXIMUM_DISTANCE};

  holder.audioObject->set(AudioObjectInteraction(OnOffInteract{false}, input_position_interaction_ranges));
  InteractionLimiter::Config config{false, GainInteractionConstraint{}, position_interaction_range_limits};

  auto harness = LimiterHarness(doc, config);

  auto result = harness.run();
  auto objects = result->getElements<AudioObject>();

  REQUIRE(objects.size() == 1);
  REQUIRE(objects.front()->has<adm::AudioObjectInteraction>());
  auto interaction = objects.front()->get<AudioObjectInteraction>();
  REQUIRE(interaction.has<PositionInteractionRange>());
  auto range = interaction.get<PositionInteractionRange>();

  REQUIRE(range.has<AzimuthInteractionMin>());
  CHECK(range.get<AzimuthInteractionMin>().get() == Catch::Approx(MINIMUM_ANGLE));
  REQUIRE(range.has<ElevationInteractionMin>());
  CHECK(range.get<ElevationInteractionMin>().get() == Catch::Approx(MINIMUM_ANGLE));
  REQUIRE(range.has<DistanceInteractionMin>());
  CHECK(range.get<DistanceInteractionMin>().get() == Catch::Approx(MINIMUM_DISTANCE));
  REQUIRE(range.has<AzimuthInteractionMax>());
  CHECK(range.get<AzimuthInteractionMax>().get() == Catch::Approx(MAXIMUM_ANGLE));
  REQUIRE(range.has<ElevationInteractionMax>());
  CHECK(range.get<ElevationInteractionMax>().get() == Catch::Approx(MAXIMUM_ANGLE));
  REQUIRE(range.has<DistanceInteractionMax>());
  CHECK(range.get<DistanceInteractionMax>().get() == Catch::Approx(MAXIMUM_DISTANCE));
}

TEST_CASE("Limiting position ranges, Cartesian") {
  auto doc = adm::Document::create();
  auto holder = addSimpleObjectTo(doc, "Test");
  auto MINIMUM_VALUE = -0.5f;
  auto BELOW_MIN_VALUE = -0.8f;

  auto MAXIMUM_VALUE = 0.5f;
  auto ABOVE_MAX_VALUE = 0.8f;

  auto input_position_interaction_ranges = PositionInteractionRange(
      XInteractionMin(BELOW_MIN_VALUE), YInteractionMin(BELOW_MIN_VALUE), ZInteractionMin(BELOW_MIN_VALUE),
      XInteractionMax(ABOVE_MAX_VALUE), YInteractionMax(ABOVE_MAX_VALUE), ZInteractionMax(ABOVE_MAX_VALUE));
  holder.audioObject->set(AudioObjectInteraction(OnOffInteract{false}, input_position_interaction_ranges));

  PositionInteractionConstraint position_interaction_range_limits;
  position_interaction_range_limits.x.min = {MINIMUM_VALUE, MINIMUM_VALUE};
  position_interaction_range_limits.x.max = {MAXIMUM_VALUE, MAXIMUM_VALUE};
  position_interaction_range_limits.y.min = {MINIMUM_VALUE, MINIMUM_VALUE};
  position_interaction_range_limits.y.max = {MAXIMUM_VALUE, MAXIMUM_VALUE};
  position_interaction_range_limits.z.min = {MINIMUM_VALUE, MINIMUM_VALUE};
  position_interaction_range_limits.z.max = {MAXIMUM_VALUE, MAXIMUM_VALUE};
  InteractionLimiter::Config config{false, GainInteractionConstraint{}, position_interaction_range_limits};

  auto harness = LimiterHarness(doc, config);

  auto result = harness.run();
  auto objects = result->getElements<AudioObject>();

  REQUIRE(objects.size() == 1);
  REQUIRE(objects.front()->has<adm::AudioObjectInteraction>());
  auto interaction = objects.front()->get<AudioObjectInteraction>();
  REQUIRE(interaction.has<PositionInteractionRange>());
  auto range = interaction.get<PositionInteractionRange>();

  REQUIRE(range.has<XInteractionMin>());
  CHECK(range.get<XInteractionMin>().get() == Catch::Approx(MINIMUM_VALUE));
  REQUIRE(range.has<YInteractionMin>());
  CHECK(range.get<YInteractionMin>().get() == Catch::Approx(MINIMUM_VALUE));
  REQUIRE(range.has<ZInteractionMin>());
  CHECK(range.get<ZInteractionMin>().get() == Catch::Approx(MINIMUM_VALUE));
  REQUIRE(range.has<XInteractionMax>());
  CHECK(range.get<XInteractionMax>().get() == Catch::Approx(MAXIMUM_VALUE));
  REQUIRE(range.has<YInteractionMax>());
  CHECK(range.get<YInteractionMax>().get() == Catch::Approx(MAXIMUM_VALUE));
  REQUIRE(range.has<ZInteractionMax>());
  CHECK(range.get<ZInteractionMax>().get() == Catch::Approx(MAXIMUM_VALUE));
}

TEST_CASE("Limiting gain ranges, linear") {
  auto doc = adm::Document::create();
  auto holder = addSimpleObjectTo(doc, "Test");
  auto MINIMUM_VALUE = 0.5f;
  auto BELOW_MIN_VALUE = 0.2f;

  auto MAXIMUM_VALUE = 0.8f;
  auto ABOVE_MAX_VALUE = 0.9f;

  auto input_gain_interaction_range =
      GainInteractionRange(GainInteractionMin(Gain(BELOW_MIN_VALUE)), GainInteractionMax(Gain(ABOVE_MAX_VALUE)));

  GainInteractionConstraint gain_interaction_range_limits;
  gain_interaction_range_limits.min = {MINIMUM_VALUE, MINIMUM_VALUE};
  gain_interaction_range_limits.max = {MAXIMUM_VALUE, MAXIMUM_VALUE};

  holder.audioObject->set(AudioObjectInteraction(OnOffInteract{false}, input_gain_interaction_range));
  InteractionLimiter::Config config{false, gain_interaction_range_limits, PositionInteractionConstraint{}};
  auto harness = LimiterHarness(doc, config);

  auto result = harness.run();
  auto objects = result->getElements<AudioObject>();

  REQUIRE(objects.size() == 1);
  REQUIRE(objects.front()->has<adm::AudioObjectInteraction>());
  auto interaction = objects.front()->get<AudioObjectInteraction>();
  REQUIRE(interaction.has<GainInteractionRange>());
  auto range = interaction.get<GainInteractionRange>();

  REQUIRE(range.has<GainInteractionMin>());
  auto gainMin = range.get<GainInteractionMin>();
  CHECK(gainMin->isLinear());
  CHECK(gainMin->get() == Catch::Approx(MINIMUM_VALUE));
  REQUIRE(range.has<GainInteractionMax>());
  auto gainMax = range.get<GainInteractionMax>();
  CHECK(gainMax->isLinear());
  CHECK(gainMax->get() == Catch::Approx(MAXIMUM_VALUE));
}

TEST_CASE("Limiting gain ranges, dB") {
  auto doc = adm::Document::create();
  auto holder = addSimpleObjectTo(doc, "Test");
  auto MINIMUM_DB = -90.0f;
  auto MINIMUM_LINEAR = static_cast<float>(Gain::fromDb(MINIMUM_DB).asLinear());
  auto BELOW_MIN_DB = -100.f;

  auto MAXIMUM_DB = 0.0f;
  auto MAXIMUM_LINEAR = static_cast<float>(Gain::fromDb(MAXIMUM_DB).asLinear());
  auto ABOVE_MAX_DB = 1.0f;

  auto input_gain_interaction_range = GainInteractionRange(GainInteractionMin(Gain::fromDb(BELOW_MIN_DB)),
                                                           GainInteractionMax(Gain::fromDb(ABOVE_MAX_DB)));

  GainInteractionConstraint gain_interaction_range_limits;
  gain_interaction_range_limits.min = {MINIMUM_LINEAR, MINIMUM_LINEAR};
  gain_interaction_range_limits.max = {MAXIMUM_LINEAR, MAXIMUM_LINEAR};

  holder.audioObject->set(AudioObjectInteraction(OnOffInteract{false}, input_gain_interaction_range));

  InteractionLimiter::Config config{false, gain_interaction_range_limits, PositionInteractionConstraint{}};
  auto harness = LimiterHarness(doc, config);

  auto result = harness.run();
  auto objects = result->getElements<AudioObject>();

  REQUIRE(objects.size() == 1);
  REQUIRE(objects.front()->has<adm::AudioObjectInteraction>());
  auto interaction = objects.front()->get<AudioObjectInteraction>();
  REQUIRE(interaction.has<GainInteractionRange>());
  auto range = interaction.get<GainInteractionRange>();

  REQUIRE(range.has<GainInteractionMin>());
  auto gainMin = range.get<GainInteractionMin>();
  CHECK(gainMin->isDb());
  CHECK(gainMin->asDb() == Catch::Approx(MINIMUM_DB));
  REQUIRE(range.has<GainInteractionMax>());
  auto gainMax = range.get<GainInteractionMax>();
  CHECK(gainMax->isDb());
  CHECK(gainMax->asDb() == Catch::Approx(MAXIMUM_DB));
}
