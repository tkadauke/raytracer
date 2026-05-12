#include <gtest/gtest.h>
#include "core/math/BoundingBox.h"
#include "core/math/Range.h"
#include "core/math/Ray.h"

#include <sstream>

using namespace std;

namespace BoundingBoxTest {
  using namespace ::testing;
  
  template<class T>
  class BoundingBoxTest : public ::testing::Test {
  };

  typedef ::testing::Types<float, double> BoundingBoxTypes;

  TYPED_TEST_SUITE(BoundingBoxTest, BoundingBoxTypes);

  TYPED_TEST(BoundingBoxTest, ShouldDefineInfiniteBoundingBox) {
    BoundingBox<TypeParam> bbox = BoundingBox<TypeParam>::infinity();
    ASSERT_EQ(Vector3<TypeParam>::minusInfinity(), bbox.min());
    ASSERT_EQ(Vector3<TypeParam>::plusInfinity(), bbox.max());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldDefineUndefinedBoundingBox) {
    ASSERT_TRUE(BoundingBox<TypeParam>::undefined().isUndefined());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldInitializeBoundingBoxAsInfinitelyLarge) {
    BoundingBox<TypeParam> bbox;
    ASSERT_EQ(Vector3<TypeParam>::plusInfinity(), bbox.min());
    ASSERT_EQ(Vector3<TypeParam>::minusInfinity(), bbox.max());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldInitializeWithValues) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_EQ(Vector3<TypeParam>(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3<TypeParam>(1, 1, 1), bbox.max());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldCalculateSize) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    Vector3<TypeParam> expected(2, 2, 2);
    ASSERT_EQ(expected, bbox.size());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldCalculateCenter) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_EQ(Vector3<TypeParam>::null(), bbox.center());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldCompareSameInstanceForEquality) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_TRUE(bbox == bbox);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldCompareDifferentInstancesForEquality) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_TRUE(bbox1 == bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldCompareDifferingInstancesForEquality) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_FALSE(bbox1 == bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldCompareSameInstancesForInequality) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_FALSE(bbox != bbox);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldCompareDifferentInstancesForInequality) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_TRUE(bbox1 != bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludePoint) {
    BoundingBox<TypeParam> bbox;
    bbox.include(Vector3<TypeParam>::null());
    ASSERT_EQ(bbox.min(), bbox.max());
    ASSERT_EQ(Vector3<TypeParam>::null(), bbox.min());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludeMultiplePoints) {
    BoundingBox<TypeParam> bbox;
    bbox.include(Vector3<TypeParam>(-1, -1, -1));
    bbox.include(Vector3<TypeParam>(1, 1, 1));
    ASSERT_EQ(Vector3<TypeParam>(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3<TypeParam>(1, 1, 1), bbox.max());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludeBoundingBox) {
    BoundingBox<TypeParam> bbox1;
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    bbox1.include(bbox2);
    
    ASSERT_EQ(bbox2, bbox1);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludeBoundingBoxViaOperator) {
    BoundingBox<TypeParam> bbox1;
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    ASSERT_EQ(bbox2, bbox1 | bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludeOverlappingBoundingBoxes) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(2, 2, 2)
    );
    
    BoundingBox<TypeParam> expected(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(2, 2, 2)
    );
    
    ASSERT_EQ(expected, bbox1 | bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludeInsideBoundingBoxes) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-2, -2, -2),
      Vector3<TypeParam>(2, 2, 2)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    ASSERT_EQ(bbox1, bbox1 | bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludeOutsideBoundingBoxes) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-2, -2, -2),
      Vector3<TypeParam>(-1, -1, -1)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    BoundingBox<TypeParam> expected(
      Vector3<TypeParam>(-2, -2, -2),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    ASSERT_EQ(expected, bbox1 | bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludeBoundingBoxViaInPlaceOperator) {
    BoundingBox<TypeParam> bbox1;
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    bbox1 |= bbox2;
    
    ASSERT_EQ(bbox2, bbox1);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIntersectInsideBoundingBoxes) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-2, -2, -2),
      Vector3<TypeParam>(2, 2, 2)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    ASSERT_EQ(bbox2, bbox1 & bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIntersectOverlappingBoundingBoxes) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(2, 2, 2)
    );
    
    BoundingBox<TypeParam> expected(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    ASSERT_EQ(expected, bbox1 & bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldReturnUndefinedBoundingBoxForEmptyIntersection) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-2, -2, -2),
      Vector3<TypeParam>(-1, -1, -1)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    ASSERT_TRUE((bbox1 & bbox2).isUndefined());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIntersectBoundingBoxesViaInPlaceOperator) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-2, -2, -2),
      Vector3<TypeParam>(2, 2, 2)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    bbox1 &= bbox2;
    
    ASSERT_EQ(bbox2, bbox1);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldBeInvalidByDefault) {
    BoundingBox<TypeParam> bbox;
    ASSERT_FALSE(bbox.isValid());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldBeValidIfMinIsSmallerThanMax) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_TRUE(bbox.isValid());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldBeInvalidIfMinIsGreaterThanMax) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(1, 1, 1),
      Vector3<TypeParam>(-1, -1, -1)
    );
    ASSERT_FALSE(bbox.isValid());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldBeDefinedByDefault) {
    BoundingBox<TypeParam> bbox;
    ASSERT_FALSE(bbox.isUndefined());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldBeInfiniteByDefault) {
    BoundingBox<TypeParam> bbox;
    ASSERT_TRUE(bbox.isInfinite());
  }

  TYPED_TEST(BoundingBoxTest, ShouldNotBeInfiniteForNormalBoundingBox) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_FALSE(bbox.isInfinite());
  }

  TYPED_TEST(BoundingBoxTest, ShouldReturnTrueIfPointIsInsideBox) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_TRUE(bbox.contains(Vector3<TypeParam>(0, 0, 0)));
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldReturnFalseIfPointIsOutsideOfBox) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    ASSERT_FALSE(bbox.contains(Vector3<TypeParam>(2, 0, 0)));
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldGrowBoundingBox) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> expected(
      Vector3<TypeParam>(-2, -2, -2),
      Vector3<TypeParam>(2, 2, 2)
    );

    ASSERT_EQ(expected, bbox.grownBy(Vector3d::one()));
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldMoveBoundingBox) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> expected(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(2, 2, 2)
    );

    ASSERT_EQ(expected, bbox.movedBy(Vector3d::one()));
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldMoveBoundingBoxWithPlusOperator) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> expected(
      Vector3<TypeParam>(0, 0, 0),
      Vector3<TypeParam>(2, 2, 2)
    );

    ASSERT_EQ(expected, bbox + Vector3d::one());
  }

  TYPED_TEST(BoundingBoxTest, ShouldComputeMinkowskiSum) {
    BoundingBox<TypeParam> bbox1(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    BoundingBox<TypeParam> bbox2(
      Vector3<TypeParam>(-2, -3, -1),
      Vector3<TypeParam>(1, 3, 2)
    );
    BoundingBox<TypeParam> expected(
      Vector3<TypeParam>(-3, -4, -2),
      Vector3<TypeParam>(2, 4, 3)
    );

    ASSERT_EQ(expected, bbox1 + bbox2);
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldReturnEightVertices) {
    vector<Vector3<TypeParam>> vertices;
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    bbox.getVertices(vertices);
    ASSERT_EQ(8u, vertices.size());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIncludeMinAndMaxVertices) {
    vector<Vector3<TypeParam>> vertices;
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    bbox.getVertices(vertices);
    ASSERT_EQ(bbox.min(), vertices.front());
    ASSERT_EQ(bbox.max(), vertices.back());
  }

  TYPED_TEST(BoundingBoxTest, ShouldStreamBoundingBoxToString) {
    BoundingBox<TypeParam> bbox(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    
    ostringstream str;
    str << bbox;
    
    ASSERT_EQ("(-1, -1, -1)-(1, 1, 1)", str.str());
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIntersectWithRayInXDirection) {
    BoundingBox<TypeParam> box(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    Rayd ray(Vector3<TypeParam>(-2, 0, 0), Vector3<TypeParam>(1, 0, 0));
    
    ASSERT_TRUE(box.intersects(ray));
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIntersectWithRayInYDirection) {
    BoundingBox<TypeParam> box(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    Rayd ray(Vector3<TypeParam>(0, -2, 0), Vector3<TypeParam>(0, 1, 0));
    
    ASSERT_TRUE(box.intersects(ray));
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldIntersectWithRayInZDirection) {
    BoundingBox<TypeParam> box(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    Rayd ray(Vector3<TypeParam>(0, 0, -2), Vector3<TypeParam>(0, 0, 1));
    
    ASSERT_TRUE(box.intersects(ray));
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldNotIntersectWithMissingRay) {
    BoundingBox<TypeParam> box(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    Rayd ray(Vector3<TypeParam>(0, 0, -2), Vector3<TypeParam>(0, 1, 0));
    
    ASSERT_FALSE(box.intersects(ray));
  }
  
  TYPED_TEST(BoundingBoxTest, ShouldNotIntersectIfBoxIsBehindRayOrigin) {
    BoundingBox<TypeParam> box(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    Rayd ray(Vector3<TypeParam>(0, 0, 2), Vector3<TypeParam>(0, 0, 1));

    ASSERT_FALSE(box.intersects(ray));
  }

  TYPED_TEST(BoundingBoxTest, ShouldReturnIntervalForHittingRay) {
    BoundingBox<TypeParam> box(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    Rayd ray(Vector4d(-3, 0, 0, 1), Vector3d(1, 0, 0));
    ::Range<TypeParam> interval(TypeParam(0), TypeParam(0));

    ASSERT_TRUE(box.intersect(ray, interval));
    // Ray along +X from x=-3: enters at x=-1 (t=2) and exits at x=+1 (t=4)
    EXPECT_NEAR(double(interval.begin()), 2.0, 1e-5);
    EXPECT_NEAR(double(interval.end()), 4.0, 1e-5);
  }

  TYPED_TEST(BoundingBoxTest, ShouldReturnIntervalForMissingRay) {
    BoundingBox<TypeParam> box(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    // Ray moves in +Y from outside the box's X range
    Rayd ray(Vector4d(0, 0, -2, 1), Vector3d(0, 1, 0));
    ::Range<TypeParam> interval(TypeParam(0), TypeParam(0));

    ASSERT_FALSE(box.intersect(ray, interval));
    // t_enter > t_exit confirms a miss
    EXPECT_GT(double(interval.begin()), double(interval.end()));
  }

  TYPED_TEST(BoundingBoxTest, ShouldReturnNegativeExitForBoxBehindRay) {
    BoundingBox<TypeParam> box(
      Vector3<TypeParam>(-1, -1, -1),
      Vector3<TypeParam>(1, 1, 1)
    );
    // Ray moves in +Z from z=+2 (box is behind)
    Rayd ray(Vector4d(0, 0, 2, 1), Vector3d(0, 0, 1));
    ::Range<TypeParam> interval(TypeParam(0), TypeParam(0));

    ASSERT_FALSE(box.intersect(ray, interval));
    // t_exit is negative — the box is entirely behind the ray origin
    EXPECT_LT(double(interval.end()), 0.0);
  }
}
