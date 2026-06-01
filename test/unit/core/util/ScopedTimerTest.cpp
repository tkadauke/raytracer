#include <gtest/gtest.h>

#include "core/util/ScopedTimer.h"

namespace ScopedTimerTest {
  TEST(ScopedTimer, StopAccumulatesElapsedSecondsOnce) {
    double seconds = 0.0;

    {
      core::util::ScopedTimer timer(&seconds);
      timer.stop();
      const double stoppedSeconds = seconds;
      timer.stop();

      EXPECT_GE(stoppedSeconds, 0.0);
      EXPECT_DOUBLE_EQ(stoppedSeconds, seconds);
    }
  }

  TEST(ScopedTimer, NullTargetIsNoop) {
    core::util::ScopedTimer timer(nullptr);

    timer.stop();

    SUCCEED();
  }
}
