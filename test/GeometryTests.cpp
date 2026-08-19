#include <catch2/catch_test_macros.hpp>

#include <chalkwalk/physical/Geometry.h>

using namespace chalkwalk::physical;

// Which branch a geometry takes is a dispatch decision made in VoiceManager;
// getting it wrong routes a drum head through a string model.
TEST_CASE("geometries route to the right synthesis branch", "[geometry]") {
  REQUIRE(isWaveguideGeometry(Geometry::String));
  REQUIRE(isWaveguideGeometry(Geometry::Tube));
  REQUIRE_FALSE(isWaveguideGeometry(Geometry::Plate));
  REQUIRE_FALSE(isWaveguideGeometry(Geometry::Membrane));
}

TEST_CASE("the default path spans x at mid height", "[geometry]") {
  REQUIRE(defaultPathPoint(0.0f).x == 0.0f);
  REQUIRE(defaultPathPoint(1.0f).x == 1.0f);
  REQUIRE(defaultPathPoint(0.5f).x == 0.5f);
  for (float t : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    REQUIRE(defaultPathPoint(t).y == 0.5f);
}
