#ifndef GIVEN_WHEN_THEN_H
#define GIVEN_WHEN_THEN_H

#define CONCAT(x, y) x ## y
#define CONCAT2(x, y) CONCAT(x, y)

#define STEPNAME(Steps, Token, Counter) CONCAT2(Steps ## Token, Counter)

#define GIVEN_(Steps, StepName, Description) \
  namespace { \
    struct StepName : public Steps::Step { \
      virtual void call(Steps* test); \
      static bool dummy; \
    }; \
    bool StepName::dummy = Steps::registerGiven(Description, new StepName); \
  } \
  void StepName::call(Steps* test)

#define GIVEN(Steps, Description) \
  GIVEN_(Steps, STEPNAME(Steps, _Given, __LINE__), Description)

#define WHEN_(Steps, StepName, Description) \
  namespace { \
    struct StepName : public Steps::Step { \
      virtual void call(Steps* test); \
      static bool dummy; \
    }; \
    bool StepName::dummy = Steps::registerWhen(Description, new StepName); \
  } \
  void StepName::call(Steps* test)

#define WHEN(Steps, Description) \
  WHEN_(Steps, STEPNAME(Steps, _When, __LINE__), Description)

#define THEN_(Steps, StepName, Description) \
  namespace { \
    struct StepName : public Steps::Step { \
      virtual void call(Steps* test); \
      static bool dummy; \
    }; \
    bool StepName::dummy = Steps::registerThen(Description, new StepName); \
  } \
  void StepName::call(Steps* test)

#define THEN(Steps, Description) \
  THEN_(Steps, STEPNAME(Steps, _Then, __LINE__), Description)

#endif
