#pragma once

namespace chalkwalk::physical {

enum class Geometry {
  String   = 0,  // 1D waveguide
  Tube     = 1,  // 1D waveguide
  Plate    = 2,  // 2D modal
  Membrane = 3,  // 2D modal
};

// Returns true if the geometry is implemented via a 1D waveguide
// (harmonic structure, spec §4.1) rather than a modal bank (§4.2).
bool isWaveguideGeometry(Geometry g);

// Spatial vector path p(η) sample for a normalised position t ∈ [0,1].
// Returns (x, y) in 0..1 space. Skeleton: linear interpolation along a
// fixed default path; the real Workbench will let the user place
// p_start and p_end on a 2D pad.
struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

Vec2 defaultPathPoint(float t);

}  // namespace chalkwalk::physical
