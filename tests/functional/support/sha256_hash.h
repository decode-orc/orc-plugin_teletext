/*
 * File:        sha256_hash.h
 * Module:      orc-plugin-teletext tests
 * Purpose:     SHA-256 digest computation for functional test goldens
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 *
 * A self-contained replacement for decode-orc's orc-core-private
 * sha256_hash.h, which a plugin repository cannot include. Same interface,
 * independent implementation (FIPS 180-4).
 */

#pragma once

#include <string>
#include <string_view>

namespace orc {

// Compute the SHA-256 digest of an in-memory buffer.
// Returns 64 lowercase hexadecimal characters.
std::string sha256_hex(std::string_view data);

// Compute the SHA-256 digest of a file's contents, streaming so large
// capture files are not fully buffered in memory.
// Returns 64 lowercase hexadecimal characters, or an empty string on I/O
// failure (with a description in *error when provided).
std::string sha256_hex_of_file(const std::string& path,
                               std::string* error = nullptr);

}  // namespace orc
