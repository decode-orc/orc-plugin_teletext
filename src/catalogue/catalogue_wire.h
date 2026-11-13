/*
 * File:        catalogue_wire.h
 * Module:      orc-plugin_teletext (shared by the stage and view binaries)
 * Purpose:     The bytes the stage and its view exchange through decode-orc's
 *              stage tool session: the catalogue view's contract, verbs and
 *              byte format
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ORC_TELETEXT_CATALOGUE_WIRE_H
#define ORC_TELETEXT_CATALOGUE_WIRE_H

// Plugin-private. decode-orc carries these bytes between the stage binary and
// the view binary without reading them (IStageToolSession on the view side,
// IStageToolRequestHandler on the stage side), so the format is this plugin's
// own business. Both binaries are built from one checkout and released
// together, but a reader may still pair a view with a stage from another
// release for a moment (an update half done), so every message starts with a
// magic and a version and a mismatch is refused with a message, never read.

#include <cstdint>
#include <string>
#include <vector>

#include "catalogue/catalogue_model.h"

namespace orc {
namespace teletext_wire {

/// The catalogue viewer's tool contract. Both stages declare it, and the view
/// binary registers its window for it.
inline constexpr const char* kCatalogueViewContractId =
    "org.decodeorc.teletext.catalogue-view.v1";

/// Ask for the catalogue. Payload: encode_view_selection(). Reply:
/// encode_catalogue(), or ok = false with a message when there is nothing to
/// show.
inline constexpr const char* kCatalogueVerb = "catalogue";

/// Play keys on an item that takes them (CatalogueItem::keys). Payload:
/// encode_play_request(). Reply: encode_payload(), the item as those keys
/// leave it, drawn under the request's selection.
inline constexpr const char* kPlayedVerb = "played";

/// Write everything the last trigger decoded under a folder. Payload:
/// encode_export_request(). Reply: encode_export_result(). Runs on the host's
/// render worker; nothing is decoded again.
inline constexpr const char* kExportVerb = "export";

/// The error a stage answers kCatalogueVerb with before it has been
/// triggered. Opening the tool only ever reads: deciding when to spend the
/// decode is what Trigger Stage is for.
inline constexpr const char* kNotTriggeredMessage =
    "This stage has not been triggered yet, so there are no results to "
    "show.\n\nRight-click the stage and choose Trigger Stage to produce "
    "them.";

/// Bumped whenever a field is added, removed or reordered.
inline constexpr uint32_t kCatalogueWireVersion = 1;

/// How the reader asked for the catalogue to be presented: the view option's
/// id (empty for the settings' own) and the ids of the toggles that are on.
struct ViewSelection {
  std::string view_option;
  std::vector<std::string> active_toggles;
};

/// What kPlayedVerb asks: every key pressed on @p item_id since it was shown,
/// in order, under the presentation on show.
struct PlayRequest {
  std::string item_id;
  std::vector<std::string> keys;
  ViewSelection selection;
};

/// What kExportVerb asks: the folder to write under, which the stage owns.
struct ExportRequest {
  std::string directory;
};

std::vector<uint8_t> encode_view_selection(const ViewSelection& selection);

/// False, with @p error set, when the bytes are not a selection of this
/// version. An empty payload is the default selection.
bool decode_view_selection(const std::vector<uint8_t>& bytes,
                           ViewSelection& selection, std::string* error);

std::vector<uint8_t> encode_catalogue(const CatalogueDataset& dataset);

/// False, with @p error set, when the bytes are not a catalogue of this
/// version or are cut short; @p dataset is then left empty.
bool decode_catalogue(const std::vector<uint8_t>& bytes,
                      CatalogueDataset& dataset, std::string* error);

// The other messages, on the same terms: false with @p error set, and the
// value left at its default, for bytes that are not that message.

std::vector<uint8_t> encode_play_request(const PlayRequest& request);
bool decode_play_request(const std::vector<uint8_t>& bytes,
                         PlayRequest& request, std::string* error);

std::vector<uint8_t> encode_payload(const CataloguePayload& payload);
bool decode_payload(const std::vector<uint8_t>& bytes,
                    CataloguePayload& payload, std::string* error);

std::vector<uint8_t> encode_export_request(const ExportRequest& request);
bool decode_export_request(const std::vector<uint8_t>& bytes,
                           ExportRequest& request, std::string* error);

std::vector<uint8_t> encode_export_result(const CatalogueExportResult& result);
bool decode_export_result(const std::vector<uint8_t>& bytes,
                          CatalogueExportResult& result, std::string* error);

}  // namespace teletext_wire
}  // namespace orc

#endif  // ORC_TELETEXT_CATALOGUE_WIRE_H
