#include "gtest.h"
#include "Singleton.h"
#include "test/helpers/TypeTestHelper.h"

namespace SingletonTest {
  struct Log {};
  typedef Singleton<Log> Logger;
  
  struct AnotherLog : public Singleton<AnotherLog> {
    int function() { return 42; }
  };
  
  TEST(Singleton, ShouldInitialize) {
    Logger::self();
  }
  
  TEST(Singleton, ShouldAlwaysReturnTheSameObject) {
    ASSERT_EQ(&Logger::self(), &Logger::self());
  }
  
  TEST(Singleton, ShouldReturnObjectOfCorrectType) {
    ASSERT_TYPES_EQ(Log(), Logger::self());
  }
  
  TEST(Singleton, ShouldSupportCRTP) {
    ASSERT_EQ(42, AnotherLog::self().function());
  }
}
