/*
 * File:        plugin_build_info.cpp
 * Module:      orc-plugin-build-info
 * Purpose:     Emit the build facts a release manifest must declare
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

// Every plugin release ships an orc-plugin-manifest.yaml declaring each
// binary's platform, host ABI, toolchain tag and SHA-256 digest (see
// docs/technical/plugin-publishing.md in decode-orc). The ABI number and
// toolchain tag are properties of the compiler that built the plugin, so this
// executable is compiled with the same toolchain and SDK headers as the plugin
// and prints the descriptor's own values; scripts/package_local.sh turns them
// into the manifest.

#include <orc/view/orc_view_plugin_abi.h>

#include <cstdio>

#include "plugin.h"

int main() {
  const auto& descriptor = orc::plugins::teletext::kPluginDescriptor;

  std::printf("plugin_id=%s\n", descriptor.plugin_id);
  std::printf("plugin_version=%s\n", descriptor.plugin_version);
  std::printf("stage_name=%s\n", orc::plugins::teletext::kAssetName);
  std::printf("abi=%u\n", descriptor.host_abi_version);
  std::printf("api=%u\n", descriptor.plugin_api_version);
  std::printf("toolchain_tag=%s\n", descriptor.toolchain_tag);
  std::printf("license_spdx=%s\n", descriptor.license_spdx);
#if defined(ORC_VIEW_QT_VERSION)
  // Only when the view binary is built: package_local.sh adds its manifest
  // entry from these.
  std::printf("view_abi=%u\n", orc::view::kViewPluginAbiVersion);
  std::printf("view_qt=%s\n", ORC_VIEW_QT_VERSION);
#endif

  return 0;
}
