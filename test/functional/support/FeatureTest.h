#ifndef FEATURE_TEST_H
#define FEATURE_TEST_H

#include <gtest/gtest.h>
#include "core/Singleton.h"

#include <regex>
#include <string>
#include <vector>

namespace testing {
  /**
    * @brief Cucumber-style Given/When/Then test fixture base.
    *
    * Subclasses register named steps via the `GIVEN` / `WHEN` /
    * `THEN` macros (see `GivenWhenThen.h`); tests then drive scenarios
    * by calling `given(text)` / `when(text)` / `then(text)`. Step
    * descriptions are interpreted as **regular expressions** matched
    * against the runtime text via `std::regex_match` (full-string,
    * implicit `^...$` anchoring) — capture groups are exposed to the
    * step body as `const std::smatch& match`. This is the same model
    * Cucumber uses for parameterised steps in Ruby.
    *
    * Example:
    * @code
    *   GIVEN(MyTest, "a sphere with radius ([\\d.]+)") {
    *     double r = std::stod(match[1]);
    *     test->add(std::make_shared<Sphere>(Vector3d::null(), r));
    *   }
    *   // ...
    *   given("a sphere with radius 0.5");
    * @endcode
    *
    * Lookup is hard-failing: a step text that matches no registered
    * pattern, or one that ambiguously matches more than one, raises a
    * `GTEST_FAIL` so the test is reported red. (The previous
    * implementation printed `"WARNING:" << name << " step not
    * defined"` to stderr and let the test report green — a real
    * silent-failure path that survived typos and stale copy-paste.)
    *
    * The fixture exposes `beforeGiven` / `beforeWhen` / `beforeThen`
    * lifecycle hooks that fire on the first transition into each
    * phase — useful for batched setup like building the scene before
    * the first `when` arrives, or rendering before the first `then`.
    */
  template<class Derived>
  class FeatureTest : public ::testing::Test {
    enum {
      STATE_INITIAL = 0,
      STATE_GIVEN = 1,
      STATE_WHEN = 2,
      STATE_THEN = 3
    };

  protected:
    inline virtual void beforeGiven() {}
    inline virtual void beforeWhen() {}
    inline virtual void beforeThen() {}

  public:
    inline FeatureTest()
      : m_state(STATE_INITIAL)
    {
    }

    inline virtual ~FeatureTest() {}

    /// A registered step. Subclasses are emitted by the GIVEN /
    /// WHEN / THEN macros — don't subclass directly.
    class Step {
    public:
      virtual ~Step() = default;
      virtual void call(Derived* test, const std::smatch& match) = 0;
    };

    inline void given(const std::string& g) {
      if (m_state != STATE_GIVEN) {
        beforeGiven();
        m_state = STATE_GIVEN;
      }
      invoke("given", Steps::self().givens, g);
    }

    inline void when(const std::string& w) {
      if (m_state != STATE_WHEN) {
        beforeWhen();
        m_state = STATE_WHEN;
      }
      invoke("when", Steps::self().whens, w);
    }

    inline void then(const std::string& t) {
      if (m_state != STATE_THEN) {
        beforeThen();
        m_state = STATE_THEN;
      }
      invoke("then", Steps::self().thens, t);
    }

    inline static bool registerGiven(const std::string& pattern, Step* step) {
      Steps::self().givens.push_back({std::regex(pattern), step});
      return true;
    }

    inline static bool registerWhen(const std::string& pattern, Step* step) {
      Steps::self().whens.push_back({std::regex(pattern), step});
      return true;
    }

    inline static bool registerThen(const std::string& pattern, Step* step) {
      Steps::self().thens.push_back({std::regex(pattern), step});
      return true;
    }

  private:
    struct PatternStep {
      std::regex pattern;
      Step* step;
    };

    struct Steps : public Singleton<Steps> {
      std::vector<PatternStep> givens;
      std::vector<PatternStep> whens;
      std::vector<PatternStep> thens;
    };

    inline void invoke(const char* phase,
                       const std::vector<PatternStep>& steps,
                       const std::string& input) {
      const PatternStep* matched = nullptr;
      std::smatch matchResult;
      int count = 0;
      for (const auto& ps : steps) {
        std::smatch m;
        if (std::regex_match(input, m, ps.pattern)) {
          if (count == 0) {
            matched = &ps;
            matchResult = m;
          }
          ++count;
        }
      }
      if (count == 0) {
        GTEST_FAIL() << phase << " step not registered: '" << input << "'";
      } else if (count > 1) {
        GTEST_FAIL() << phase << " step ambiguous: '" << input
                     << "' matches " << count << " registered patterns";
      } else {
        matched->step->call(static_cast<Derived*>(this), matchResult);
      }
    }

    int m_state;
  };
}

#endif
