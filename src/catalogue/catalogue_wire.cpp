/*
 * File:        catalogue_wire.cpp
 * Module:      orc-plugin_teletext (shared by the stage and view binaries)
 * Purpose:     Encode and decode the catalogue view's messages
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "catalogue/catalogue_wire.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace orc {
namespace teletext_wire {
namespace {

// Every message: these four bytes, the version, then a kind byte, so a
// selection is never read as a catalogue or the other way round.
constexpr uint8_t kMagic[4] = {'O', 'T', 'C', 'W'};
constexpr uint8_t kKindSelection = 1;
constexpr uint8_t kKindCatalogue = 2;
constexpr uint8_t kKindPlayRequest = 3;
constexpr uint8_t kKindPayload = 4;
constexpr uint8_t kKindExportRequest = 5;
constexpr uint8_t kKindExportResult = 6;

// Little-endian throughout, whatever the host's byte order.
class Writer {
 public:
  static constexpr bool kReading = false;

  void u8(uint8_t& v) { out_.push_back(v); }
  void u32(uint32_t& v) {
    for (int shift = 0; shift < 32; shift += 8) {
      out_.push_back(static_cast<uint8_t>(v >> shift));
    }
  }
  void u64(uint64_t& v) {
    for (int shift = 0; shift < 64; shift += 8) {
      out_.push_back(static_cast<uint8_t>(v >> shift));
    }
  }
  std::vector<uint8_t> take() { return std::move(out_); }

 private:
  std::vector<uint8_t> out_;
};

struct Truncated : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class Reader {
 public:
  static constexpr bool kReading = true;

  explicit Reader(const std::vector<uint8_t>& in) : in_(in) {}

  void u8(uint8_t& v) {
    need(1);
    v = in_[pos_++];
  }
  void u32(uint32_t& v) {
    need(4);
    v = 0;
    for (int shift = 0; shift < 32; shift += 8) {
      v |= static_cast<uint32_t>(in_[pos_++]) << shift;
    }
  }
  void u64(uint64_t& v) {
    need(8);
    v = 0;
    for (int shift = 0; shift < 64; shift += 8) {
      v |= static_cast<uint64_t>(in_[pos_++]) << shift;
    }
  }
  /// A count read from the bytes, bounded by what is left of them, so a
  /// damaged length cannot ask for an allocation the message could not fill.
  size_t count(uint32_t n, size_t min_bytes_each) {
    const size_t left = in_.size() - pos_;
    if (min_bytes_each > 0 && n > left / min_bytes_each) {
      throw Truncated("a length runs past the end of the message");
    }
    return n;
  }
  bool at_end() const { return pos_ == in_.size(); }

 private:
  void need(size_t n) {
    if (in_.size() - pos_ < n) {
      throw Truncated("the message is cut short");
    }
  }

  const std::vector<uint8_t>& in_;
  size_t pos_ = 0;
};

// One function per type serves both directions, so a field added to the
// writer cannot be missing from the reader.

template <typename A>
void io(A& a, bool& v) {
  uint8_t b = v ? 1 : 0;
  a.u8(b);
  if constexpr (A::kReading) {
    v = b != 0;
  }
}

template <typename A>
void io(A& a, uint8_t& v) {
  a.u8(v);
}

template <typename A>
void io(A& a, int& v) {
  uint32_t u = static_cast<uint32_t>(v);
  a.u32(u);
  if constexpr (A::kReading) {
    v = static_cast<int>(u);
  }
}

template <typename A>
void io(A& a, int16_t& v) {
  uint32_t u = static_cast<uint16_t>(v);
  a.u32(u);
  if constexpr (A::kReading) {
    v = static_cast<int16_t>(static_cast<uint16_t>(u));
  }
}

template <typename A>
void io(A& a, uint32_t& v) {
  a.u32(v);
}

template <typename A>
void io(A& a, uint64_t& v) {
  a.u64(v);
}

template <typename A>
void io(A& a, char32_t& v) {
  uint32_t u = static_cast<uint32_t>(v);
  a.u32(u);
  if constexpr (A::kReading) {
    v = static_cast<char32_t>(u);
  }
}

template <typename A>
void io(A& a, double& v) {
  static_assert(sizeof(double) == sizeof(uint64_t), "IEEE double expected");
  uint64_t u = 0;
  std::memcpy(&u, &v, sizeof u);
  a.u64(u);
  if constexpr (A::kReading) {
    std::memcpy(&v, &u, sizeof u);
  }
}

template <typename A, typename E,
          typename = std::enable_if_t<std::is_enum<E>::value>>
void io(A& a, E& v) {
  uint8_t u = static_cast<uint8_t>(v);
  a.u8(u);
  if constexpr (A::kReading) {
    v = static_cast<E>(u);
  }
}

template <typename A>
uint32_t io_size(A& a, size_t size) {
  if (size > std::numeric_limits<uint32_t>::max()) {
    throw std::length_error("too many entries for the catalogue format");
  }
  uint32_t n = static_cast<uint32_t>(size);
  a.u32(n);
  return n;
}

template <typename A>
void io(A& a, std::string& v) {
  const uint32_t n = io_size(a, v.size());
  if constexpr (A::kReading) {
    v.resize(a.count(n, 1));
    for (char& c : v) {
      uint8_t b = 0;
      a.u8(b);
      c = static_cast<char>(b);
    }
  } else {
    for (char c : v) {
      uint8_t b = static_cast<uint8_t>(c);
      a.u8(b);
    }
  }
}

template <typename A>
void io(A& a, std::vector<bool>& v) {
  const uint32_t n = io_size(a, v.size());
  if constexpr (A::kReading) {
    v.assign(a.count(n, 1), false);
  }
  for (size_t i = 0; i < v.size(); ++i) {
    bool b = v[i];
    io(a, b);
    if constexpr (A::kReading) {
      v[i] = b;
    }
  }
}

template <typename A, typename T>
void io(A& a, std::vector<T>& v) {
  const uint32_t n = io_size(a, v.size());
  if constexpr (A::kReading) {
    // Every element costs at least one byte on the wire.
    v.clear();
    v.resize(a.count(n, 1));
  }
  for (T& element : v) {
    io(a, element);
  }
}

template <typename A>
void io(A& a, CatalogueColour& v) {
  io(a, v.red);
  io(a, v.green);
  io(a, v.blue);
  io(a, v.transparent);
  io(a, v.palette_index);
}

template <typename A>
void io(A& a, CatalogueColourStep& v) {
  io(a, v.time_ms);
  io(a, v.colour);
}

template <typename A>
void io(A& a, CataloguePaletteTrack& v) {
  io(a, v.steps);
  io(a, v.loop_start_ms);
  io(a, v.loop_period_ms);
}

template <typename A>
void io(A& a, CataloguePoint& v) {
  io(a, v.x);
  io(a, v.y);
}

template <typename A>
void io(A& a, CatalogueSize& v) {
  io(a, v.dx);
  io(a, v.dy);
}

template <typename A>
void io(A& a, CatalogueBitmap& v) {
  io(a, v.width);
  io(a, v.height);
  io(a, v.elements);
}

template <typename A>
void io(A& a, CatalogueCell& v) {
  io(a, v.mosaic);
  io(a, v.character);
  io(a, v.mosaic_pattern);
  io(a, v.mosaic_separated);
  io(a, v.foreground);
  io(a, v.background);
  io(a, v.double_height);
  io(a, v.double_height_lower);
  io(a, v.flash);
  io(a, v.concealed);
  io(a, v.boxed);
  io(a, v.damaged);
}

template <typename A>
void io(A& a, CatalogueRowStatus& v) {
  io(a, v.received);
  io(a, v.unconfirmed);
}

template <typename A>
void io(A& a, CatalogueCellGrid& v) {
  io(a, v.rows);
  io(a, v.columns);
  io(a, v.cells);
  io(a, v.row_status);
  io(a, v.palette);
  io(a, v.cell_aspect_width);
  io(a, v.cell_aspect_height);
  io(a, v.boxed_only);
  io(a, v.data_lost);
}

template <typename A>
void io(A& a, CatalogueDrawOp& v) {
  io(a, v.kind);
  io(a, v.points);
  io(a, v.origin);
  io(a, v.size);
  io(a, v.filled);
  io(a, v.outlined);
  io(a, v.pen_size);
  io(a, v.line_style);
  io(a, v.fill_pattern);
  io(a, v.fill_mask_size);
  io(a, v.colour);
  io(a, v.background);
  io(a, v.has_background);
  io(a, v.colour_run);
  io(a, v.text);
  io(a, v.advance);
  io(a, v.character_count);
  io(a, v.mosaic_pattern);
  io(a, v.mosaic_separated);
  io(a, v.glyph_index);
  io(a, v.rotation_degrees);
  io(a, v.reverse_video);
  io(a, v.underlined);
  io(a, v.appear_ms);
  io(a, v.reveal_duration_ms);
  io(a, v.reveal_from_y);
  io(a, v.reveal_to_y);
}

template <typename A>
void io(A& a, CatalogueDisplayList& v) {
  io(a, v.ops);
  io(a, v.aspect_height);
  io(a, v.display_aspect_height);
  io(a, v.nominal_width);
  io(a, v.nominal_height);
  io(a, v.pixel_aligned);
  io(a, v.palette);
  io(a, v.palette_tracks);
  io(a, v.glyphs);
  io(a, v.fill_masks);
}

template <typename A>
void io(A& a, CatalogueTextDocument& v) {
  io(a, v.text);
  io(a, v.monospace);
}

template <typename A>
void io(A& a, CatalogueColumn& v) {
  io(a, v.id);
  io(a, v.title);
  io(a, v.numeric);
}

template <typename A>
void io(A& a, CatalogueTable& v) {
  io(a, v.columns);
  io(a, v.rows);
}

template <typename A>
void io(A& a, CataloguePayload& v) {
  io(a, v.kind);
  io(a, v.grid);
  io(a, v.display_list);
  io(a, v.document);
  io(a, v.table);
  io(a, v.companion_text);
  io(a, v.headline);
  io(a, v.condition);
}

template <typename A>
void io(A& a, CatalogueLink& v) {
  io(a, v.role);
  io(a, v.label);
  io(a, v.target_id);
}

template <typename A>
void io(A& a, CatalogueItem& v) {
  io(a, v.id);
  io(a, v.parent_id);
  io(a, v.values);
  io(a, v.badges);
  io(a, v.selectable);
  io(a, v.tooltip);
  io(a, v.find_key);
  io(a, v.find_aliases);
  io(a, v.variant_label);
  io(a, v.links);
  io(a, v.keys);
}

template <typename A>
void io(A& a, CatalogueViewOption& v) {
  io(a, v.id);
  io(a, v.label);
  io(a, v.tooltip);
}

template <typename A>
void io(A& a, CatalogueViewToggle& v) {
  io(a, v.id);
  io(a, v.label);
  io(a, v.tooltip);
  io(a, v.active);
}

template <typename A>
void io(A& a, CatalogueSchema& v) {
  io(a, v.columns);
  io(a, v.item_noun);
  io(a, v.variant_noun);
  io(a, v.find_label);
  io(a, v.find_placeholder);
  io(a, v.highlight_label);
  io(a, v.view_label);
  io(a, v.view_options);
  io(a, v.view_option);
  io(a, v.toggles);
  io(a, v.empty_message);
  io(a, v.keypad_label);
}

template <typename A>
void io(A& a, CatalogueExportResult& v) {
  io(a, v.supported);
  io(a, v.files);
  io(a, v.failures);
  io(a, v.summary);
}

template <typename A>
void io(A& a, CatalogueSummary& v) {
  io(a, v.headline);
  io(a, v.notices);
}

template <typename A>
void io(A& a, CatalogueDataset& v) {
  io(a, v.schema);
  io(a, v.items);
  io(a, v.payloads);
  io(a, v.summary);
}

template <typename A>
void io(A& a, ViewSelection& v) {
  io(a, v.view_option);
  io(a, v.active_toggles);
}

template <typename A>
void io(A& a, PlayRequest& v) {
  io(a, v.item_id);
  io(a, v.keys);
  io(a, v.selection);
}

template <typename A>
void io(A& a, ExportRequest& v) {
  io(a, v.directory);
}

void write_header(Writer& w, uint8_t kind) {
  for (uint8_t b : kMagic) {
    w.u8(b);
  }
  uint32_t version = kCatalogueWireVersion;
  w.u32(version);
  w.u8(kind);
}

// Throws Truncated, or returns a message for a header that is not ours.
std::string read_header(Reader& r, uint8_t expected_kind) {
  for (uint8_t expected : kMagic) {
    uint8_t b = 0;
    r.u8(b);
    if (b != expected) {
      return "the reply is not a teletext catalogue message";
    }
  }
  uint32_t version = 0;
  r.u32(version);
  if (version != kCatalogueWireVersion) {
    return "the teletext stage and its view are from different releases "
           "(catalogue format " +
           std::to_string(version) + ", this view reads " +
           std::to_string(kCatalogueWireVersion) +
           "); install the plugin again so both halves match";
  }
  uint8_t kind = 0;
  r.u8(kind);
  if (kind != expected_kind) {
    return "the message is not the kind that was expected";
  }
  return std::string();
}

template <typename T>
bool decode(const std::vector<uint8_t>& bytes, uint8_t kind, T& value,
            std::string* error) {
  value = T{};
  Reader r(bytes);
  try {
    std::string problem = read_header(r, kind);
    if (problem.empty()) {
      io(r, value);
      if (!r.at_end()) {
        problem = "the message has bytes after its end";
      }
    }
    if (!problem.empty()) {
      value = T{};
      if (error) {
        *error = std::move(problem);
      }
      return false;
    }
  } catch (const Truncated& e) {
    value = T{};
    if (error) {
      *error = e.what();
    }
    return false;
  }
  return true;
}

template <typename T>
std::vector<uint8_t> encode(uint8_t kind, const T& value) {
  Writer w;
  write_header(w, kind);
  // io() takes non-const references so one function serves both directions;
  // the writer only reads through them.
  io(w, const_cast<T&>(value));
  return w.take();
}

}  // namespace

std::vector<uint8_t> encode_play_request(const PlayRequest& request) {
  return encode(kKindPlayRequest, request);
}

bool decode_play_request(const std::vector<uint8_t>& bytes,
                         PlayRequest& request, std::string* error) {
  return decode(bytes, kKindPlayRequest, request, error);
}

std::vector<uint8_t> encode_payload(const CataloguePayload& payload) {
  return encode(kKindPayload, payload);
}

bool decode_payload(const std::vector<uint8_t>& bytes,
                    CataloguePayload& payload, std::string* error) {
  return decode(bytes, kKindPayload, payload, error);
}

std::vector<uint8_t> encode_export_request(const ExportRequest& request) {
  return encode(kKindExportRequest, request);
}

bool decode_export_request(const std::vector<uint8_t>& bytes,
                           ExportRequest& request, std::string* error) {
  return decode(bytes, kKindExportRequest, request, error);
}

std::vector<uint8_t> encode_export_result(const CatalogueExportResult& result) {
  return encode(kKindExportResult, result);
}

bool decode_export_result(const std::vector<uint8_t>& bytes,
                          CatalogueExportResult& result, std::string* error) {
  return decode(bytes, kKindExportResult, result, error);
}

std::vector<uint8_t> encode_view_selection(const ViewSelection& selection) {
  return encode(kKindSelection, selection);
}

bool decode_view_selection(const std::vector<uint8_t>& bytes,
                           ViewSelection& selection, std::string* error) {
  if (bytes.empty()) {
    selection = ViewSelection{};
    return true;
  }
  return decode(bytes, kKindSelection, selection, error);
}

std::vector<uint8_t> encode_catalogue(const CatalogueDataset& dataset) {
  return encode(kKindCatalogue, dataset);
}

bool decode_catalogue(const std::vector<uint8_t>& bytes,
                      CatalogueDataset& dataset, std::string* error) {
  return decode(bytes, kKindCatalogue, dataset, error);
}

}  // namespace teletext_wire
}  // namespace orc
