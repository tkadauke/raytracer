#ifndef FEATURE_TEST_H
#define FEATURE_TEST_H

#include "gtest.h"
#include "core/Singleton.h"

#include <map>
#include <string>

namespace testing {
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
    
    class Step {
    public:
      virtual void call(Derived* test) = 0;
    };
    
    inline void given(const std::string& g) {
      if (m_state != STATE_GIVEN) {
        beforeGiven();
        m_state = STATE_GIVEN;
      }

      auto step = Steps::self().givens[g];
      if (step) {
        step->call(static_cast<Derived*>(this));
      } else {
        std::cerr << "WARNING: 'given' step '" << g << "' is not defined!" << std::endl;
      }
    }
  
    inline void when(const std::string& w) {
      if (m_state != STATE_WHEN) {
        beforeWhen();
        m_state = STATE_WHEN;
      }

      auto step = Steps::self().whens[w];
      if (step) {
        step->call(static_cast<Derived*>(this));
      } else {
        std::cerr << "WARNING: 'when' step '" << w << "' is not defined!" << std::endl;
      }
    }
  
    inline void then(const std::string& t) {
      if (m_state != STATE_THEN) {
        beforeThen();
        m_state = STATE_THEN;
      }

      auto step = Steps::self().thens[t];
      if (step) {
        step->call(static_cast<Derived*>(this));
      } else {
        std::cerr << "WARNING: 'then' step '" << t << "' is not defined!" << std::endl;
      }
    }
  
    inline static bool registerGiven(const std::string& description, Step* step) {
      Steps::self().givens[description] = step;
      return true;
    }

    inline static bool registerWhen(const std::string& description, Step* step) {
      Steps::self().whens[description] = step;
      return true;
    }

    inline static bool registerThen(const std::string& description, Step* step) {
      Steps::self().thens[description] = step;
      return true;
    }

  private:
    int m_state;
    struct Steps : public Singleton<Steps> {
      std::map<std::string, Step*> givens;
      std::map<std::string, Step*> whens;
      std::map<std::string, Step*> thens;
    };
  };
}

#endif
