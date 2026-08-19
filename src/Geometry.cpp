#include <chalkwalk/physical/Geometry.h>

namespace chalkwalk::physical {

bool isWaveguideGeometry(Geometry g) {
  return g == Geometry::String || g == Geometry::Tube;
}

Vec2 defaultPathPoint(float t) {
  // Default path runs along x at mid-y.
  Vec2 p;
  p.x = t;
  p.y = 0.5f;
  return p;
}

}  // namespace chalkwalk::physical
