#pragma once

#include "gmock/gmock.h"

namespace testing {
  class MockDestructor {
  public:
    MockDestructor()
      : m_expectDestructorCall(false)
    {
    }
    
    inline virtual ~MockDestructor() {
      if (m_expectDestructorCall)
        destructorCall();
    }

    MOCK_METHOD0(destructorCall, void());
    
    inline void expectDestructorCall() {
      m_expectDestructorCall = true;
      EXPECT_CALL(*this, destructorCall());
    }
  
  private:
    bool m_expectDestructorCall;
  };
}
