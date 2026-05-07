#include <gtest/gtest.h>

#include "world/objects/ScriptedSurface.h"

// ScriptedSurface.cpp uses #include "ScriptedSurface.moc" (inline moc) to
// compile ScriptElementRegistry, so there is no separate
// moc_ScriptedSurface.cpp in the raytracer build tree. Including
// ScriptedSurface.h here without SKIP_AUTOMOC would cause CMake AUTOMOC to
// generate one for unit_tests, which then duplicates the ScriptedSurface
// symbols already pulled in via --whole-archive. This file has SKIP_AUTOMOC
// set in test/CMakeLists.txt to avoid that collision.

namespace ScriptedSurfaceTest {
  TEST(ScriptedSurface, ShouldDefaultToEmptyScriptName) {
    ScriptedSurface s;
    EXPECT_TRUE(s.scriptName().isEmpty());
  }

  TEST(ScriptedSurface, ShouldDefaultToNotGeneratedAndVisible) {
    // Inherits Surface defaults: visible=true, material=nullptr.
    ScriptedSurface s;
    EXPECT_TRUE(s.visible());
    EXPECT_EQ(nullptr, s.material());
  }
}
