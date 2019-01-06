#include "gtest.h"
#include "raytracer/State.h"

namespace StateTest {
  using namespace ::testing;
  using namespace raytracer;

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
}
