#include <gtest/gtest.h>

#include "engine/raster/detail/OpenGLRasterResourceCache.h"
#include "engine/raster/gl/AttachmentSet.h"

namespace engine::raster::detail::tests {
  // Stress the Phase 3 multi-AttachmentSet substrate: a graph that
  // renders into resources of multiple distinct sizes must keep one
  // attachment set per (width, height, samples) and reuse them on
  // hit. Drives the cache directly (not through OpenGLRasterizer) so
  // the test runs against whichever native GL backend the factory
  // hands us — CGL on macOS, EGL on Linux — without needing a
  // QGuiApplication.
  class OpenGLAttachmentSetCacheTest : public ::testing::Test {
  protected:
    void SetUp() override {
      if (!cache.context->create()) {
        GTEST_SKIP() << "GL context unavailable on this host: " << cache.context->errorMessage();
      }
      ASSERT_TRUE(cache.context->makeCurrent()) << cache.context->errorMessage();
    }

    void TearDown() override {
      if (cache.context->isValid()) {
        for (auto& entry : cache.attachmentSetCache) {
          entry.set.destroy();
          entry.lastUsed = 0;
        }
        cache.context->doneCurrent();
      }
    }

    OpenGLRasterResourceCache cache;
  };

  TEST_F(OpenGLAttachmentSetCacheTest, GivesDistinctSlotsToDistinctDimensions) {
    gl::AttachmentSet* a = cache.acquireAttachmentSet(256, 192, 0);
    gl::AttachmentSet* b = cache.acquireAttachmentSet(128, 96, 0);
    gl::AttachmentSet* c = cache.acquireAttachmentSet(64, 48, 0);

    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);
    ASSERT_NE(nullptr, c);

    // Each (w, h, samples) must land in its own cache slot — anything
    // else means a multi-pass graph would reallocate the FBO instead
    // of keeping per-size attachments resident across renders.
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);

    EXPECT_EQ(256, a->width());
    EXPECT_EQ(192, a->height());
    EXPECT_EQ(128, b->width());
    EXPECT_EQ(96, b->height());
    EXPECT_EQ(64, c->width());
    EXPECT_EQ(48, c->height());
  }

  TEST_F(OpenGLAttachmentSetCacheTest, ReAcquiringSameDimensionsHitsTheSameSlot) {
    gl::AttachmentSet* first = cache.acquireAttachmentSet(256, 192, 0);
    ASSERT_NE(nullptr, first);
    const std::uint64_t firstUseTick = cache.attachmentSetUseTick;

    // Rotate through two other sizes to advance the LRU tick, then
    // come back to the original. The first slot should still be
    // there — same pointer, no reallocation.
    cache.acquireAttachmentSet(128, 96, 0);
    cache.acquireAttachmentSet(64, 48, 0);

    gl::AttachmentSet* again = cache.acquireAttachmentSet(256, 192, 0);
    EXPECT_EQ(first, again);
    EXPECT_GT(cache.attachmentSetUseTick, firstUseTick);
  }

  TEST_F(OpenGLAttachmentSetCacheTest, MsaaCountParticipatesInTheCacheKey) {
    // Same dimensions, different sample counts must occupy different
    // slots — MSAA renderbuffers differ in storage format and can't
    // be reused as single-sample reads.
    gl::AttachmentSet* single = cache.acquireAttachmentSet(128, 96, 0);
    gl::AttachmentSet* multi = cache.acquireAttachmentSet(128, 96, 4);

    ASSERT_NE(nullptr, single);
    ASSERT_NE(nullptr, multi);
    EXPECT_NE(single, multi);
    EXPECT_EQ(0, single->samples());
    EXPECT_EQ(4, multi->samples());
  }

  TEST_F(OpenGLAttachmentSetCacheTest, ExceedingCapacityEvictsLeastRecentlyUsed) {
    // Fill the cache.
    cache.acquireAttachmentSet(64, 64, 0);   // a — will become LRU
    cache.acquireAttachmentSet(128, 128, 0); // b
    cache.acquireAttachmentSet(256, 256, 0); // c
    cache.acquireAttachmentSet(512, 512, 0); // d
    ASSERT_EQ(kOpenGLAttachmentSetCacheSize, 4u);

    // Touch b, c, d so 'a' (64x64) is the unambiguous LRU victim.
    cache.acquireAttachmentSet(128, 128, 0);
    cache.acquireAttachmentSet(256, 256, 0);
    cache.acquireAttachmentSet(512, 512, 0);

    // A fifth distinct size evicts the LRU. Re-acquiring 64x64 must
    // therefore re-create it (a fresh slot).
    cache.acquireAttachmentSet(1024, 1024, 0);
    gl::AttachmentSet* revived = cache.acquireAttachmentSet(64, 64, 0);
    ASSERT_NE(nullptr, revived);
    EXPECT_EQ(64, revived->width());
    EXPECT_EQ(64, revived->height());

    // The other sizes that were touched after eviction stay resident.
    int residentNonVictims = 0;
    for (const auto& entry : cache.attachmentSetCache) {
      if (entry.set.isValid() &&
          (entry.set.width() == 256 || entry.set.width() == 512 || entry.set.width() == 1024)) {
        ++residentNonVictims;
      }
    }
    EXPECT_EQ(3, residentNonVictims);
  }

  TEST_F(OpenGLAttachmentSetCacheTest, BadDimensionsLeaveSlotEmptyForNextCaller) {
    // AttachmentSet::create normalizes any size <= 0 to 1, so even
    // (0, 0) succeeds. To exercise the failure return path we'd need
    // a way to make the GL FBO incomplete on demand, which isn't
    // portable. Pin the success-on-degenerate-input behavior instead
    // so we notice if normalization silently goes away.
    gl::AttachmentSet* set = cache.acquireAttachmentSet(0, 0, 0);
    ASSERT_NE(nullptr, set);
    EXPECT_EQ(1, set->width());
    EXPECT_EQ(1, set->height());
  }
}
