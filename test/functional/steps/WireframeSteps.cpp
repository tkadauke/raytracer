#include "test/functional/support/GivenWhenThen.h"
#include "test/functional/support/WireframeFeatureTest.h"

using namespace testing;

GIVEN(EngineFeatureTest, "a wireframe lod of ([0-9]+)") {
  // Wireframe-only tuning step. Other engine fixtures don't have a
  // tessellation LOD knob, so the dynamic_cast no-ops there rather
  // than failing — using this step in a non-wireframe scenario is a
  // no-op, not an error.
  if (auto* wireframe = dynamic_cast<WireframeFeatureTest*>(test)) {
    wireframe->setLod(std::stoi(match[1]));
  }
}
