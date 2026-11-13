/*
 * File:        sha256_hash.cpp
 * Module:      orc-plugin-teletext tests
 * Purpose:     SHA-256 digest computation for functional test goldens
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 *
 * Straightforward FIPS 180-4 SHA-256, written from the published pseudocode
 * (Wikipedia's "SHA-2" article reproduces it verbatim from the standard).
 * Verified against the NIST test vectors for "" and "abc" in
 * sha256_hash_test.cpp.
 */

#include "sha256_hash.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace orc {
namespace {

constexpr std::array<uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

class Sha256 {
 public:
  Sha256() = default;

  void update(const uint8_t* data, size_t len) {
    total_bits_ += static_cast<uint64_t>(len) * 8;
    while (len > 0) {
      const size_t take = std::min(len, sizeof(buffer_) - buffer_len_);
      std::memcpy(buffer_ + buffer_len_, data, take);
      buffer_len_ += take;
      data += take;
      len -= take;
      if (buffer_len_ == sizeof(buffer_)) {
        process_block(buffer_);
        buffer_len_ = 0;
      }
    }
  }

  std::array<uint8_t, 32> finish() {
    uint8_t pad[64] = {0x80};
    size_t pad_len = (buffer_len_ < 56) ? (56 - buffer_len_)
                                        : (120 - buffer_len_);
    update_no_count(pad, pad_len);

    uint8_t length_be[8];
    for (int i = 0; i < 8; ++i) {
      length_be[i] = static_cast<uint8_t>(total_bits_ >> (56 - 8 * i));
    }
    update_no_count(length_be, 8);

    std::array<uint8_t, 32> digest{};
    for (int i = 0; i < 8; ++i) {
      digest[4 * i + 0] = static_cast<uint8_t>(h_[i] >> 24);
      digest[4 * i + 1] = static_cast<uint8_t>(h_[i] >> 16);
      digest[4 * i + 2] = static_cast<uint8_t>(h_[i] >> 8);
      digest[4 * i + 3] = static_cast<uint8_t>(h_[i]);
    }
    return digest;
  }

 private:
  void update_no_count(const uint8_t* data, size_t len) {
    // Same as update() but the 0x80/zero padding bytes must not count
    // toward total_bits_, which was already fixed by the caller.
    while (len > 0) {
      const size_t take = std::min(len, sizeof(buffer_) - buffer_len_);
      std::memcpy(buffer_ + buffer_len_, data, take);
      buffer_len_ += take;
      data += take;
      len -= take;
      if (buffer_len_ == sizeof(buffer_)) {
        process_block(buffer_);
        buffer_len_ = 0;
      }
    }
  }

  void process_block(const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<uint32_t>(block[4 * i]) << 24) |
             (static_cast<uint32_t>(block[4 * i + 1]) << 16) |
             (static_cast<uint32_t>(block[4 * i + 2]) << 8) |
             static_cast<uint32_t>(block[4 * i + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 =
          rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 =
          rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
    uint32_t e = h_[4], f = h_[5], g = h_[6], h = h_[7];

    for (int i = 0; i < 64; ++i) {
      const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const uint32_t ch = (e & f) ^ (~e & g);
      const uint32_t temp1 = h + s1 + ch + kRoundConstants[i] + w[i];
      const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temp2 = s0 + maj;

      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    h_[0] += a;
    h_[1] += b;
    h_[2] += c;
    h_[3] += d;
    h_[4] += e;
    h_[5] += f;
    h_[6] += g;
    h_[7] += h;
  }

  std::array<uint32_t, 8> h_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                0xa54ff53a, 0x510e527f, 0x9b05688c,
                                0x1f83d9ab, 0x5be0cd19};
  uint8_t buffer_[64] = {};
  size_t buffer_len_ = 0;
  uint64_t total_bits_ = 0;
};

std::string to_hex(const std::array<uint8_t, 32>& digest) {
  static const char* kHexDigits = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (const uint8_t byte : digest) {
    out.push_back(kHexDigits[byte >> 4]);
    out.push_back(kHexDigits[byte & 0x0f]);
  }
  return out;
}

}  // namespace

std::string sha256_hex(std::string_view data) {
  Sha256 hasher;
  hasher.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
  return to_hex(hasher.finish());
}

std::string sha256_hex_of_file(const std::string& path, std::string* error) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    if (error) *error = "could not open " + path;
    return {};
  }

  Sha256 hasher;
  std::vector<char> chunk(1 << 16);
  while (file) {
    file.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const auto read = file.gcount();
    if (read > 0) {
      hasher.update(reinterpret_cast<const uint8_t*>(chunk.data()),
                    static_cast<size_t>(read));
    }
  }
  if (file.bad()) {
    if (error) *error = "read error on " + path;
    return {};
  }

  return to_hex(hasher.finish());
}

}  // namespace orc
