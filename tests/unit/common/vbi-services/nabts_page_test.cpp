/*
 * File:        nabts_page_test.cpp
 * Module:      orc-core-tests
 * Purpose:     The NAPLPS default colour map against ITU-T T.101 Table II-3
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vbi-services/nabts_page.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

namespace orc {
namespace {

/**
 * @brief The whole of T.101 Table II-3's "Default colour map - Data Syntax III"
 *
 * Transcribed as G, R, B — the order X3.110 §5.3.1 specifies and Figure 12
 * draws, and the order the table's own columns are in. (The markdown extraction
 * of the table labels those columns "R G B"; that is a mis-extraction, and this
 * test is what proves it. Reading them as R G B makes entry 9 a green-dominant
 * colour 45 degrees from blue on a hue circle where green sits at 240 — which
 * the algorithm of §5.3.2.5.2 cannot produce. Reading them as G R B reproduces
 * every entry exactly.)
 */
struct MapEntry {
  uint8_t green;
  uint8_t red;
  uint8_t blue;
};

// The table as T.101 prints it: three bits a gun.
constexpr MapEntry kTableII3[kNabtsColourMapEntries] = {
    // The uniform grey ramp of the low half, G = R = B.
    {0, 0, 0},  // 0  nominal black
    {1, 1, 1},  // 1
    {2, 2, 2},  // 2
    {3, 3, 3},  // 3  grey
    {4, 4, 4},  // 4
    {5, 5, 5},  // 5
    {6, 6, 6},  // 6
    {7, 7, 7},  // 7  nominal white
    // Eight hues equally spaced around the hue circle, starting at blue.
    {0, 0, 7},  // 8   0 degrees — blue
    {0, 5, 7},  // 9   45
    {0, 7, 4},  // 10  90
    {2, 7, 0},  // 11  135
    {7, 7, 0},  // 12  180 — yellow
    {7, 2, 0},  // 13  225
    {7, 0, 4},  // 14  270
    {5, 0, 7},  // 15  315
};

constexpr int kTableGunMax = 7;

// The map holds four bits a gun (nabts_page.h), where the table prints three.
// Brought down to three bits it is the table, entry for entry. Of the four bits:
// a gun the table has off or full is off or full; the grey ramp is the table's
// ramp spaced uniformly over fifteen; and a hue's partial gun, which the
// algorithm of §5.3.2.5.2 computes afresh at four bits rather than scaling the
// table (90 degrees puts blue at 3.5 of 7, printed 4, and 7.5 of 15, held as
// 8), is within a step of the table's value scaled.
TEST(NabtsDefaultColourMap, MatchesTableII3AtFourBitsAGun) {
  NabtsColour map[kNabtsColourMapEntries];
  nabts_default_colour_map(map);

  const auto to_three_bits = [](uint8_t gun) {
    return static_cast<int>(std::lround(static_cast<double>(gun) *
                                        kTableGunMax / kNabtsGunMax));
  };
  const auto scaled = [](uint8_t gun) {
    return static_cast<double>(gun) * kNabtsGunMax / kTableGunMax;
  };
  for (size_t i = 0; i < kNabtsColourMapEntries; ++i) {
    const uint8_t held[3] = {map[i].green, map[i].red, map[i].blue};
    const uint8_t table[3] = {kTableII3[i].green, kTableII3[i].red,
                              kTableII3[i].blue};
    const char* const gun_name[3] = {"green", "red", "blue"};
    for (int g = 0; g < 3; ++g) {
      EXPECT_EQ(to_three_bits(held[g]), table[g])
          << "entry " << i << " " << gun_name[g];
      if (table[g] == 0 || table[g] == kTableGunMax) {
        EXPECT_EQ(held[g], table[g] == 0 ? 0 : kNabtsGunMax)
            << "entry " << i << " " << gun_name[g];
      } else if (i < kNabtsColourMapEntries / 2) {
        EXPECT_EQ(held[g], std::lround(scaled(table[g])))
            << "entry " << i << " " << gun_name[g];
      } else {
        EXPECT_LE(std::fabs(held[g] - scaled(table[g])), 1.0)
            << "entry " << i << " " << gun_name[g];
      }
    }
    EXPECT_FALSE(map[i].transparent) << "entry " << i;
  }
}

// CEA-516 §8.6.3 has services choose from 4096 colours, four bits a gun, and
// Table D1 item 5(4)'s "set of 512 obtained by allocating three bits each to
// G R & B" is the least a receiver may hold.
TEST(NabtsDefaultColourMap, UsesFourBitsAGun) {
  NabtsColour map[kNabtsColourMapEntries];
  nabts_default_colour_map(map);
  for (size_t i = 0; i < kNabtsColourMapEntries; ++i) {
    EXPECT_LE(map[i].green, kNabtsGunMax) << "entry " << i;
    EXPECT_LE(map[i].red, kNabtsGunMax) << "entry " << i;
    EXPECT_LE(map[i].blue, kNabtsGunMax) << "entry " << i;
  }
}

// §5.3.2.5.2: "The first half of the default color map is used to store a
// complete, uniformly spaced grey scale. This comprises the ordered set of
// colors where G = R = B" — and it runs from black to white inclusive.
TEST(NabtsDefaultColourMap, TheLowHalfIsAGreyRampFromBlackToWhite) {
  NabtsColour map[kNabtsColourMapEntries];
  nabts_default_colour_map(map);

  EXPECT_EQ(map[0], kNabtsNominalBlack);
  EXPECT_EQ(map[7], kNabtsNominalWhite);
  for (size_t i = 0; i < 8; ++i) {
    EXPECT_EQ(map[i].green, map[i].red) << "entry " << i;
    EXPECT_EQ(map[i].green, map[i].blue) << "entry " << i;
    if (i > 0) {
      EXPECT_GT(map[i].green, map[i - 1].green) << "entry " << i;
    }
  }
}

// §5.3.2.5.2 places blue at 0 degrees, red at 120 and green at 240, and the
// eight hues start at 0 and proceed round the circle — so the three pure
// primaries fall on entries 8, 12 - 4 and 12 + ... in other words at the
// multiples of 120 degrees, which with 45-degree steps means blue exactly and
// the other two only approximately. What is exact is that each hue has one gun
// full on and one full off.
TEST(NabtsDefaultColourMap, EveryHueHasOneGunFullOnAndOneFullOff) {
  NabtsColour map[kNabtsColourMapEntries];
  nabts_default_colour_map(map);

  for (size_t i = 8; i < kNabtsColourMapEntries; ++i) {
    const uint8_t guns[3] = {map[i].green, map[i].red, map[i].blue};
    int full_on = 0;
    int full_off = 0;
    for (const uint8_t gun : guns) {
      full_on += (gun == kNabtsGunMax) ? 1 : 0;
      full_off += (gun == 0) ? 1 : 0;
    }
    EXPECT_GE(full_on, 1) << "entry " << i << " has no primary at full";
    EXPECT_GE(full_off, 1) << "entry " << i << " has no primary off";
  }

  // Entry 8 is 0 degrees, which is pure blue.
  EXPECT_EQ(map[8].blue, kNabtsGunMax);
  EXPECT_EQ(map[8].green, 0u);
  EXPECT_EQ(map[8].red, 0u);
  // Entry 12 is 180 degrees, equidistant from red and green: both full on.
  EXPECT_EQ(map[12].green, kNabtsGunMax);
  EXPECT_EQ(map[12].red, kNabtsGunMax);
  EXPECT_EQ(map[12].blue, 0u);
}

TEST(NabtsPageSnapshot, IsEmptyBeforeAnythingIsDecoded) {
  const NabtsPageSnapshot snapshot;
  EXPECT_TRUE(snapshot.empty());
  EXPECT_TRUE(snapshot.primitives.empty());
  EXPECT_TRUE(snapshot.drcs.empty());
  for (const NabtsTextureMask& mask : snapshot.texture_masks) {
    EXPECT_FALSE(mask.defined());
  }
}

// Table D1 item 10: "The full X dimension (width) of the unit screen, and the Y
// dimension (height) from 0.0 to 0.78125, shall be visible in the display
// area."
TEST(NabtsPageSnapshot, TheDisplayAreaIsTheLowerFractionOfTheUnitScreen) {
  EXPECT_DOUBLE_EQ(kNabtsDisplayAreaHeight, 0.78125);
  EXPECT_LT(kNabtsDisplayAreaHeight, 1.0);
}

// A non-spacing mark is composed onto the next character deposited into its
// field (X3.110 §7.2): the same origin, size and rotation, to a millionth of
// the unit screen, and nothing else.
TEST(NabtsPageTest, TheSameCharacterFieldIsItsOriginSizeAndRotation) {
  NabtsPrimitive mark;
  mark.origin = {0.25, 0.5};
  mark.size = {0.025, 0.04};
  NabtsPrimitive letter = mark;
  EXPECT_TRUE(nabts_same_character_field(mark, letter));

  letter.origin.x += 1e-7;
  EXPECT_TRUE(nabts_same_character_field(mark, letter))
      << "rounding a relative coordinate";
  letter.origin.x = mark.origin.x + 1.0 / 768.0;
  EXPECT_FALSE(nabts_same_character_field(mark, letter)) << "the next cell";

  letter = mark;
  letter.size.dy *= 2.0;
  EXPECT_FALSE(nabts_same_character_field(mark, letter)) << "another size";

  letter = mark;
  letter.rotation = NabtsCharRotation::k90;
  EXPECT_FALSE(nabts_same_character_field(mark, letter)) << "another rotation";
}

}  // namespace
}  // namespace orc
