/*
 * File:        plugin.h
 * Module:      orc-stage-plugin-teletext
 * Purpose:     Plugin descriptor for the teletext/NABTS stage plugin
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <orc/abi/orc_plugin_sdk.h>

#ifndef ORC_STAGE_PLUGIN_VERSION
#define ORC_STAGE_PLUGIN_VERSION "dev"
#endif

// ORC_TELETEXT_CORE_PLUGIN is set by the ORC_TELETEXT_AS_CORE_PLUGIN CMake
// option. decode-orc sets it when it builds this repository as a bundled
// submodule; release assets for the Plugin Manager leave it unset.
#ifndef ORC_TELETEXT_CORE_PLUGIN
#define ORC_TELETEXT_CORE_PLUGIN 0
#endif

namespace orc::plugins::teletext {

// Release-asset name token: orc-plugin_<kAssetName>_<platform>_abi<N>.<ext>.
// The binary registers two stages, so the token names the plugin, not a stage.
inline constexpr const char* kAssetName = "teletext";

inline constexpr orc::StagePluginDescriptor kPluginDescriptor =
    ORC_STAGE_PLUGIN_DESCRIPTOR("org.decodeorc.stage.teletext",
                                ORC_STAGE_PLUGIN_VERSION, "GPL-3.0-or-later",
                                ORC_TELETEXT_CORE_PLUGIN != 0);

}  // namespace orc::plugins::teletext
