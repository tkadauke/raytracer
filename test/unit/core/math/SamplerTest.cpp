#include "gtest.h"
#include "core/math/Sampler.h"

namespace SamplerTest {
  TEST(Sampler, ShouldFlipCoin) {
    Sampler sampler(10);
    // 10x the sample rate should guarantee that we have both true and false
    // outcomes
    int i;
    for (i = 0; i != 100; i++) {
      if (sampler.coinFlip()) {
        break;
      }
    }
    ASSERT_TRUE(i != 100);
  }

  TEST(Sampler, ShouldFlipCoinWithFunction) {
    Sampler sampler(10);
    // 10x the sample rate should guarantee that we have both true and false
    // outcomes
    bool yes = false;
    for (int i = 0; i != 100; i++) {
      sampler.coinFlip([&] { yes = true; });
    }
    ASSERT_TRUE(yes);
  }
}
