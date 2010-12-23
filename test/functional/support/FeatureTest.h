#ifndef FEATURE_TEST_H
#define FEATURE_TEST_H

#include "gtest.h"
#include "core/Singleton.h"

#include <map>
#include <string>

namespace testing {
  template<class Derived>
  class FeatureTest : public ::testing::Test {
  public:
    class Step {
    public:
      virtual void call(Derived* test) = 0;
    };
  
    void given(const std::string& g) {
      Step* step = Steps::self().givens[g];
      if (step) {
        step->call(static_cast<Derived*>(this));
      } else {
        std::cerr << "WARNING: 'given' step '" << g << "' is not defined!" << std::endl;
      }
    }
  
    void when(const std::string& w) {
      Step* step = Steps::self().whens[w];
      if (step) {
        step->call(static_cast<Derived*>(this));
      } else {
        std::cerr << "WARNING: 'when' step '" << w << "' is not defined!" << std::endl;
      }
    }
  
    void then(const std::string& t) {
      Step* step = Steps::self().thens[t];
      if (step) {
        step->call(static_cast<Derived*>(this));
      } else {
        std::cerr << "WARNING: 'then' step '" << t << "' is not defined!" << std::endl;
      }
    }
  
    static bool registerGiven(const std::string& description, Step* step) {
      Steps::self().givens[description] = step;
      return true;
    }

    static bool registerWhen(const std::string& description, Step* step) {
      Steps::self().whens[description] = step;
      return true;
    }

    static bool registerThen(const std::string& description, Step* step) {
      Steps::self().thens[description] = step;
      return true;
    }

  private:
    struct Steps : public Singleton<Steps> {
      std::map<std::string, Step*> givens;
      std::map<std::string, Step*> whens;
      std::map<std::string, Step*> thens;
    };
  };
}

#endif
