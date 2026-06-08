#include <gtest/gtest.h>
#include "core/Buffer.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/cameras/Camera.h"
#include "render/cameras/PinholeCamera.h"
#include "render/samplers/RegularSampler.h"
#include "render/viewplanes/ViewPlane.h"
#include "test/mocks/raytracer/MockViewPlane.h"

namespace CameraTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;

  class ConcreteCamera : public Camera {
  public:
    inline ConcreteCamera()
        : Camera() {
    }

    inline ConcreteCamera(const Vector3d& position, const Vector3d& target)
        : Camera(position, target) {
    }

    void render(std::shared_ptr<RayCaster>, Buffer<Colord>&, const Recti&) const override {
      // noop
    }

    Rayd rayForPixel(double, double, render::SampleStream&) const override {
      return Rayd::undefined;
    }

    std::shared_ptr<Camera> clone() const override {
      auto result = std::make_shared<ConcreteCamera>();
      copyBaseStateTo(*result);
      return result;
    }

    const char* fingerprintType() const override {
      return "ConcreteCamera";
    }
  };

  class ProgressivePublishingRayCaster : public RayCaster {
  public:
    explicit ProgressivePublishingRayCaster(Buffer<unsigned int>& buffer)
        : m_buffer(buffer) {
    }

    Colord rayColor(const Rayd&, State&) const override {
      if (m_calls == 1) {
        m_bufferBeforeSecondSample = m_buffer[0][0];
      }
      ++m_calls;
      return m_calls == 1 ? Colord::red() : Colord::black();
    }

    bool prefersProgressiveSamplePublishing() const override {
      return true;
    }

    int calls() const {
      return m_calls;
    }

    unsigned int bufferBeforeSecondSample() const {
      return m_bufferBeforeSecondSample;
    }

  private:
    Buffer<unsigned int>& m_buffer;
    mutable int m_calls{0};
    mutable unsigned int m_bufferBeforeSecondSample{0};
  };

  TEST(Camera, ShouldConstructWithoutParameters) {
    ConcreteCamera camera;
  }

  TEST(Camera, ShouldConstructWithParameters) {
    ConcreteCamera camera(Vector3d(0, 0, 1), Vector3d::null);
  }

  TEST(Camera, ShouldDeleteViewPlaneOnDestruct) {
    auto plane = std::make_shared<NiceMock<MockViewPlane>>();
    auto camera = new ConcreteCamera;
    plane->expectDestructorCall();
    camera->setViewPlane(plane);
    delete camera;
  }

  TEST(Camera, ShouldEnableProgressIndicators) {
    ConcreteCamera camera;
    ASSERT_FALSE(camera.showProgressIndicators());
    camera.setShowProgressIndicators(true);
    ASSERT_TRUE(camera.showProgressIndicators());
  }

  TEST(Camera, ShouldReturnMatrix) {
    ConcreteCamera camera(Vector3d(0, 0, -1), Vector3d::null);
    Matrix4d expected(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -1, 0, 0, 0, 1);
    ASSERT_EQ(expected, camera.matrix());
  }

  TEST(Camera, ShouldReturnMatrixWithCorrectTranslation) {
    ConcreteCamera camera(Vector3d(4, 3, 2), Vector3d::null);
    ASSERT_EQ(4, camera.matrix()[0][3]);
    ASSERT_EQ(3, camera.matrix()[1][3]);
    ASSERT_EQ(2, camera.matrix()[2][3]);
  }

  TEST(Camera, ShouldReturnInverseMatrix) {
    ConcreteCamera camera(Vector3d(0, 0, -2), Vector3d::null);
    ASSERT_EQ(camera.matrix().inverted(), camera.inverseMatrix());
  }

  TEST(Camera, ClipSpaceProjectionIsUndefinedByDefault) {
    ConcreteCamera camera(Vector3d(0, 0, -2), Vector3d::null);
    EXPECT_TRUE(camera.projectPointToClipSpace(Vector3d::null).isUndefined());
  }

  TEST(Camera, ShouldRecalculateMatrixWhenPositionIsChanged) {
    ConcreteCamera camera;
    camera.setPosition(Vector3d(0, 0, -2));
    Matrix4d expected(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -2, 0, 0, 0, 1);
    ASSERT_EQ(expected, camera.matrix());
  }

  TEST(Camera, ShouldRecalculateInverseMatrixWhenPositionIsChanged) {
    ConcreteCamera camera(Vector3d(0, 0, -2), Vector3d::null);
    camera.inverseMatrix();
    camera.setPosition(Vector3d(0, 0, -3));

    Matrix4d expected(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 3, 0, 0, 0, 1);
    ASSERT_EQ(expected, camera.inverseMatrix());
  }

  TEST(Camera, ShouldRecalculateMatrixWhenTargetIsChanged) {
    ConcreteCamera camera;
    camera.setTarget(Vector3d(0, 0, 1));
    Matrix4d expected(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    ASSERT_EQ(expected, camera.matrix());
  }

  TEST(Camera, ShouldRecalculateInverseMatrixWhenTargetIsChanged) {
    ConcreteCamera camera(Vector3d(0, 0, -2), Vector3d::null);
    camera.inverseMatrix();
    camera.setTarget(Vector3d(0, 0, 1));

    ASSERT_EQ(camera.matrix().inverted(), camera.inverseMatrix());
  }

  TEST(Camera, DefaultAspectModeIsStretch) {
    ConcreteCamera camera;
    ASSERT_EQ(render::AspectMode::Stretch, camera.aspectMode());
  }

  TEST(Camera, CanSetAspectMode) {
    ConcreteCamera camera;
    camera.setAspectMode(render::AspectMode::FitWidth);
    ASSERT_EQ(render::AspectMode::FitWidth, camera.aspectMode());
  }

  TEST(Camera, AspectModeIsPropagatedToViewPlane) {
    ConcreteCamera camera;
    camera.setAspectMode(render::AspectMode::FitHeight);
    ASSERT_EQ(render::AspectMode::FitHeight, camera.viewPlane()->aspectMode());
  }

  TEST(Camera, AspectModeIsPropagatedToNewViewPlane) {
    ConcreteCamera camera;
    camera.setAspectMode(render::AspectMode::FitWidth);
    auto plane = std::make_shared<render::ViewPlane>();
    camera.setViewPlane(plane);
    ASSERT_EQ(render::AspectMode::FitWidth, plane->aspectMode());
  }

  TEST(Camera, AspectRatioIsPropagatedToViewPlane) {
    ConcreteCamera camera;
    camera.setAspectRatio(16.0 / 9.0);
    ASSERT_NEAR(16.0 / 9.0, camera.viewPlane()->aspectRatio(), 0.001);
  }

  TEST(Camera, AspectSettingsArePreservedInClone) {
    ConcreteCamera camera;
    camera.setAspectMode(render::AspectMode::FitExact);
    camera.setAspectRatio(2.39);
    auto clone = camera.clone();
    ASSERT_EQ(render::AspectMode::FitExact, clone->aspectMode());
    ASSERT_NEAR(2.39, clone->aspectRatio(), 0.001);
  }

  TEST(Camera, ShouldSetViewPlane) {
    ConcreteCamera camera;
    auto plane = std::make_shared<render::ViewPlane>();
    camera.setViewPlane(plane);
    ASSERT_EQ(plane, camera.viewPlane());
  }

  TEST(Camera, ShouldDeleteOldViewPlaneWhenNewIsSet) {
    ConcreteCamera camera;
    auto plane = std::make_shared<testing::NiceMock<MockViewPlane>>();
    plane->expectDestructorCall();
    camera.setViewPlane(plane);
    camera.setViewPlane(std::make_shared<render::ViewPlane>());
  }

  TEST(Camera, ShouldReturnDefaultViewPlane) {
    ConcreteCamera camera;
    ASSERT_NE(static_cast<std::shared_ptr<render::ViewPlane>>(0), camera.viewPlane());
  }

  TEST(Camera, RenderableRectClipsToFitExactInnerRect) {
    ConcreteCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    camera.setAspectMode(render::AspectMode::FitExact);
    camera.setAspectRatio(4.0 / 3.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(16, 9));

    const Recti actual = camera.renderableRect(Recti(0, 0, 16, 9));
    ASSERT_EQ(2, actual.left());
    ASSERT_EQ(0, actual.top());
    ASSERT_EQ(12, actual.width());
    ASSERT_EQ(9, actual.height());
  }

  TEST(Camera, PrimaryRaySampleConsumesPixelAndTimeDimensions) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    camera.viewPlane()->setup(camera.matrix(), Recti(4, 4));
    auto pixel = camera.viewPlane()->begin(Recti(0, 0, 4, 4));

    auto sample = camera.primaryRaySample(pixel, 0, std::nullopt);

    ASSERT_TRUE(sample.has_value());
    ASSERT_TRUE(sample->ray.direction().isDefined());
    ASSERT_NE(nullptr, sample->sampleStream);
    ASSERT_GE(sample->timeSample, 0.0);
    ASSERT_LT(sample->timeSample, 1.0);
  }

  TEST(Camera, PrimaryRaySampleUsesCallerOwnedStream) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    auto sampler = std::make_shared<RegularSampler>();
    sampler->setup(4, 16);
    camera.viewPlane()->setSampler(sampler);
    camera.viewPlane()->setup(camera.matrix(), Recti(4, 4));
    auto pixel = camera.viewPlane()->begin(Recti(0, 0, 4, 4));
    const auto tileSeed = std::optional<std::uint64_t>(1234);

    auto retainedSample = camera.primaryRaySample(pixel, 2, tileSeed);
    auto stream = sampler->stream(2, camera.primaryRayPixelHash(pixel, tileSeed));
    auto borrowedSample = camera.primaryRaySample(pixel, *stream);

    ASSERT_TRUE(retainedSample.has_value());
    ASSERT_TRUE(borrowedSample.has_value());
    ASSERT_EQ(retainedSample->ray.origin(), borrowedSample->ray.origin());
    ASSERT_EQ(retainedSample->ray.direction(), borrowedSample->ray.direction());
    ASSERT_DOUBLE_EQ(retainedSample->timeSample, borrowedSample->timeSample);

    auto generatorStream = sampler->stream(2, camera.primaryRayPixelHash(pixel, tileSeed));
    auto generatedSample = camera.primaryRayGenerator()->sample(pixel, *generatorStream);

    ASSERT_TRUE(generatedSample.has_value());
    ASSERT_EQ(borrowedSample->ray.origin(), generatedSample->ray.origin());
    ASSERT_EQ(borrowedSample->ray.direction(), generatedSample->ray.direction());
    ASSERT_DOUBLE_EQ(borrowedSample->timeSample, generatedSample->timeSample);
  }

  TEST(Camera, ProgressiveSamplePublishingWritesRunningAveragesBeforeFinalSample) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    auto sampler = std::make_shared<RegularSampler>();
    sampler->setup(4, 1);
    camera.viewPlane()->setSampler(sampler);
    camera.viewPlane()->setup(camera.matrix(), Recti(1, 1));

    Buffer<unsigned int> buffer(1, 1);
    auto raycaster = std::make_shared<ProgressivePublishingRayCaster>(buffer);

    camera.render(raycaster, buffer, std::shared_ptr<render::Tonemap>(), Recti(0, 0, 1, 1));

    EXPECT_EQ(4, raycaster->calls());
    EXPECT_EQ(0x00ff0000u, raycaster->bufferBeforeSecondSample());
    EXPECT_EQ(Colord(0.25, 0.0, 0.0).rgb(), buffer[0][0]);
  }

  TEST(Camera, ProgressiveSamplePublishingWaitsForFullSamplePassBeforePublishing) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    auto sampler = std::make_shared<RegularSampler>();
    sampler->setup(4, 1);
    camera.viewPlane()->setSampler(sampler);
    camera.viewPlane()->setup(camera.matrix(), Recti(2, 1));

    Buffer<unsigned int> buffer(2, 1);
    buffer.clear();
    auto raycaster = std::make_shared<ProgressivePublishingRayCaster>(buffer);

    camera.render(raycaster, buffer, std::shared_ptr<render::Tonemap>(), Recti(0, 0, 2, 1));

    EXPECT_EQ(8, raycaster->calls());
    EXPECT_EQ(0u, raycaster->bufferBeforeSecondSample());
  }

  TEST(Camera, ProgressiveSamplePublishingUsesOneAccumulatorSlotPerTiledPixel) {
    PinholeCamera camera(Vector3d(0, 0, -5), Vector3d::null);
    auto sampler = std::make_shared<RegularSampler>();
    sampler->setup(4, 1);
    camera.viewPlane()->setSampler(sampler);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 640, 480));

    Buffer<unsigned int> buffer(32, 32);
    buffer.clear();
    auto raycaster = std::make_shared<ProgressivePublishingRayCaster>(buffer);

    camera.render(raycaster, buffer, std::shared_ptr<render::Tonemap>(), Recti(0, 0, 32, 32));

    EXPECT_EQ(32 * 32 * 4, raycaster->calls());
  }

  TEST(Camera, ShouldNotBeCancelledAfterConstruction) {
    ASSERT_FALSE(ConcreteCamera().isCancelled());
    ASSERT_FALSE(ConcreteCamera(Vector3d(), Vector3d()).isCancelled());
  }

  TEST(Camera, ShouldBeCancelledAfterCancellation) {
    ConcreteCamera camera;
    camera.cancel();
    ASSERT_TRUE(camera.isCancelled());
  }

  TEST(Camera, ShouldUncancel) {
    ConcreteCamera camera;
    camera.cancel();
    camera.uncancel();
    ASSERT_FALSE(camera.isCancelled());
  }
}
