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
    state.recordEvent("foo");
    ASSERT_EQ(nullptr, state.events.get());
  }
  
  TEST(State, ShouldStartTracing) {
    State state;
    state.startTrace();
    state.recordEvent("foo");
    ASSERT_EQ(1ul, state.events->size());
  }
  
  TEST(State, ShouldRecurse) {
    State state;
    state.recurseIn();
    ASSERT_EQ(1, state.recursionDepth);
    state.recurseOut();
    ASSERT_EQ(0, state.recursionDepth);
  }
  
  TEST(State, ShouldRecordHit) {
    State state;
    state.hit("Box");
    ASSERT_EQ(1, state.intersectionHits);
  }
  
  TEST(State, ShouldRecordMiss) {
    State state;
    state.miss("Box");
    ASSERT_EQ(1, state.intersectionMisses);
  }
}
