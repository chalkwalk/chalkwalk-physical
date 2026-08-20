// SPDX-License-Identifier: MIT
// Part of chalkwalk-physical. See LICENSE.
#pragma once

// Mathematical constants for the models here, in one place.
//
// Adopted from Anvil, where this file was written to stop pi being respelled
// per translation unit. The three spellings it replaced in this library were
// not merely untidy: the waveguide's tuning used a seven-digit 3.14159265f
// while the exciters used the full float value, so the same quantity was
// slightly different depending on which file computed it. That is invisible
// until something is measured to a fraction of a cent, and this library's
// tuning tests measure exactly that.

namespace chalkwalk::physical {

inline constexpr float kPi = 3.14159265358979f;
inline constexpr float kTwoPi = 6.28318530718f;

}  // namespace chalkwalk::physical
