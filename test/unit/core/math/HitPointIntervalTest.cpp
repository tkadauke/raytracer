#include <gtest/gtest.h>
#include "core/math/HitPointInterval.h"
#include "render/primitives/Box.h"

namespace HitPointIntervalTest {
  static render::Box* box = new render::Box(Vector3d::null, Vector3d::one);
  
  TEST(HitPointInterval, ShouldInitialize) {
    HitPointInterval interval;
  }
  
  TEST(HitPointInterval, ShouldInitializeWithTwoHitPoints) {
    HitPointInterval interval((HitPoint()), HitPoint());
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
    HitPoint hitPoint(box, 5, Vector3d(), Vector3d());
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
    HitPoint hitPoint(box, 5, Vector3d(), Vector3d());
    interval.add(hitPoint);
    ASSERT_TRUE(hitPoint == interval.max());
  }
  
  TEST(HitPointInterval, ShouldSetClosestAndFarthestHitPointWhenOnlyOneHitPointIsAdded) {
    HitPointInterval interval;
    HitPoint hitPoint(box, 5, Vector3d(), Vector3d());
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
    HitPoint hitPoint(box, 5, Vector3d(), Vector3d());
    interval2.add(hitPoint);
    HitPointInterval unionInterval = interval1 | interval2;
    
    ASSERT_TRUE(unionInterval.min() == hitPoint);
    ASSERT_TRUE(unionInterval.max() == hitPoint);
  }
  
  TEST(HitPointInterval, ShouldComputeUnionOfTwoNonEmptyIntervals) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint1(box, 5, Vector3d(), Vector3d());
    HitPoint hitPoint2(box, 3, Vector3d(), Vector3d());
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
    HitPoint hitPoint(box, 5, Vector3d(), Vector3d());
    interval2.add(hitPoint);
    HitPointInterval intersectionInterval = interval1 & interval2;
    ASSERT_TRUE(intersectionInterval.min() == HitPoint::undefined());
    ASSERT_TRUE(intersectionInterval.max() == HitPoint::undefined());
  }
  
  TEST(HitPointInterval, ShouldComputeIntersectionOfTwoNonEmptyIntervals) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint1(box, 2, Vector3d(), Vector3d());
    HitPoint hitPoint2(box, 5, Vector3d(), Vector3d());
    HitPoint hitPoint3(box, 4, Vector3d(), Vector3d());
    HitPoint hitPoint4(box, 7, Vector3d(), Vector3d());
    interval1.add(hitPoint1, hitPoint2);
    interval2.add(hitPoint3, hitPoint4);
    HitPointInterval intersectionInterval = interval1 & interval2;
    
    ASSERT_TRUE(intersectionInterval.min() == hitPoint3);
    ASSERT_TRUE(intersectionInterval.max() == hitPoint2);
  }
  
  TEST(HitPointInterval, ShouldReturnEmptyIntersectionIfIntervalsDontOverlap) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint1(box, 2, Vector3d(), Vector3d());
    HitPoint hitPoint2(box, 3, Vector3d(), Vector3d());
    HitPoint hitPoint3(box, 4, Vector3d(), Vector3d());
    HitPoint hitPoint4(box, 5, Vector3d(), Vector3d());
    interval1.add(hitPoint1, hitPoint2);
    interval2.add(hitPoint3, hitPoint4);
    HitPointInterval intersectionInterval = interval1 & interval2;
    
    ASSERT_TRUE(intersectionInterval.min() == HitPoint::undefined());
    ASSERT_TRUE(intersectionInterval.max() == HitPoint::undefined());
  }

  TEST(HitPointInterval, ShouldComputeCompositeOfIntervals) {
    HitPointInterval interval1, interval2;
    HitPoint hitPoint1(box, 2, Vector3d(), Vector3d());
    HitPoint hitPoint2(box, 3, Vector3d(), Vector3d());
    HitPoint hitPoint3(box, 4, Vector3d(), Vector3d());
    HitPoint hitPoint4(box, 5, Vector3d(), Vector3d());
    interval1.add(hitPoint1, hitPoint2);
    interval2.add(hitPoint3, hitPoint4);
    HitPointInterval compositeInterval = interval1 + interval2;
    
    ASSERT_TRUE(compositeInterval.min() == hitPoint1);
    ASSERT_TRUE(compositeInterval.max() == hitPoint4);
  }

  TEST(HitPointInterval, ShouldComputeMergedInterval) {
    HitPointInterval interval;
    HitPoint hitPoint1(box, 2, Vector3d(), Vector3d());
    HitPoint hitPoint2(box, 3, Vector3d(), Vector3d());
    HitPoint hitPoint3(box, 4, Vector3d(), Vector3d());
    HitPoint hitPoint4(box, 5, Vector3d(), Vector3d());
    interval.add(hitPoint1, hitPoint2);
    interval.add(hitPoint3, hitPoint4);
    
    interval = interval.merged();
    ASSERT_EQ(2ul, interval.points().size());
    ASSERT_TRUE(interval.min() == hitPoint1);
    ASSERT_TRUE(interval.max() == hitPoint4);
  }
  
  TEST(HitPointInterval, ShouldTransformInterval) {
    HitPointInterval interval;
    HitPoint hitPoint1(box, 2, Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    HitPoint hitPoint2(box, 3, Vector3d(2, 0, 0), Vector3d(0, 1, 0));
    interval.add(hitPoint1, hitPoint2);

    Matrix4d pointMatrix = Matrix3d::rotateZ(1_radians);
    Matrix3d normalMatrix = Matrix3d::rotateX(1_radians);

    HitPointInterval transformed = interval.transform(pointMatrix, normalMatrix);
    ASSERT_EQ(Vector3d(pointMatrix * Vector4d(1, 0, 0)), transformed.min().point());
  }

  // Zero-allocation contract: intervals with ≤4 hit points must stay in the
  // inline buffer.  usingInlineStorage() exposes the SmallVector state so
  // tests can assert the invariant without a counting-allocator harness.
  TEST(HitPointInterval, SmallBufferUsedForFourOrFewerHitPoints) {
    HitPointInterval interval;
    interval.addIn (HitPoint(box, 1, Vector3d(), Vector3d()));
    interval.addOut(HitPoint(box, 2, Vector3d(), Vector3d()));
    interval.addIn (HitPoint(box, 3, Vector3d(), Vector3d()));
    interval.addOut(HitPoint(box, 4, Vector3d(), Vector3d()));
    ASSERT_EQ(4ul, interval.points().size());
    ASSERT_TRUE(interval.points().usingInlineStorage());
  }

  TEST(HitPointInterval, HeapFallbackForFiveOrMoreHitPoints) {
    HitPointInterval interval;
    for (int i = 0; i < 5; ++i)
      interval.add(HitPoint(box, double(i + 1), Vector3d(), Vector3d()), (i & 1) == 0);
    ASSERT_EQ(5ul, interval.points().size());
    ASSERT_FALSE(interval.points().usingInlineStorage());
  }

  TEST(HitPointInterval, CopyPreservesInlineStorageForSmallIntervals) {
    HitPointInterval a;
    a.addIn (HitPoint(box, 1, Vector3d(), Vector3d()));
    a.addOut(HitPoint(box, 2, Vector3d(), Vector3d()));
    HitPointInterval b = a;
    ASSERT_TRUE(b.points().usingInlineStorage());
    ASSERT_TRUE(b.min() == a.min());
    ASSERT_TRUE(b.max() == a.max());
  }

  TEST(HitPointInterval, MovePreservesCorrectness) {
    HitPointInterval a;
    a.addIn (HitPoint(box, 1, Vector3d(), Vector3d()));
    a.addOut(HitPoint(box, 2, Vector3d(), Vector3d()));
    HitPoint expectedMin = a.min();
    HitPointInterval b = std::move(a);
    ASSERT_TRUE(b.points().usingInlineStorage());
    ASSERT_TRUE(b.min() == expectedMin);
  }
}
