#pragma once
#include <adm/elements/time.hpp>
#include <adm/elements_fwd.hpp>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace eat::render {

struct SilentTrackSpec {
  auto operator<=>(const SilentTrackSpec &) const = default;
};

struct DirectTrackSpec {
  std::shared_ptr<adm::AudioTrackUid> track;
  auto operator<=>(const DirectTrackSpec &) const = default;
};

using TrackSpec = std::variant<DirectTrackSpec, SilentTrackSpec>;

struct ADMPath {
  std::shared_ptr<adm::AudioProgramme> audioProgramme;
  std::shared_ptr<adm::AudioContent> audioContent;
  std::vector<std::shared_ptr<adm::AudioObject>> audioObjects;
  std::vector<std::shared_ptr<adm::AudioPackFormat>> audioPackFormats;
  std::shared_ptr<adm::AudioChannelFormat> audioChannelFormat;
};

struct RenderingItem {
  virtual ~RenderingItem() = default;
};

struct MonoRenderingItem : public RenderingItem {
  TrackSpec track_spec;
  ADMPath adm_path;
};

struct ObjectRenderingItem : public MonoRenderingItem {};

struct DirectSpeakersRenderingItem : public MonoRenderingItem {};

struct HOATypeMetadata {
  std::optional<adm::Time> rtime;
  std::optional<adm::Time> duration;

  // one for each track
  std::vector<int> orders;
  std::vector<int> degrees;

  std::string normalization = "SN3D";

  // empty if nfcRefDist was zero
  std::optional<double> nfcRefDist = {};
  bool screenRef = false;
};

struct HOARenderingItem : public RenderingItem {
  // one for each track
  std::vector<TrackSpec> tracks;
  std::vector<ADMPath> adm_paths;

  // one for each audioBlockFormat; currently always one
  std::vector<HOATypeMetadata> type_metadata;
};

/// parent for errors raised during item selection
class ItemSelectionError : public std::runtime_error {
  // TODO: add ADMPath here to add context
 public:
  using std::runtime_error::runtime_error;
};

struct SelectionResult {
  std::vector<std::shared_ptr<RenderingItem>> items;
  std::vector<std::string> warnings;
};

struct DefaultStart {};
using ProgrammeStart = std::shared_ptr<adm::AudioProgramme>;
using ContentStart = std::vector<std::shared_ptr<adm::AudioContent>>;
using ObjectStart = std::vector<std::shared_ptr<adm::AudioObject>>;

/// start point for item selection -- either the default (first programme, all
/// objects, or all tracks), a specific programme, a set of contents, or a set
/// of objects
using SelectionStart = std::variant<DefaultStart, ProgrammeStart, ContentStart, ObjectStart>;

struct SelectionOptions {
  SelectionOptions() = default;
  SelectionOptions(SelectionStart start);

  SelectionStart start = DefaultStart{};
  // TODO: add complementary audioObjects here
};

SelectionResult select_items(const std::shared_ptr<adm::Document> &doc, const SelectionOptions &options = {});
}  // namespace eat::render
