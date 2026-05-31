#if defined(__linux__)

#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/Color.h"
#include "engine/raster/gl/Bindings.h"
#include "engine/raster/gl/EglContext.h"

namespace engine::raster::gl::tests {
  TEST(EglContext, ProbeAnswersAvailability) {
    // Mesa surfaceless EGL ships everywhere we test, but CI hosts
    // without libEGL or with a broken driver legitimately fail the
    // probe. The contract is that probe() returns the right
    // Availability — not that it always succeeds.
    const Availability info = EglContext::probe();
    if (!info.available()) {
      GTEST_SKIP() << "EGL surfaceless context unavailable on this host: " << info.error();
    }
    EXPECT_FALSE(info.detail().empty());
  }

  TEST(EglContext, CreatesContextAndAllocatesFbo) {
    EglContext context;
    if (!context.create(64, 48, 0)) {
      GTEST_SKIP() << "EGL surfaceless context unavailable on this host: "
                   << context.errorMessage();
    }
    EXPECT_TRUE(context.isValid());
  }

  TEST(EglContext, ReadsBackClearedColorBuffer) {
    EglContext context;
    if (!context.create(8, 8, 0)) {
      GTEST_SKIP() << "EGL surfaceless context unavailable on this host: "
                   << context.errorMessage();
    }
    ASSERT_TRUE(context.makeCurrent());
    ASSERT_TRUE(context.bindFramebuffer());
    glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Buffer<Colord> buffer(8, 8);
    context.copyColorTo(buffer);
    context.releaseFramebuffer();
    context.doneCurrent();

    EXPECT_NEAR(0.2, buffer[0][0].r(), 1.0 / 255.0);
    EXPECT_NEAR(0.4, buffer[0][0].g(), 1.0 / 255.0);
    EXPECT_NEAR(0.6, buffer[0][0].b(), 1.0 / 255.0);
  }

  TEST(EglContext, MakeCurrentMigrateDetachReroundtripWorks) {
    EglContext context;
    if (!context.create(4, 4, 0)) {
      GTEST_SKIP() << "EGL surfaceless context unavailable on this host: "
                   << context.errorMessage();
    }
    ASSERT_TRUE(context.makeCurrent());
    context.doneCurrent();
    context.detachFromCurrentThread();
    EXPECT_TRUE(context.migrateToCurrentThread());
    EXPECT_TRUE(context.makeCurrent());
    context.doneCurrent();
  }
}

#endif // __linux__
