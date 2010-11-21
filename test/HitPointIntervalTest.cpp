#include "gtest.h"
#include "HitPointInterval.h"

namespace HitPointIntervalTest {
  TEST(HitPointInterval, ShouldInitialize) {
    HitPointInterval interval;
  }
  
  TEST(HitPointInterval, ShouldInitializeWithPair) {
    HitPointInterval interval(HitPointInterval::Pair(HitPoint(), HitPoint()));
  }
  
  TEST(HitPointInterval, ShouldInitializeWithTwoHitPoints) {
    HitPointInterval interval(HitPoint(), HitPoint());
  }
  
  TEST(HitPointInterval, ShouldAddPair) {
    HitPointInterval interval;
    interval.add(HitPointInterval::Pair(HitPoint(), HitPoint()));
  }
  
  TEST(HitPointInterval, ShouldAddSingleHitPoint) {
    HitPointInterval interval;
    interval.add(HitPoint());
  }
  
  TEST(HitPointInterval, ShouldAddTwoHitPointsAsPair) {
    HitPointInterval interval;
    interval.add(HitPoint(), HitPoint());
  }
  
  TEST(HitPointInterval, ShouldReturnUndefinedClosestHitPointOnEmptyInterval) {
    HitPointInterval interval;
    const HitPoint& i = interval.min();
    ASSERT_TRUE(i == HitPoint::undefined);
  }
  
  TEST(HitPointInterval, ShouldReturnClosestHitPoint) {
    HitPointInterval interval;
    HitPoint hitPoint(5, Vector3d(), Vector3d());
    interval.add(hitPoint);
    ASSERT_TRUE(hitPoint == interval.min());
  }
  
  TEST(HitPointInterval, ShouldReturnUndefinedFarthestHitPointOnEmptyInterval) {
    HitPointInterval interval;
    const HitPoint& i = interval.max();
    ASSERT_TRUE(i == HitPoint::undefined);
  }
  
  TEST(HitPointInterval, ShouldReturnFarthestHitPoint) {
    HitPointInterval interval;
    HitPoint hitPoint(5, Vector3d(), Vector3d());
    interval.add(hitPoint);
    ASSERT_TRUE(hitPoint == interval.max());
  }
  
  TEST(HitPointInterval, ShouldSetClosestAndFarthestHitPointWhenOnlyOneHitPointIsAdded) {
    HitPointInterval interval;
    HitPoint hitPoint(5, Vector3d(), Vector3d());
    interval.add(hitPoint);
    ASSERT_TRUE(interval.min() == interval.max());
  }
  
  TEST(HitPointInterval, ShouldComputeUnionOfTwoEmptyIntervals) {
    HitPointInterval interval1, interval2;
    HitPointInterval unionInterval = interval1 | interval2;
    ASSERT_TRUE(unionInterval.min() == HitPoint::undefined);
    ASSERT_TRUE(unionInterval.max() == HitPoint::undefined);
  }
  
  TEST(HitPointInterval, ShouldComputeUnionOfEmptyAndNonEmptyIntervals) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint(5, Vector3d(), Vector3d());
    interval2.add(hitPoint);
    HitPointInterval unionInterval = interval1 | interval2;
    ASSERT_TRUE(unionInterval.min() == hitPoint);
    ASSERT_TRUE(unionInterval.max() == hitPoint);
  }
  
  TEST(HitPointInterval, ShouldComputeUnionOfTwoNonEmptyIntervals) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint1(5, Vector3d(), Vector3d());
    HitPoint hitPoint2(3, Vector3d(), Vector3d());
    interval1.add(hitPoint1);
    interval2.add(hitPoint2);
    HitPointInterval unionInterval = interval1 | interval2;
    ASSERT_TRUE(unionInterval.min() == hitPoint2);
    ASSERT_TRUE(unionInterval.max() == hitPoint1);
  }
}
