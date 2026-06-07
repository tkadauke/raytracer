#include <gtest/gtest.h>
#include "render/Object.h"
#include "render/State.h"

namespace StateTest {
  using namespace ::testing;
  using namespace render;

  TEST(State, ShouldInitialize) {
    State state;
    ASSERT_EQ(false, state.traceEvents);
    ASSERT_EQ(0, state.maxRecursionDepth);
  }

  TEST(State, ShouldNotTraceEventsByDefault) {
    State state;
    state.recordEvent(nullptr, "foo");
    ASSERT_EQ(nullptr, state.events.get());
  }

  TEST(State, ShouldStartTracing) {
    State state;
    state.startTrace();
    state.recordEvent(nullptr, "foo");
    ASSERT_EQ(1ul, state.events->size());
  }

  TEST(State, ShouldRecurse) {
    State state;
    state.recurseIn();
    ASSERT_EQ(1, state.recursionDepth);
    state.recurseOut();
    ASSERT_EQ(0, state.recursionDepth);
  }

  TEST(State, ShouldCountRays) {
    State state;
    state.recurseIn();
    ASSERT_EQ(1, state.numRays);
    state.recurseIn();
    ASSERT_EQ(2, state.numRays);
  }

  TEST(State, ShouldRecordHit) {
    State state;
    state.hit(nullptr, "Box");
    ASSERT_EQ(1, state.intersectionHits);
  }

  TEST(State, ShouldRecordMiss) {
    State state;
    state.miss(nullptr, "Box");
    ASSERT_EQ(1, state.intersectionMisses);
  }

  TEST(State, ShouldRecordPacketHitScalarFallbackReasons) {
    Object primitive;
    State state;

    state.packetHitScalarFallback(&primitive, "Primitive::intersectPacketHits");
    state.packetHitScalarFallback(&primitive, "Primitive::intersectPacketHits");
    state.packetHitScalarFallback(&primitive, "");

    EXPECT_EQ(3u, state.packetHitScalarFallbacks);
    EXPECT_EQ(2u, state.packetHitScalarFallbacksByReason.at("Primitive::intersectPacketHits"));
    EXPECT_EQ(1u, state.packetHitScalarFallbacksByReason.at("unknown"));
  }

  TEST(State, ShouldCloneForPathContinuationAfterCurrentRayExits) {
    Object primitive;
    State state;
    state.startTrace();
    state.recurseIn();
    state.recurseIn();
    state.hit(&primitive, "hit");
    state.shadowMiss(&primitive, "shadow");
    state.packetHitScalarFallback(&primitive, "packet");
    state.timeSample = 0.25;
    state.throughput = 0.5;

    State child = state.cloneForPathContinuation();
    state.recordEvent(nullptr, "parent-only");

    EXPECT_TRUE(child.traceEvents);
    ASSERT_NE(nullptr, child.events);
    EXPECT_EQ(3u, child.events->size());
    EXPECT_EQ(2, child.numRays);
    EXPECT_EQ(1, child.recursionDepth);
    EXPECT_EQ(2, child.maxRecursionDepth);
    EXPECT_EQ(1, child.intersectionHits);
    EXPECT_EQ(1, child.shadowIntersectionMisses);
    EXPECT_EQ(1u, child.packetHitScalarFallbacks);
    EXPECT_EQ(1u, child.packetHitScalarFallbacksByReason.at("packet"));
    EXPECT_DOUBLE_EQ(0.25, child.timeSample);
    EXPECT_DOUBLE_EQ(0.5, child.throughput);
  }
}
