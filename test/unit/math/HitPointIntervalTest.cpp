#include "gtest.h"
#include "math/HitPointInterval.h"

namespace HitPointIntervalTest {
  TEST(HitPointInterval, ShouldInitialize) {
    HitPointInterval interval;
  }
  
  TEST(HitPointInterval, ShouldInitializeWithTwoHitPoints) {
    HitPointInterval interval(HitPoint(), HitPoint());
  }
  
  TEST(HitPointInterval, ShouldAddSingleHitPoint) {
    HitPointInterval interval;
    interval.add(HitPoint());
  }
  
  TEST(HitPointInterval, ShouldAddTwoHitPointsAsPair) {
    HitPointInterval interval;
    interval.add(HitPoint(), HitPoint());
  }
  
  TEST(HitPointInterval, ShouldReturnTrueForEmptyInterval) {
    ASSERT_TRUE(HitPointInterval().empty());
  }
  
  TEST(HitPointInterval, ShouldReturnUndefinedClosestHitPointOnEmptyInterval) {
    HitPointInterval interval;
    const HitPoint& i = interval.min();
    ASSERT_TRUE(i == HitPoint::undefined());
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
    ASSERT_TRUE(i == HitPoint::undefined());
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
    ASSERT_TRUE(unionInterval.min() == HitPoint::undefined());
    ASSERT_TRUE(unionInterval.max() == HitPoint::undefined());
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
  
  TEST(HitPointInterval, ShouldComputeIntersectionOfTwoEmptyIntervals) {
    HitPointInterval interval1, interval2;
    HitPointInterval intersectionInterval = interval1 & interval2;
    ASSERT_TRUE(intersectionInterval.min() == HitPoint::undefined());
    ASSERT_TRUE(intersectionInterval.max() == HitPoint::undefined());
  }
  
  TEST(HitPointInterval, ShouldComputeIntersectionOfEmptyAndNonEmptyIntervals) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint(5, Vector3d(), Vector3d());
    interval2.add(hitPoint);
    HitPointInterval intersectionInterval = interval1 & interval2;
    ASSERT_TRUE(intersectionInterval.min() == HitPoint::undefined());
    ASSERT_TRUE(intersectionInterval.max() == HitPoint::undefined());
  }
  
  TEST(HitPointInterval, ShouldComputeIntersectionOfTwoNonEmptyIntervals) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint1(2, Vector3d(), Vector3d());
    HitPoint hitPoint2(5, Vector3d(), Vector3d());
    HitPoint hitPoint3(4, Vector3d(), Vector3d());
    HitPoint hitPoint4(7, Vector3d(), Vector3d());
    interval1.add(hitPoint1, hitPoint2);
    interval2.add(hitPoint3, hitPoint4);
    HitPointInterval intersectionInterval = interval1 & interval2;
    
    ASSERT_TRUE(intersectionInterval.min() == hitPoint3);
    ASSERT_TRUE(intersectionInterval.max() == hitPoint2);
  }
  
  TEST(HitPointInterval, ShouldReturnEmptyIntersectionIfIntervalsDontOverlap) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint1(2, Vector3d(), Vector3d());
    HitPoint hitPoint2(3, Vector3d(), Vector3d());
    HitPoint hitPoint3(4, Vector3d(), Vector3d());
    HitPoint hitPoint4(5, Vector3d(), Vector3d());
    interval1.add(hitPoint1, hitPoint2);
    interval2.add(hitPoint3, hitPoint4);
    HitPointInterval intersectionInterval = interval1 & interval2;
    
    ASSERT_TRUE(intersectionInterval.min() == HitPoint::undefined());
    ASSERT_TRUE(intersectionInterval.max() == HitPoint::undefined());
  }
  
  TEST(HitPointInterval, ShouldTransformInterval) {
    HitPointInterval interval;
    HitPoint hitPoint1(2, Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    HitPoint hitPoint2(3, Vector3d(2, 0, 0), Vector3d(0, 1, 0));
    interval.add(hitPoint1, hitPoint2);
    
    Matrix4d pointMatrix = Matrix3d::rotateZ(1);
    Matrix3d normalMatrix = Matrix3d::rotateX(1);
    
    HitPointInterval transformed = interval.transform(pointMatrix, normalMatrix);
    ASSERT_EQ(Vector3d(pointMatrix * Vector4d(1, 0, 0)), transformed.min().point());
  }
}
