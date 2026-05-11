#include <gtest/gtest.h>
#include "render/viewplanes/ViewPlane.h"
#include "test/abstract/AbstractViewPlaneIteratorTest.h"
#include "test/helpers/VectorTestHelper.h"

namespace ViewPlaneTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;
  
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

  TEST(ViewPlane, ShouldConvertClipCoordinatesToScreenCoordinates) {
    ViewPlane plane(Matrix4d(), Recti(200, 150));
    ASSERT_VECTOR_NEAR(Vector3d(150, 37.5, 4.0),
                       plane.screenFromClipUnchecked(Vector4d(0.5, -0.5, 4.0, 1.0)), 0.001);
  }

  TEST(ViewPlane, ShouldRejectClipCoordinatesWithInvalidPerspectiveDivide) {
    ViewPlane plane(Matrix4d(), Recti(200, 150));
    ASSERT_TRUE(plane.screenFromClip(Vector4d(0.0, 0.0, 1.0, 0.0)).isUndefined());
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
  
  // ── Aspect mode tests ────────────────────────────────────────────────

  TEST(ViewPlane, DefaultAspectModeIsStretch) {
    ViewPlane plane;
    ASSERT_EQ(AspectMode::Stretch, plane.aspectMode());
  }

  TEST(ViewPlane, CanSetAndGetAspectMode) {
    ViewPlane plane;
    plane.setAspectMode(AspectMode::FitWidth);
    ASSERT_EQ(AspectMode::FitWidth, plane.aspectMode());
  }

  TEST(ViewPlane, StretchModePreservesHardcoded8x6Extents) {
    // The Stretch mode must stay byte-for-byte identical to the
    // pre-AspectMode behavior so existing callers are unaffected.
    ViewPlane plane;
    plane.setAspectMode(AspectMode::Stretch);
    plane.setup(Matrix4d(), Recti(16, 9));
    // hSpan=8, vSpan=6 always in Stretch mode.
    // topLeft.x = -4, topLeft.y = -3
    ASSERT_NEAR(-4.0, plane.topLeft().x(), 0.001);
    ASSERT_NEAR(-3.0, plane.topLeft().y(), 0.001);
  }

  TEST(ViewPlane, FitWidthProducesSquarePixelsFor16x9Buffer) {
    ViewPlane plane;
    plane.setAspectMode(AspectMode::FitWidth);
    plane.setup(Matrix4d(), Recti(16, 9));
    // right per pixel = 8/16 = 0.5; down per pixel = (8*9/16)/9 = 8/16 = 0.5 → square
    ASSERT_NEAR(0.5, plane.right().length(), 0.001);
    ASSERT_NEAR(0.5, plane.down().length(), 0.001);
  }

  TEST(ViewPlane, FitWidthHSpanIsConstantAcrossBufferAspects) {
    // H-span = right * width = 8 always in FitWidth mode.
    ViewPlane wideplane;
    wideplane.setAspectMode(AspectMode::FitWidth);
    wideplane.setup(Matrix4d(), Recti(160, 90));
    double hSpanWide = wideplane.right().length() * 160;

    ViewPlane tallplane;
    tallplane.setAspectMode(AspectMode::FitWidth);
    tallplane.setup(Matrix4d(), Recti(90, 160));
    double hSpanTall = tallplane.right().length() * 90;

    ASSERT_NEAR(8.0, hSpanWide, 0.001);
    ASSERT_NEAR(8.0, hSpanTall, 0.001);
  }

  TEST(ViewPlane, FitHeightProducesSquarePixelsFor16x9Buffer) {
    ViewPlane plane;
    plane.setAspectMode(AspectMode::FitHeight);
    plane.setup(Matrix4d(), Recti(16, 9));
    // down per pixel = 6/9 ≈ 0.667; right per pixel = (6*16/9)/16 = 6/9 ≈ 0.667 → square
    ASSERT_NEAR(plane.right().length(), plane.down().length(), 0.001);
  }

  TEST(ViewPlane, FitHeightVSpanIsConstantAcrossBufferAspects) {
    // V-span = down * height = 6 always in FitHeight mode.
    ViewPlane wideplane;
    wideplane.setAspectMode(AspectMode::FitHeight);
    wideplane.setup(Matrix4d(), Recti(160, 90));
    double vSpanWide = wideplane.down().length() * 90;

    ViewPlane tallplane;
    tallplane.setAspectMode(AspectMode::FitHeight);
    tallplane.setup(Matrix4d(), Recti(90, 160));
    double vSpanTall = tallplane.down().length() * 160;

    ASSERT_NEAR(6.0, vSpanWide, 0.001);
    ASSERT_NEAR(6.0, vSpanTall, 0.001);
  }

  TEST(ViewPlane, FitExactComputesInnerRectForWideBuffer) {
    // 16×9 buffer with 4:3 intrinsic → pillarbox: innerW=12, innerH=9, offsetX=2
    ViewPlane plane;
    plane.setAspectMode(AspectMode::FitExact);
    plane.setAspectRatio(4.0 / 3.0);
    plane.setup(Matrix4d(), Recti(16, 9));
    ASSERT_EQ(2,  plane.innerRect().left());
    ASSERT_EQ(0,  plane.innerRect().top());
    ASSERT_EQ(12, plane.innerRect().width());
    ASSERT_EQ(9,  plane.innerRect().height());
  }

  TEST(ViewPlane, FitExactComputesInnerRectForTallBuffer) {
    // 9×16 buffer with 4:3 intrinsic → letterbox: innerW=9, innerH=6, offsetY=5
    ViewPlane plane;
    plane.setAspectMode(AspectMode::FitExact);
    plane.setAspectRatio(4.0 / 3.0);
    plane.setup(Matrix4d(), Recti(9, 16));
    ASSERT_EQ(0, plane.innerRect().left());
    ASSERT_EQ(5, plane.innerRect().top());
    ASSERT_EQ(9, plane.innerRect().width());
    ASSERT_EQ(6, plane.innerRect().height());
  }

  TEST(ViewPlane, FitExactProducesSquarePixelsInsideInnerRect) {
    // For a 16×9 buffer with 4:3 intrinsic the inner rect is 12×9.
    // right per pixel = 8/12 ≈ 0.667; down per pixel = 6/9 ≈ 0.667 → square
    ViewPlane plane;
    plane.setAspectMode(AspectMode::FitExact);
    plane.setAspectRatio(4.0 / 3.0);
    plane.setup(Matrix4d(), Recti(16, 9));
    ASSERT_NEAR(plane.right().length(), plane.down().length(), 0.001);
  }

  TEST(ViewPlane, FitExactInnerRectIsFullBufferWhenAspectMatches) {
    // Exact 4:3 buffer with 4:3 intrinsic → no bars
    ViewPlane plane;
    plane.setAspectMode(AspectMode::FitExact);
    plane.setAspectRatio(4.0 / 3.0);
    plane.setup(Matrix4d(), Recti(8, 6));
    ASSERT_EQ(0, plane.innerRect().left());
    ASSERT_EQ(0, plane.innerRect().top());
    ASSERT_EQ(8, plane.innerRect().width());
    ASSERT_EQ(6, plane.innerRect().height());
  }

  TEST(ViewPlane, FitExactDefaultRatioIs4x3WhenNoneSet) {
    // Unset ratio defaults to 4:3 inside the view plane.
    ViewPlane plane;
    plane.setAspectMode(AspectMode::FitExact);
    // Do NOT call setAspectRatio — let it fall through to the 4:3 default.
    plane.setup(Matrix4d(), Recti(16, 9));
    // 4:3 intrinsic in a 16:9 buffer → pillarbox: innerW=12
    ASSERT_EQ(12, plane.innerRect().width());
  }

  TEST(ViewPlane, StretchInnerRectIsFullBuffer) {
    ViewPlane plane;
    plane.setAspectMode(AspectMode::Stretch);
    plane.setup(Matrix4d(), Recti(16, 9));
    ASSERT_EQ(0,  plane.innerRect().left());
    ASSERT_EQ(0,  plane.innerRect().top());
    ASSERT_EQ(16, plane.innerRect().width());
    ASSERT_EQ(9,  plane.innerRect().height());
  }

  INSTANTIATE_TYPED_TEST_SUITE_P(
    Regular,
    AbstractViewPlaneIteratorTest,
    ViewPlane
  );
}
