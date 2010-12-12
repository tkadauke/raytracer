#include "gtest.h"
#include "ViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"

namespace ViewPlaneTest {
  using namespace ::testing;
  
  TEST(ViewPlane, ShouldInitialize) {
    ViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
  }
  
  TEST(ViewPlane, ShouldInitializeWithValues) {
    ViewPlane plane(Matrix4d(), 10, 10);
    ASSERT_EQ(10, plane.width());
    ASSERT_EQ(10, plane.height());
  }
  
  TEST(ViewPlane, ShouldSetupVectorsWhenInitializedWithValues) {
    ViewPlane plane(Matrix4d::translate(Vector3d(10, 0, 0)), 8, 6);
    ASSERT_EQ(Vector3d(6, -3, 0), plane.topLeft());
    ASSERT_EQ(Vector3d(1, 0, 0), plane.right());
    ASSERT_EQ(Vector3d(0, 1, 0), plane.down());
  }
  
  TEST(ViewPlane, ShouldSetupVectors) {
    ViewPlane plane;
    plane.setup(Matrix4d::translate(Vector3d(10, 0, 0)), 8, 6);
    ASSERT_EQ(Vector3d(6, -3, 0), plane.topLeft());
    ASSERT_EQ(Vector3d(1, 0, 0), plane.right());
    ASSERT_EQ(Vector3d(0, 1, 0), plane.down());
  }
  
  namespace Iterator {
    TEST(ViewPlane_Iterator, ShouldReturnCurrent) {
      ViewPlane plane(Matrix4d(), 8, 6);
      ViewPlane::Iterator iterator = plane.begin();
      ASSERT_EQ(Vector3d(-4, -3, 0), *iterator);
    }
    
    TEST(ViewPlane_Iterator, ShouldReturnTrueWhenTwoBeginIteratorsAreCompared) {
      ViewPlane plane(Matrix4d(), 8, 6);
      ASSERT_TRUE(plane.begin() == plane.begin());
    }
    
    TEST(ViewPlane_Iterator, ShouldReturnTrueWhenTwoEndIteratorsAreCompared) {
      ViewPlane plane(Matrix4d(), 8, 6);
      ASSERT_TRUE(plane.end() == plane.end());
    }
    
    TEST(ViewPlane_Iterator, ShouldCompareForInEquality) {
      ViewPlane plane(Matrix4d(), 8, 6);
      ASSERT_TRUE(plane.begin() != plane.end());
    }
    
    TEST(ViewPlane_Iterator, ShouldReturnCurrentRow) {
      ViewPlane plane(Matrix4d(), 8, 6);
      ASSERT_EQ(0, plane.begin().row());
    }
    
    TEST(ViewPlane_Iterator, ShouldReturnCurrentColumn) {
      ViewPlane plane(Matrix4d(), 8, 6);
      ASSERT_EQ(0, plane.begin().column());
    }
    
    TEST(ViewPlane_Iterator, ShouldReturnHeightAsCurrentRowForEndIterator) {
      ViewPlane plane(Matrix4d(), 8, 6);
      ASSERT_EQ(6, plane.end().row());
    }
    
    TEST(ViewPlane_Iterator, ShouldReturnZeroAsCurrentColumnForEndIterator) {
      ViewPlane plane(Matrix4d(), 8, 6);
      ASSERT_EQ(0, plane.end().column());
    }
  }
  
  INSTANTIATE_TYPED_TEST_CASE_P(Regular, AbstractViewPlaneIteratorTest, ViewPlane);
}
