/*
 * File:        sha256_hash_test.cpp
 * Module:      orc-plugin-teletext tests
 * Purpose:     NIST test vectors for the local SHA-256 implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "sha256_hash.h"

#include <gtest/gtest.h>

namespace orc {
namespace {

TEST(Sha256Hash, EmptyString) {
  EXPECT_EQ(
      sha256_hex(""),
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Hash, Abc) {
  EXPECT_EQ(
      sha256_hex("abc"),
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Hash, LongerMessage) {
  EXPECT_EQ(
      sha256_hex(
          "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

}  // namespace
}  // namespace orc
