#include <gtest/gtest.h>

#include "world/objects/Element.h"
#include "world/objects/ElementFactory.h"

#include <QVariant>

#include <memory>

namespace ScriptedSurfaceTest {
  std::unique_ptr<Element> createScriptedSurface() {
    auto surface = ElementFactory::self().create("ScriptedSurface");
    EXPECT_NE(nullptr, surface.get());
    return surface;
  }

  TEST(ScriptedSurface, ShouldDefaultToEmptyScriptName) {
    const auto surface = createScriptedSurface();
    ASSERT_NE(nullptr, surface.get());
    EXPECT_TRUE(surface->property("scriptName").toString().isEmpty());
  }

  TEST(ScriptedSurface, ShouldDefaultToVisibleAndNotGenerated) {
    const auto surface = createScriptedSurface();
    ASSERT_NE(nullptr, surface.get());
    EXPECT_TRUE(surface->property("visible").toBool());
    EXPECT_FALSE(surface->isGenerated());
  }
}
