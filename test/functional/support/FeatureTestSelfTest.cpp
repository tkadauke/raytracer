// Self-tests for the cucumber-style step framework in
// `test/functional/support/FeatureTest.h`. These pin the
// regex-dispatch and hard-fail invariants the rest of the functional
// suite relies on — without them, framework regressions would surface
// only as silent green tests in user code.

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#include "test/functional/support/FeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include <functional>
#include <string>

namespace FeatureTestSelfTestNs {
  // Two fixtures with isolated step registries — `FeatureTest<T>`
  // keeps its `Steps` singleton parameterised by `T`, so each fixture
  // class has its own pattern table. Putting parameterised /
  // missing-step / ambiguous-step concerns in separate fixtures keeps
  // each scenario from polluting the others' table.

  class FeatureTestSelfTest : public ::testing::FeatureTest<FeatureTestSelfTest> {
  public:
    int sawRadius{0};
    int sawTwoArgsA{0};
    int sawTwoArgsB{0};
    bool sawCentered{false};
  };

  class FeatureTestAmbiguityTest : public ::testing::FeatureTest<FeatureTestAmbiguityTest> {
  };

  // Capture a fatal failure raised inside `fn` and return its
  // message. `EXPECT_FATAL_FAILURE` wraps the statement in a static
  // helper class with no access to `this`, which doesn't compose with
  // member-function calls; this lambda-friendly form does.
  static std::string captureFatalFailure(std::function<void()> fn) {
    ::testing::TestPartResultArray failures;
    {
      ::testing::ScopedFakeTestPartResultReporter reporter(
        ::testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD,
        &failures);
      fn();
    }
    if (failures.size() == 0) return "<no failure>";
    const auto& first = failures.GetTestPartResult(0);
    if (first.type() != ::testing::TestPartResult::kFatalFailure) {
      return std::string("<non-fatal: ") + first.message() + ">";
    }
    return first.message();
  }
}

using namespace FeatureTestSelfTestNs;

// Single-capture regex step. Demonstrates Cucumber-style argument
// passing: the digit run gets matched, captured, and exposed as
// `match[1]` for std::stoi.
GIVEN(FeatureTestSelfTest, "a centered sphere with radius ([0-9]+)") {
  test->sawRadius = std::stoi(match[1]);
}

// Two-capture regex. Shows multiple groups go through cleanly.
GIVEN(FeatureTestSelfTest, "two values: ([0-9]+) and ([0-9]+)") {
  test->sawTwoArgsA = std::stoi(match[1]);
  test->sawTwoArgsB = std::stoi(match[2]);
}

// No-capture step — bodies that don't use `match` don't need to
// declare anything special; the macro suppresses the unused-parameter
// warning.
GIVEN(FeatureTestSelfTest, "a centered sphere") {
  test->sawCentered = true;
}

// Two patterns that both match the same input. Used by the ambiguity
// test — kept on `FeatureTestAmbiguityTest`'s own registry so the
// `(.*)` wildcard doesn't accidentally match unrelated steps in the
// other fixture.
GIVEN(FeatureTestAmbiguityTest, "duplicated trigger") {}
GIVEN(FeatureTestAmbiguityTest, "(.*) trigger") {}

namespace FeatureTestSelfTestNs {
  TEST_F(FeatureTestSelfTest, RegexCaptureExposesGroupAsMatch) {
    given("a centered sphere with radius 7");
    EXPECT_EQ(7, sawRadius);
  }

  TEST_F(FeatureTestSelfTest, MultipleCaptureGroupsComeThrough) {
    given("two values: 42 and 99");
    EXPECT_EQ(42, sawTwoArgsA);
    EXPECT_EQ(99, sawTwoArgsB);
  }

  TEST_F(FeatureTestSelfTest, NoCaptureStepsStillWork) {
    given("a centered sphere");
    EXPECT_TRUE(sawCentered);
  }

  TEST_F(FeatureTestSelfTest, MissingGivenIsHardFailure) {
    auto msg = captureFatalFailure([this] {
      given("definitely not a registered step");
    });
    EXPECT_NE(std::string::npos, msg.find("given step not registered"))
      << "actual: " << msg;
  }

  TEST_F(FeatureTestSelfTest, MissingWhenIsHardFailure) {
    auto msg = captureFatalFailure([this] {
      when("definitely not a registered step");
    });
    EXPECT_NE(std::string::npos, msg.find("when step not registered"))
      << "actual: " << msg;
  }

  TEST_F(FeatureTestSelfTest, MissingThenIsHardFailure) {
    auto msg = captureFatalFailure([this] {
      then("definitely not a registered step");
    });
    EXPECT_NE(std::string::npos, msg.find("then step not registered"))
      << "actual: " << msg;
  }

  TEST_F(FeatureTestAmbiguityTest, AmbiguousStepIsHardFailure) {
    auto msg = captureFatalFailure([this] {
      given("duplicated trigger");
    });
    EXPECT_NE(std::string::npos, msg.find("given step ambiguous"))
      << "actual: " << msg;
  }
}
