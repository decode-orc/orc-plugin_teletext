/*
 * File:        teletext_view_plugin.cpp
 * Module:      orc-view_teletext
 * Purpose:     The plugin's view binary: opens the catalogue browser for the
 *              Teletext Pages and NABTS Records tools and feeds it from the
 *              stage through the tool session
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

// orc-gui loads this library; orc-cli never does. It reaches the stage only
// through the IStageToolSession it is opened with, and only bytes cross:
// src/catalogue/catalogue_wire.h is the whole of what the two halves share.

#include <orc/view/orc_view_registration.h>

#include <memory>
#include <string>
#include <utility>

#include "catalogue/catalogue_wire.h"
#include "catalogue_session_controller.h"
#include "cataloguedialog.h"

namespace {

QWidget* make_catalogue_view(const orc::view::ViewOpenRequest& request) {
  if (!request.session) {
    return nullptr;
  }
  auto* dialog = new CatalogueDialog(request.parent);
  // Parented to the dialog, so it goes when the dialog does; no reply arrives
  // after that, because the host closes the session first.
  new CatalogueSessionController(dialog, request.session);
  return dialog;
}

constexpr orc::view::ViewPluginDescriptor kViewDescriptor =
    ORC_VIEW_PLUGIN_DESCRIPTOR("org.decodeorc.stage.teletext",
                               ORC_VIEW_PLUGIN_VERSION);

}  // namespace

ORC_DEFINE_VIEW_PLUGIN(kViewDescriptor,
                       {orc::teletext_wire::kCatalogueViewContractId,
                        &make_catalogue_view})
