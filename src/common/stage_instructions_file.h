/*
 * File:        stage_instructions_file.h
 * Module:      orc-stage-plugin-teletext
 * Purpose:     Per-stage instructions.md lookup for a two-stage plugin binary
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

// The SDK's ORC_STAGE_INSTRUCTIONS_MD reads one file per shared library,
// <library base name>.md. This plugin registers two stages from one library,
// so each stage reads its own file instead:
//
//   <library base name>.<stage name>.md
//   e.g. liborc-stage-plugin-teletext.teletext_sink.md
//
// CMakeLists.txt copies src/<stage>/instructions.md to that name beside the
// built and the installed library.

#include <fstream>
#include <sstream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace orc::plugins::teletext {

/// Reads <dir of this library>/<library base name>.<stage_name>.md.
/// plugin_symbol must be an address inside this library. Returns an empty
/// string when the library path or the file cannot be found.
inline std::string read_stage_instructions_file(const void* plugin_symbol,
                                                const char* stage_name) {
  std::string lib_path;

#if defined(__unix__) || defined(__APPLE__)
  ::Dl_info info{};
  if (::dladdr(plugin_symbol, &info) != 0 && info.dli_fname != nullptr) {
    lib_path = info.dli_fname;
  }
#elif defined(_WIN32)
  HMODULE hmod = nullptr;
  if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(plugin_symbol), &hmod) &&
      hmod != nullptr) {
    char buf[MAX_PATH] = {};
    if (::GetModuleFileNameA(hmod, buf, MAX_PATH) > 0) {
      lib_path = buf;
    }
  }
#endif

  if (lib_path.empty()) return {};

  const auto slash = lib_path.find_last_of("/\\");
  const auto dot = lib_path.rfind('.');
  const std::string base =
      (dot != std::string::npos && (slash == std::string::npos || dot > slash))
          ? lib_path.substr(0, dot)
          : lib_path;

  std::ifstream f(base + "." + stage_name + ".md");
  if (!f.is_open()) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace orc::plugins::teletext

/// Class-body replacement for ORC_STAGE_INSTRUCTIONS_MD in this plugin.
// clang-format off
#define ORC_TELETEXT_STAGE_INSTRUCTIONS_MD(stage_name)                    \
  std::string get_instructions() const override {                        \
    static const char kSentinel_ = '\0';                                 \
    return ::orc::plugins::teletext::read_stage_instructions_file(       \
        static_cast<const void*>(&kSentinel_), stage_name);              \
  }
// clang-format on
