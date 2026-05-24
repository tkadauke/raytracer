#include <gtest/gtest.h>

#include "widgets/QtDisplay.h"
#include "engine/raytracer/Raytracer.h"
#include "render/cameras/Camera.h"
#include "render/RenderEngine.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

#include <QSemaphore>

#include <atomic>

namespace QtDisplayTest {
  class QtDisplayTest : public ::testing::GuiTest {};

  class BlockingEngine : public render::RenderEngine {
  public:
    BlockingEngine()
        : render::RenderEngine(std::shared_ptr<render::Scene>()) {
    }

    void render(Buffer<Colord>&) override {
    }

    void render(Buffer<unsigned int>&) override {
      entered.release();
      release.acquire();
    }

    void cancel() override {
      ++cancelCalls;
      release.release();
    }

    void uncancel() override {
    }

    QSemaphore entered;
    QSemaphore release;
    std::atomic<int> cancelCalls{0};
  };

  TEST_F(QtDisplayTest, ShouldInitialize) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
  }

  TEST_F(QtDisplayTest, ShouldInitializeBufferAtDefaultWidgetSize) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    EXPECT_EQ(display.size(), display.bufferSize());
  }

  TEST_F(QtDisplayTest, ShouldDefaultToInteractive) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    EXPECT_TRUE(display.interactive());
  }

  TEST_F(QtDisplayTest, ShouldSetAndGetInteractive) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    display.setInteractive(false);
    EXPECT_FALSE(display.interactive());
  }

  TEST_F(QtDisplayTest, ShouldDefaultToCancelRenderOnInteraction) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    EXPECT_TRUE(display.cancelRenderOnInteraction());
  }

  TEST_F(QtDisplayTest, ShouldSetAndGetCancelRenderOnInteraction) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    display.setCancelRenderOnInteraction(false);
    EXPECT_FALSE(display.cancelRenderOnInteraction());
  }

  TEST_F(QtDisplayTest, ShouldCancelInFlightRenderWhenInteractionPolicyAllows) {
    auto engine = std::make_shared<BlockingEngine>();
    QtDisplay display(nullptr, engine);
    display.setBufferSize(QSize(4, 4));

    display.render();
    ASSERT_TRUE(engine->entered.tryAcquire(1, 1000));

    display.render();

    EXPECT_EQ(1, engine->cancelCalls.load());
  }

  TEST_F(QtDisplayTest, ShouldDeferInFlightRenderWhenInteractionPolicyDisallowsCancellation) {
    auto engine = std::make_shared<BlockingEngine>();
    QtDisplay display(nullptr, engine);
    display.setBufferSize(QSize(4, 4));
    display.setCancelRenderOnInteraction(false);

    display.render();
    ASSERT_TRUE(engine->entered.tryAcquire(1, 1000));

    display.render();

    EXPECT_EQ(0, engine->cancelCalls.load());
    engine->release.release();
  }

  TEST_F(QtDisplayTest, ShouldAcceptSetDistance) {
    // setDistance updates the internal interactive-camera distance the
    // wheel-event handler uses for zoom; no observable getter, so just
    // smoke-test the call.
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    QtDisplay display(nullptr, rt);
    display.setDistance(2.5);
  }

  TEST_F(QtDisplayTest, ShouldRenderFromInteractiveCameraPose) {
    auto engine = std::make_shared<BlockingEngine>();
    QtDisplay display(nullptr, engine);
    display.setBufferSize(QSize(4, 4));

    display.setInteractiveCameraPose(Vector3d(1, 2, 3), Vector3d(1, 2, 1));
    display.render();
    ASSERT_TRUE(engine->entered.tryAcquire(1, 1000));

    ASSERT_VECTOR_NEAR(Vector3d(1, 2, 3), engine->camera()->position(), 1e-9);
    ASSERT_VECTOR_NEAR(Vector3d(1, 2, 1), engine->camera()->target(), 1e-9);

    engine->release.release();
    display.stop();
  }
}
