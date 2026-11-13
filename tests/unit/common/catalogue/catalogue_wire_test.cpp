/*
 * File:        catalogue_wire_test.cpp
 * Module:      orc-plugin_teletext tests
 * Purpose:     The catalogue survives the trip from the stage binary to the
 *              view binary, and damaged or foreign bytes are refused
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "catalogue/catalogue_wire.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using namespace orc;
using namespace orc::teletext_wire;

// Every field away from its default, so a field the format forgets comes back
// as its default and fails below.
CatalogueDataset every_field_set() {
  CatalogueDataset d;
  d.schema.columns = {{"page", "Page", true}};
  d.schema.item_noun = "Page";
  d.schema.variant_noun = "Sub-page";
  d.schema.find_label = "Go to";
  d.schema.find_placeholder = "100";
  d.schema.highlight_label = "Damage";
  d.schema.view_label = "Receiver";
  d.schema.view_options = {{"a", "A", "tip a"}, {"b", "B", "tip b"}};
  d.schema.view_option = "b";
  d.schema.toggles = {{"t", "Toggle", "tip t", true}};
  d.schema.empty_message = "nothing";
  d.schema.keypad_label = "Keypad";

  CatalogueItem item;
  item.id = "100";
  item.parent_id = "1";
  item.values = {"100", "Index"};
  item.badges = {"subs"};
  item.selectable = false;
  item.tooltip = "tip";
  item.find_key = "100";
  item.find_aliases = {"150", "1985"};
  item.variant_label = "1/2";
  item.links = {{CatalogueLink::Role::kKey, "Red", "200"},
                {CatalogueLink::Role::kReference, "see 300", "300"}};
  item.keys = {"1", "2", "#"};
  d.items.push_back(item);

  CataloguePayload grid;
  grid.kind = CataloguePayload::Kind::kCellGrid;
  grid.grid.rows = 1;
  grid.grid.columns = 1;
  CatalogueCell cell;
  cell.mosaic = true;
  cell.character = U'é';
  cell.mosaic_pattern = 0x2a;
  cell.mosaic_separated = true;
  cell.foreground = 3;
  cell.background = 4;
  cell.double_height = true;
  cell.double_height_lower = true;
  cell.flash = true;
  cell.concealed = true;
  cell.boxed = true;
  cell.damaged = true;
  grid.grid.cells = {cell};
  grid.grid.row_status = {{false, true}};
  grid.grid.palette = {{1, 2, 3, true, -2}};
  grid.grid.cell_aspect_width = 12;
  grid.grid.cell_aspect_height = 10;
  grid.grid.boxed_only = true;
  grid.grid.data_lost = true;
  grid.companion_text = "companion";
  grid.headline = "headline";
  grid.condition = "condition";
  d.payloads.push_back(grid);

  d.items.push_back(item);
  CataloguePayload list;
  list.kind = CataloguePayload::Kind::kDisplayList;
  CatalogueDrawOp op;
  op.kind = CatalogueDrawKind::kGlyph;
  op.points = {{0.25, -0.5}};
  op.origin = {0.125, 0.75};
  op.size = {1.5, 2.5};
  op.filled = true;
  op.outlined = true;
  op.pen_size = {0.01, 0.02};
  op.line_style = CatalogueLineStyle::kDashDotted;
  op.fill_pattern = CatalogueFillPattern::kMask2;
  op.fill_mask_size = {0.3, 0.4};
  op.colour = {10, 20, 30, false};
  op.background = {40, 50, 60, true};
  op.has_background = true;
  op.appear_ms = 1500;
  op.reveal_duration_ms = 250;
  op.reveal_from_y = 0.9;
  op.reveal_to_y = 0.1;
  op.colour_run = {{1, 1, 1, false}, {2, 2, 2, true}};
  op.text = "text";
  op.advance = {0.05, 0.0};
  op.character_count = 4;
  op.mosaic_pattern = 7;
  op.mosaic_separated = true;
  op.glyph_index = 2;
  op.rotation_degrees = -90;
  op.reverse_video = true;
  op.underlined = true;
  list.display_list.ops = {op};
  list.display_list.aspect_height = 0.75;
  list.display_list.display_aspect_height = 0.8;
  list.display_list.nominal_width = 256;
  list.display_list.nominal_height = 200;
  list.display_list.pixel_aligned = true;
  list.display_list.palette = {{5, 6, 7, false, 3}};
  CataloguePaletteTrack track;
  track.steps = {{0, {1, 2, 3, false}}, {400, {4, 5, 6, true, 7}}};
  track.loop_start_ms = 100;
  track.loop_period_ms = 800;
  list.display_list.palette_tracks = {CataloguePaletteTrack{}, track};
  CatalogueBitmap bitmap;
  bitmap.width = 2;
  bitmap.height = 1;
  bitmap.elements = {true, false};
  list.display_list.glyphs = {bitmap};
  list.display_list.fill_masks = {bitmap, bitmap};
  list.document.text = "listing";
  list.document.monospace = false;
  list.table.columns = {{"c", "C", false}};
  list.table.rows = {{"r1"}, {"r2"}};
  d.payloads.push_back(list);

  d.summary.headline = "summary";
  d.summary.notices = {"one", "two"};
  return d;
}

CatalogueDataset round_trip(const CatalogueDataset& in) {
  CatalogueDataset out;
  std::string error;
  EXPECT_TRUE(decode_catalogue(encode_catalogue(in), out, &error)) << error;
  return out;
}

TEST(CatalogueWire, EveryFieldSurvives) {
  const CatalogueDataset out = round_trip(every_field_set());

  EXPECT_EQ(out.schema.columns.at(0).id, "page");
  EXPECT_TRUE(out.schema.columns.at(0).numeric);
  EXPECT_EQ(out.schema.item_noun, "Page");
  EXPECT_EQ(out.schema.variant_noun, "Sub-page");
  EXPECT_EQ(out.schema.find_label, "Go to");
  EXPECT_EQ(out.schema.find_placeholder, "100");
  EXPECT_EQ(out.schema.highlight_label, "Damage");
  EXPECT_EQ(out.schema.view_label, "Receiver");
  ASSERT_EQ(out.schema.view_options.size(), 2u);
  EXPECT_EQ(out.schema.view_options[1].tooltip, "tip b");
  EXPECT_EQ(out.schema.view_option, "b");
  ASSERT_EQ(out.schema.toggles.size(), 1u);
  EXPECT_EQ(out.schema.toggles[0].label, "Toggle");
  EXPECT_TRUE(out.schema.toggles[0].active);
  EXPECT_EQ(out.schema.empty_message, "nothing");
  EXPECT_EQ(out.schema.keypad_label, "Keypad");

  ASSERT_EQ(out.items.size(), 2u);
  const CatalogueItem& item = out.items[0];
  EXPECT_EQ(item.id, "100");
  EXPECT_EQ(item.parent_id, "1");
  EXPECT_EQ(item.values, (std::vector<std::string>{"100", "Index"}));
  EXPECT_EQ(item.badges, std::vector<std::string>{"subs"});
  EXPECT_FALSE(item.selectable);
  EXPECT_EQ(item.tooltip, "tip");
  EXPECT_EQ(item.find_key, "100");
  EXPECT_EQ(item.find_aliases, (std::vector<std::string>{"150", "1985"}));
  EXPECT_EQ(item.variant_label, "1/2");
  ASSERT_EQ(item.links.size(), 2u);
  EXPECT_EQ(item.links[0].role, CatalogueLink::Role::kKey);
  EXPECT_EQ(item.links[0].label, "Red");
  EXPECT_EQ(item.links[0].target_id, "200");
  EXPECT_EQ(item.links[1].role, CatalogueLink::Role::kReference);
  EXPECT_EQ(item.keys, (std::vector<std::string>{"1", "2", "#"}));

  ASSERT_EQ(out.payloads.size(), 2u);
  const CataloguePayload& grid = out.payloads[0];
  EXPECT_EQ(grid.kind, CataloguePayload::Kind::kCellGrid);
  ASSERT_TRUE(grid.grid.valid());
  const CatalogueCell& cell = grid.grid.at(0, 0);
  EXPECT_TRUE(cell.mosaic);
  EXPECT_EQ(cell.character, U'é');
  EXPECT_EQ(cell.mosaic_pattern, 0x2a);
  EXPECT_TRUE(cell.mosaic_separated);
  EXPECT_EQ(cell.foreground, 3);
  EXPECT_EQ(cell.background, 4);
  EXPECT_TRUE(cell.double_height);
  EXPECT_TRUE(cell.double_height_lower);
  EXPECT_TRUE(cell.flash);
  EXPECT_TRUE(cell.concealed);
  EXPECT_TRUE(cell.boxed);
  EXPECT_TRUE(cell.damaged);
  ASSERT_EQ(grid.grid.row_status.size(), 1u);
  EXPECT_FALSE(grid.grid.row_status[0].received);
  EXPECT_TRUE(grid.grid.row_status[0].unconfirmed);
  ASSERT_EQ(grid.grid.palette.size(), 1u);
  EXPECT_EQ(grid.grid.palette[0], (CatalogueColour{1, 2, 3, true, -2}));
  EXPECT_EQ(grid.grid.cell_aspect_width, 12);
  EXPECT_EQ(grid.grid.cell_aspect_height, 10);
  EXPECT_TRUE(grid.grid.boxed_only);
  EXPECT_TRUE(grid.grid.data_lost);
  EXPECT_EQ(grid.companion_text, "companion");
  EXPECT_EQ(grid.headline, "headline");
  EXPECT_EQ(grid.condition, "condition");

  const CataloguePayload& list = out.payloads[1];
  EXPECT_EQ(list.kind, CataloguePayload::Kind::kDisplayList);
  ASSERT_EQ(list.display_list.ops.size(), 1u);
  const CatalogueDrawOp& op = list.display_list.ops[0];
  EXPECT_EQ(op.kind, CatalogueDrawKind::kGlyph);
  ASSERT_EQ(op.points.size(), 1u);
  EXPECT_EQ(op.points[0].x, 0.25);
  EXPECT_EQ(op.points[0].y, -0.5);
  EXPECT_EQ(op.origin.x, 0.125);
  EXPECT_EQ(op.origin.y, 0.75);
  EXPECT_EQ(op.size.dx, 1.5);
  EXPECT_EQ(op.size.dy, 2.5);
  EXPECT_TRUE(op.filled);
  EXPECT_TRUE(op.outlined);
  EXPECT_EQ(op.pen_size.dx, 0.01);
  EXPECT_EQ(op.pen_size.dy, 0.02);
  EXPECT_EQ(op.line_style, CatalogueLineStyle::kDashDotted);
  EXPECT_EQ(op.fill_pattern, CatalogueFillPattern::kMask2);
  EXPECT_EQ(op.fill_mask_size.dx, 0.3);
  EXPECT_EQ(op.fill_mask_size.dy, 0.4);
  EXPECT_EQ(op.colour, (CatalogueColour{10, 20, 30, false}));
  EXPECT_EQ(op.background, (CatalogueColour{40, 50, 60, true}));
  EXPECT_TRUE(op.has_background);
  EXPECT_EQ(op.appear_ms, 1500u);
  EXPECT_EQ(op.reveal_duration_ms, 250u);
  EXPECT_EQ(op.reveal_from_y, 0.9);
  EXPECT_EQ(op.reveal_to_y, 0.1);
  ASSERT_EQ(op.colour_run.size(), 2u);
  EXPECT_EQ(op.colour_run[1], (CatalogueColour{2, 2, 2, true}));
  EXPECT_EQ(op.text, "text");
  EXPECT_EQ(op.advance.dx, 0.05);
  EXPECT_EQ(op.character_count, 4);
  EXPECT_EQ(op.mosaic_pattern, 7);
  EXPECT_TRUE(op.mosaic_separated);
  EXPECT_EQ(op.glyph_index, 2);
  EXPECT_EQ(op.rotation_degrees, -90);
  EXPECT_TRUE(op.reverse_video);
  EXPECT_TRUE(op.underlined);
  EXPECT_EQ(list.display_list.aspect_height, 0.75);
  EXPECT_EQ(list.display_list.display_aspect_height, 0.8);
  EXPECT_EQ(list.display_list.nominal_width, 256);
  EXPECT_EQ(list.display_list.nominal_height, 200);
  EXPECT_TRUE(list.display_list.pixel_aligned);
  ASSERT_EQ(list.display_list.palette.size(), 1u);
  EXPECT_EQ(list.display_list.palette[0].palette_index, 3);
  ASSERT_EQ(list.display_list.palette_tracks.size(), 2u);
  EXPECT_TRUE(list.display_list.palette_tracks[0].steps.empty());
  const CataloguePaletteTrack& track = list.display_list.palette_tracks[1];
  ASSERT_EQ(track.steps.size(), 2u);
  EXPECT_EQ(track.steps[1].time_ms, 400u);
  EXPECT_EQ(track.steps[1].colour, (CatalogueColour{4, 5, 6, true, 7}));
  EXPECT_EQ(track.loop_start_ms, 100u);
  EXPECT_EQ(track.loop_period_ms, 800u);
  EXPECT_TRUE(list.display_list.timed());
  ASSERT_EQ(list.display_list.glyphs.size(), 1u);
  EXPECT_TRUE(list.display_list.glyphs[0].defined());
  EXPECT_EQ(list.display_list.glyphs[0].elements,
            (std::vector<bool>{true, false}));
  EXPECT_EQ(list.display_list.fill_masks.size(), 2u);
  EXPECT_EQ(list.document.text, "listing");
  EXPECT_FALSE(list.document.monospace);
  ASSERT_EQ(list.table.columns.size(), 1u);
  EXPECT_FALSE(list.table.columns[0].numeric);
  ASSERT_EQ(list.table.rows.size(), 2u);
  EXPECT_EQ(list.table.rows[1], std::vector<std::string>{"r2"});

  EXPECT_EQ(out.summary.headline, "summary");
  EXPECT_EQ(out.summary.notices, (std::vector<std::string>{"one", "two"}));
}

TEST(CatalogueWire, AnEmptyCatalogueSurvives) {
  const CatalogueDataset out = round_trip(CatalogueDataset{});
  EXPECT_TRUE(out.items.empty());
  EXPECT_TRUE(out.consistent());
}

TEST(CatalogueWire, ASelectionSurvives) {
  ViewSelection in;
  in.view_option = "512 x 400";
  in.active_toggles = {"repair", "reconstruct_lost"};
  ViewSelection out;
  std::string error;
  ASSERT_TRUE(decode_view_selection(encode_view_selection(in), out, &error))
      << error;
  EXPECT_EQ(out.view_option, in.view_option);
  EXPECT_EQ(out.active_toggles, in.active_toggles);
}

TEST(CatalogueWire, APlayRequestSurvives) {
  PlayRequest in;
  in.item_id = "A00/1A4";
  in.keys = {"1", "9"};
  in.selection.view_option = "256 x 200";
  in.selection.active_toggles = {"repair"};
  PlayRequest out;
  std::string error;
  ASSERT_TRUE(decode_play_request(encode_play_request(in), out, &error))
      << error;
  EXPECT_EQ(out.item_id, in.item_id);
  EXPECT_EQ(out.keys, in.keys);
  EXPECT_EQ(out.selection.view_option, in.selection.view_option);
  EXPECT_EQ(out.selection.active_toggles, in.selection.active_toggles);
}

TEST(CatalogueWire, APayloadSurvives) {
  const CatalogueDataset d = every_field_set();
  CataloguePayload out;
  std::string error;
  ASSERT_TRUE(decode_payload(encode_payload(d.payloads[1]), out, &error))
      << error;
  EXPECT_EQ(out.kind, CataloguePayload::Kind::kDisplayList);
  EXPECT_EQ(out.display_list.ops.size(), 1u);
  EXPECT_EQ(out.document.text, "listing");
}

TEST(CatalogueWire, AnExportRequestAndResultSurvive) {
  ExportRequest request{"/tmp/export here"};
  ExportRequest request_out;
  ASSERT_TRUE(decode_export_request(encode_export_request(request), request_out,
                                    nullptr));
  EXPECT_EQ(request_out.directory, request.directory);

  CatalogueExportResult result;
  result.supported = true;
  result.files = 5000000000ull;
  result.failures = {"pages/100: disk full"};
  result.summary = {"412 records, as sent, under records/"};
  CatalogueExportResult result_out;
  ASSERT_TRUE(
      decode_export_result(encode_export_result(result), result_out, nullptr));
  EXPECT_TRUE(result_out.supported);
  EXPECT_EQ(result_out.files, 5000000000ull);
  EXPECT_EQ(result_out.failures, result.failures);
  EXPECT_EQ(result_out.summary, result.summary);
}

TEST(CatalogueWire, AnEmptySelectionIsTheDefault) {
  ViewSelection out;
  out.view_option = "left over";
  ASSERT_TRUE(decode_view_selection({}, out, nullptr));
  EXPECT_TRUE(out.view_option.empty());
  EXPECT_TRUE(out.active_toggles.empty());
}

// Every prefix of a valid message is refused, never read as something.
TEST(CatalogueWire, ACutShortMessageIsRefused) {
  const std::vector<uint8_t> whole = encode_catalogue(every_field_set());
  for (size_t length = 0; length < whole.size(); ++length) {
    const std::vector<uint8_t> part(whole.begin(), whole.begin() + length);
    CatalogueDataset out;
    std::string error;
    EXPECT_FALSE(decode_catalogue(part, out, &error)) << length;
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(out.items.empty());
  }
}

TEST(CatalogueWire, TrailingBytesAreRefused) {
  std::vector<uint8_t> bytes = encode_catalogue(every_field_set());
  bytes.push_back(0);
  CatalogueDataset out;
  EXPECT_FALSE(decode_catalogue(bytes, out, nullptr));
}

// A length the damage made huge is refused before anything is allocated for
// it.
TEST(CatalogueWire, AnImpossibleLengthIsRefused) {
  std::vector<uint8_t> bytes = encode_catalogue(CatalogueDataset{});
  // Header (4 magic + 4 version + 1 kind), then the column count.
  ASSERT_GT(bytes.size(), 13u);
  bytes[9] = bytes[10] = bytes[11] = bytes[12] = 0xff;
  CatalogueDataset out;
  std::string error;
  EXPECT_FALSE(decode_catalogue(bytes, out, &error));
  EXPECT_FALSE(error.empty());
}

TEST(CatalogueWire, AnotherVersionIsRefusedWithAReason) {
  std::vector<uint8_t> bytes = encode_catalogue(CatalogueDataset{});
  bytes[4] = static_cast<uint8_t>(kCatalogueWireVersion + 1);
  CatalogueDataset out;
  std::string error;
  EXPECT_FALSE(decode_catalogue(bytes, out, &error));
  EXPECT_NE(error.find("different releases"), std::string::npos) << error;
}

TEST(CatalogueWire, ASelectionIsNotReadAsACatalogue) {
  CatalogueDataset out;
  EXPECT_FALSE(decode_catalogue(encode_view_selection({}), out, nullptr));
  ViewSelection selection;
  EXPECT_FALSE(decode_view_selection(encode_catalogue(CatalogueDataset{}),
                                     selection, nullptr));
}

TEST(CatalogueWire, ForeignBytesAreRefused) {
  CatalogueDataset out;
  std::string error;
  EXPECT_FALSE(
      decode_catalogue({'h', 'e', 'l', 'l', 'o', 0, 0, 0, 0, 0}, out, &error));
  EXPECT_FALSE(error.empty());
}

}  // namespace
