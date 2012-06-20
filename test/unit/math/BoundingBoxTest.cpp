#include "gtest.h"
#include "math/BoundingBox.h"

#include <sstream>

using namespace std;

namespace BoundingBoxTest {
  using namespace ::testing;
  
  TEST(BoundingBox, ShouldInitializeBoundingBoxAsInfinitesimallySmall) {
    BoundingBox bbox;
    ASSERT_EQ(Vector3d::plusInfinity(), bbox.min());
    ASSERT_EQ(Vector3d::minusInfinity(), bbox.max());
  }
  
  TEST(BoundingBox, ShouldInitializeWithValues) {
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
  
  TEST(BoundingBox, ShouldCalculateCenter) {
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_EQ(Vector3d::null(), bbox.center());
  }
  
  TEST(BoundingBox, ShouldCompareSameInstanceForEquality) {
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_TRUE(bbox == bbox);
  }
  
  TEST(BoundingBox, ShouldCompareDifferentInstancesForEquality) {
    BoundingBox bbox1(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    BoundingBox bbox2(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_TRUE(bbox1 == bbox2);
  }
  
  TEST(BoundingBox, ShouldCompareDifferingInstancesForEquality) {
    BoundingBox bbox1(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    BoundingBox bbox2(Vector3d(0, 0, 0), Vector3d(1, 1, 1));
    ASSERT_FALSE(bbox1 == bbox2);
  }
  
  TEST(BoundingBox, ShouldCompareSameInstancesForInequality) {
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_FALSE(bbox != bbox);
  }
  
  TEST(BoundingBox, ShouldCompareDifferentInstancesForInequality) {
    BoundingBox bbox1(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    BoundingBox bbox2(Vector3d(0, 0, 0), Vector3d(1, 1, 1));
    ASSERT_TRUE(bbox1 != bbox2);
  }
  
  TEST(BoundingBox, ShouldIncludePoint) {
    BoundingBox bbox;
    bbox.include(Vector3d::null());
    ASSERT_EQ(bbox.min(), bbox.max());
    ASSERT_EQ(Vector3d::null(), bbox.min());
  }
  
  TEST(BoundingBox, ShouldIncludeMultiplePoints) {
    BoundingBox bbox;
    bbox.include(Vector3d(-1, -1, -1));
    bbox.include(Vector3d(1, 1, 1));
    ASSERT_EQ(Vector3d(-1, -1, -1), bbox.min());
    ASSERT_EQ(Vector3d(1, 1, 1), bbox.max());
  }
  
  TEST(BoundingBox, ShouldIncludeBoundingBox) {
    BoundingBox bbox1;
    BoundingBox bbox2(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    
    bbox1.include(bbox2);
    
    ASSERT_EQ(bbox2, bbox1);
  }
  
  TEST(BoundingBox, ShouldIncludeBoundingBoxViaOperator) {
    BoundingBox bbox1;
    BoundingBox bbox2(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    
    ASSERT_EQ(bbox2, bbox1 | bbox2);
  }
  
  TEST(BoundingBox, ShouldIncludeOverlappingBoundingBoxes) {
    BoundingBox bbox1(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    BoundingBox bbox2(Vector3d(0, 0, 0), Vector3d(2, 2, 2));
    
    BoundingBox expected(Vector3d(-1, -1, -1), Vector3d(2, 2, 2));
    
    ASSERT_EQ(expected, bbox1 | bbox2);
  }
  
  TEST(BoundingBox, ShouldIncludeInsideBoundingBoxes) {
    BoundingBox bbox1(Vector3d(-2, -2, -2), Vector3d(2, 2, 2));
    BoundingBox bbox2(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    
    ASSERT_EQ(bbox1, bbox1 | bbox2);
  }
  
  TEST(BoundingBox, ShouldIncludeOutsideBoundingBoxes) {
    BoundingBox bbox1(Vector3d(-2, -2, -2), Vector3d(-1, -1, -1));
    BoundingBox bbox2(Vector3d(0, 0, 0), Vector3d(1, 1, 1));
    
    BoundingBox expected(Vector3d(-2, -2, -2), Vector3d(1, 1, 1));
    
    ASSERT_EQ(expected, bbox1 | bbox2);
  }
  
  TEST(BoundingBox, ShouldIncludeBoundingBoxViaInPlaceOperator) {
    BoundingBox bbox1;
    BoundingBox bbox2(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    
    bbox1 |= bbox2;
    
    ASSERT_EQ(bbox2, bbox1);
  }
  
  TEST(BoundingBox, ShouldIntersectInsideBoundingBoxes) {
    BoundingBox bbox1(Vector3d(-2, -2, -2), Vector3d(2, 2, 2));
    BoundingBox bbox2(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    
    ASSERT_EQ(bbox2, bbox1 & bbox2);
  }
  
  TEST(BoundingBox, ShouldIntersectOverlappingBoundingBoxes) {
    BoundingBox bbox1(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    BoundingBox bbox2(Vector3d(0, 0, 0), Vector3d(2, 2, 2));
    
    BoundingBox expected(Vector3d(0, 0, 0), Vector3d(1, 1, 1));
    
    ASSERT_EQ(expected, bbox1 & bbox2);
  }
  
  TEST(BoundingBox, ShouldReturnUndefinedBoundingBoxForEmptyIntersection) {
    BoundingBox bbox1(Vector3d(-2, -2, -2), Vector3d(-1, -1, -1));
    BoundingBox bbox2(Vector3d(0, 0, 0), Vector3d(1, 1, 1));
    
    ASSERT_TRUE((bbox1 & bbox2).isUndefined());
  }
  
  TEST(BoundingBox, ShouldIntersectBoundingBoxesViaInPlaceOperator) {
    BoundingBox bbox1(Vector3d(-2, -2, -2), Vector3d(2, 2, 2));
    BoundingBox bbox2(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    
    bbox1 &= bbox2;
    
    ASSERT_EQ(bbox2, bbox1);
  }
  
  TEST(BoundingBox, ShouldBeInvalidByDefault) {
    BoundingBox bbox;
    ASSERT_FALSE(bbox.isValid());
  }
  
  TEST(BoundingBox, ShouldBeValidIfMinIsSmallerThanMax) {
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_TRUE(bbox.isValid());
  }
  
  TEST(BoundingBox, ShouldBeInvalidIfMinIsGreaterThanMax) {
    BoundingBox bbox(Vector3d(1, 1, 1), Vector3d(-1, -1, -1));
    ASSERT_FALSE(bbox.isValid());
  }
  
  TEST(BoundingBox, ShouldBeDefinedByDefault) {
    BoundingBox bbox;
    ASSERT_FALSE(bbox.isUndefined());
  }
  
  TEST(BoundingBox, ShouldDefineUndefinedBoundingBox) {
    ASSERT_TRUE(BoundingBox::undefined().isUndefined());
  }
  
  TEST(BoundingBox, ShouldReturnTrueIfPointIsInsideBox) {
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_TRUE(bbox.contains(Vector3d(0, 0, 0)));
  }
  
  TEST(BoundingBox, ShouldReturnFalseIfPointIsOutsideOfBox) {
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    ASSERT_FALSE(bbox.contains(Vector3d(2, 0, 0)));
  }
  
  TEST(BoundingBox, ShouldReturnEightVertices) {
    vector<Vector3d> vertices;
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    bbox.getVertices(vertices);
    ASSERT_EQ(8u, vertices.size());
  }
  
  TEST(BoundingBox, ShouldIncludeMinAndMaxVertices) {
    vector<Vector3d> vertices;
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    bbox.getVertices(vertices);
    ASSERT_EQ(bbox.min(), vertices.front());
    ASSERT_EQ(bbox.max(), vertices.back());
  }

  TEST(BoundingBox, ShouldStreamBoundingBoxToString) {
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    
    ostringstream str;
    str << bbox;
    
    ASSERT_EQ("(-1, -1, -1)-(1, 1, 1)", str.str());
  }
}
