#include "gtest.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"
#include "test/helpers/VectorTestHelper.h"

namespace ViewPlaneTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(ViewPlane, ShouldInitialize) {
    ViewPlane plane;
    ASSERT_EQ(0, plane.width());
    ASSERT_EQ(0, plane.height());
    ASSERT_EQ(1, plane.pixelSize());
  }
  
  TEST(ViewPlane, ShouldInitializeWithValues) {
    ViewPlane plane(Matrix4d(), Recti(10, 10));
    ASSERT_EQ(10, plane.width());
    ASSERT_EQ(10, plane.height());
    ASSERT_EQ(1, plane.pixelSize());
  }
  
  TEST(ViewPlane, ShouldSetupVectorsWhenInitializedWithValues) {
    ViewPlane plane(Matrix4d::translate(Vector3d(10, 0, 0)), Recti(8, 6));
    ASSERT_EQ(Vector3d(6, -3, 0), plane.topLeft());
    ASSERT_EQ(Vector3d(1, 0, 0), plane.right());
    ASSERT_EQ(Vector3d(0, 1, 0), plane.down());
  }
  
  TEST(ViewPlane, ShouldSetupVectors) {
    ViewPlane plane;
    plane.setup(Matrix4d::translate(Vector3d(10, 0, 0)), Recti(8, 6));
    ASSERT_EQ(Vector3d(6, -3, 0), plane.topLeft());
    ASSERT_EQ(Vector3d(1, 0, 0), plane.right());
    ASSERT_EQ(Vector3d(0, 1, 0), plane.down());
  }
  
  TEST(ViewPlane, ShouldCalculatePixelPosition) {
    ViewPlane plane(Matrix4d(), Recti(10, 10));
    ASSERT_VECTOR_NEAR(Vector3d(-4, -3, 0), plane.pixelAt(0, 0), 0.001);
    ASSERT_VECTOR_NEAR(Vector3d( 4,  3, 0), plane.pixelAt(10, 10), 0.001);
  }
  
  namespace Iterator {
    struct ViewPlane_Iterator : public ::testing::Test {
      virtual void SetUp() {
        fullRect = Recti(8, 6);
      }
      
      Recti fullRect;
    };
    
    TEST_F(ViewPlane_Iterator, ShouldReturnCurrent) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      auto iterator = plane.begin(this->fullRect);
      ASSERT_EQ(Vector3d(-4, -3, 0), *iterator);
    }
    
    TEST_F(ViewPlane_Iterator, ShouldReturnPixel) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      auto iterator = plane.begin(this->fullRect);
      ASSERT_EQ(Vector2d(0, 0), iterator.pixel());
    }
    
    TEST_F(ViewPlane_Iterator, ShouldMultiplyCurrentByPixelSize) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      plane.setPixelSize(2);
      auto iterator = plane.begin(this->fullRect);
      ASSERT_EQ(Vector3d(-8, -6, 0), *iterator);
    }
    
    TEST_F(ViewPlane_Iterator, ShouldReturnTrueWhenTwoBeginIteratorsAreCompared) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      ASSERT_TRUE(plane.begin(this->fullRect) == plane.begin(this->fullRect));
    }
    
    TEST_F(ViewPlane_Iterator, ShouldReturnTrueWhenTwoEndIteratorsAreCompared) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      ASSERT_TRUE(plane.end(this->fullRect) == plane.end(this->fullRect));
    }
    
    TEST_F(ViewPlane_Iterator, ShouldCompareForInEquality) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      ASSERT_TRUE(plane.begin(this->fullRect) != plane.end(this->fullRect));
    }
    
    TEST_F(ViewPlane_Iterator, ShouldReturnCurrentRow) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      ASSERT_EQ(0, plane.begin(this->fullRect).row());
    }
    
    TEST_F(ViewPlane_Iterator, ShouldReturnCurrentColumn) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      ASSERT_EQ(0, plane.begin(this->fullRect).column());
    }
    
    TEST_F(ViewPlane_Iterator, ShouldReturnHeightAsCurrentRowForEndIterator) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      ASSERT_EQ(6, plane.end(this->fullRect).row());
    }
    
    TEST_F(ViewPlane_Iterator, ShouldReturnZeroAsCurrentColumnForEndIterator) {
      ViewPlane plane(Matrix4d(), this->fullRect);
      ASSERT_EQ(0, plane.end(this->fullRect).column());
    }
  }
  
  INSTANTIATE_TYPED_TEST_CASE_P(
    Regular,
    AbstractViewPlaneIteratorTest,
    ViewPlane
  );
}
