#ifndef GIVEN_WHEN_THEN_H
#define GIVEN_WHEN_THEN_H

#include <regex>

// Macros for Cucumber-style Given / When / Then step registration.
// The Description argument is a regex matched against the runtime
// step text via std::regex_match — capture groups in the pattern are
// exposed to the body via `const std::smatch& match`.
//
// Bodies that don't use the captures simply ignore the `match`
// parameter; the [[maybe_unused]] attribute on it suppresses
// -Wunused-parameter for those bodies.

#define CONCAT(x, y) x ## y
#define CONCAT2(x, y) CONCAT(x, y)

#define STEPNAME(Steps, Token, Counter) CONCAT2(Steps ## Token, Counter)

#define GIVEN_(Steps, StepName, Description) \
  namespace { \
    struct StepName : public Steps::Step { \
      virtual void call(Steps* test, const std::smatch& match); \
      static bool dummy; \
    }; \
    bool StepName::dummy = Steps::registerGiven(Description, new StepName); \
  } \
  void StepName::call([[maybe_unused]] Steps* test, [[maybe_unused]] const std::smatch& match)

#define GIVEN(Steps, Description) \
  GIVEN_(Steps, STEPNAME(Steps, _Given, __LINE__), Description)

#define WHEN_(Steps, StepName, Description) \
  namespace { \
    struct StepName : public Steps::Step { \
      virtual void call(Steps* test, const std::smatch& match); \
      static bool dummy; \
    }; \
    bool StepName::dummy = Steps::registerWhen(Description, new StepName); \
  } \
  void StepName::call([[maybe_unused]] Steps* test, [[maybe_unused]] const std::smatch& match)

#define WHEN(Steps, Description) \
  WHEN_(Steps, STEPNAME(Steps, _When, __LINE__), Description)

#define THEN_(Steps, StepName, Description) \
  namespace { \
    struct StepName : public Steps::Step { \
      virtual void call(Steps* test, const std::smatch& match); \
      static bool dummy; \
    }; \
    bool StepName::dummy = Steps::registerThen(Description, new StepName); \
  } \
  void StepName::call([[maybe_unused]] Steps* test, [[maybe_unused]] const std::smatch& match)

#define THEN(Steps, Description) \
  THEN_(Steps, STEPNAME(Steps, _Then, __LINE__), Description)

#endif
