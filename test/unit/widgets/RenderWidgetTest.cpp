#include <gtest/gtest.h>

#include "widgets/RenderWidget.h"
#include "engine/raytracer/Raytracer.h"
#include "render/RenderEngine.h"
#include "core/Buffer.h"

#include "test/helpers/GuiTestHelper.h"

#include <QCoreApplication>
#include <QPainter>
#include <QSemaphore>

#include <atomic>
#include <utility>
#include <vector>

namespace RenderWidgetTest {
  class RenderWidgetTest : public ::testing::GuiTest {};

  class SolidColorEngine : public render::RenderEngine {
  public:
    explicit SolidColorEngine(unsigned int color)
      : render::RenderEngine(std::shared_ptr<render::Scene>()),
        color(color)
    {
    }

    void render(Buffer<Colord>&) override {
    }

    void render(Buffer<unsigned int>& buffer) override {
      for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
          buffer[y][x] = color;
        }
      }
      done.release();
    }

    void cancel() override {
    }

    void uncancel() override {
    }

    unsigned int color;
    QSemaphore done;
  };

  class BlockingEngine : public render::RenderEngine {
  public:
    BlockingEngine()
      : render::RenderEngine(std::shared_ptr<render::Scene>())
    {
    }

    void render(Buffer<Colord>&) override {
    }

    void render(Buffer<unsigned int>&) override {
      entered.release();
      release.acquire();
    }

    void cancel() override {
      release.release();
    }

    void uncancel() override {
    }

    QSemaphore entered;
    QSemaphore release;
  };

  struct CloneGate {
    QSemaphore entered;
    QSemaphore release;
    QSemaphore finished;
    std::atomic<int> cancelCalls{0};
    std::atomic<bool> releaseOnCancel{true};
  };

  struct CloneState {
    std::vector<std::shared_ptr<CloneGate>> gates;
  };

  class CloneableBlockingEngine : public render::RenderEngine {
  public:
    CloneableBlockingEngine()
      : render::RenderEngine(std::shared_ptr<render::Scene>()),
        state(std::make_shared<CloneState>())
    {
    }

    explicit CloneableBlockingEngine(std::shared_ptr<CloneState> state)
      : render::RenderEngine(std::shared_ptr<render::Scene>()),
        state(std::move(state)),
        gate(std::make_shared<CloneGate>())
    {
    }

    std::shared_ptr<render::RenderEngine> cloneForRender() const override {
      auto clone = std::make_shared<CloneableBlockingEngine>(state);
      state->gates.push_back(clone->gate);
      return clone;
    }

    void render(Buffer<Colord>&) override {
    }

    void render(Buffer<unsigned int>&) override {
      gate->entered.release();
      gate->release.acquire();
      gate->finished.release();
    }

    void cancel() override {
      ++gate->cancelCalls;
      if (gate->releaseOnCancel.load()) {
        gate->release.release();
      }
    }

    void uncancel() override {
    }

    std::shared_ptr<CloneState> state;
    std::shared_ptr<CloneGate> gate;
  };

  QRgb paintedPixel(RenderWidget& widget) {
    QImage image(widget.size(), QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    widget.QWidget::render(&painter);
    return image.pixel(0, 0);
  }

  TEST_F(RenderWidgetTest, ShouldInitializeWithNullScene) {
    // RenderWidget owns a Raytracer (held shared) but the scene pointer
    // can be null — the render path short-circuits when there's nothing
    // to draw. Smoke-test construction with a no-scene Raytracer.
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);
  }

  TEST_F(RenderWidgetTest, ShouldAcceptSetBufferSize) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);
    widget.setBufferSize(QSize(100, 50));
  }

  TEST_F(RenderWidgetTest, ShouldAcceptSetShowProgressIndicators) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);
    widget.setShowProgressIndicators(true);
    widget.setShowProgressIndicators(false);
  }

  TEST_F(RenderWidgetTest, ShouldAcceptDisplayModes) {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);

    widget.setDisplayMode(RenderWidget::DisplayMode::PeriodicUpdate);
    EXPECT_EQ(RenderWidget::DisplayMode::PeriodicUpdate, widget.displayMode());

    widget.setDisplayMode(RenderWidget::DisplayMode::CompletedTilePublishing);
    EXPECT_EQ(RenderWidget::DisplayMode::CompletedTilePublishing, widget.displayMode());

    widget.setDisplayMode(RenderWidget::DisplayMode::DoubleBuffer);
    EXPECT_EQ(RenderWidget::DisplayMode::DoubleBuffer, widget.displayMode());
  }

  TEST_F(RenderWidgetTest, ShouldKeepFrontBufferVisibleWhileNextRenderStarts) {
    auto solid = std::make_shared<SolidColorEngine>(qRgb(0, 255, 0));
    RenderWidget widget(nullptr, solid);
    widget.setBufferSize(QSize(2, 2));
    widget.resize(2, 2);

    widget.render();
    ASSERT_TRUE(solid->done.tryAcquire(1, 5000));
    widget.stop();
    ASSERT_EQ(qRgb(0, 255, 0), paintedPixel(widget));

    auto blocking = std::make_shared<BlockingEngine>();
    widget.setEngine(blocking);
    widget.setDisplayMode(RenderWidget::DisplayMode::DoubleBuffer);

    widget.render();
    ASSERT_TRUE(blocking->entered.tryAcquire(1, 5000));

    EXPECT_EQ(qRgb(0, 255, 0), paintedPixel(widget));
  }

  TEST_F(RenderWidgetTest, ShouldReuseBackBufferForPeriodicPreviewRenders) {
    auto solid = std::make_shared<SolidColorEngine>(qRgb(0, 255, 0));
    RenderWidget widget(nullptr, solid);
    widget.setBufferSize(QSize(2, 2));
    widget.resize(2, 2);

    widget.render();
    ASSERT_TRUE(solid->done.tryAcquire(1, 5000));
    widget.stop();

    auto blocking = std::make_shared<BlockingEngine>();
    widget.setEngine(blocking);
    widget.setDisplayMode(RenderWidget::DisplayMode::PeriodicUpdate);
    widget.setClearBackBufferOnRenderStart(false);

    widget.render();
    ASSERT_TRUE(blocking->entered.tryAcquire(1, 5000));
    widget.timerEvent(nullptr);

    EXPECT_EQ(qRgb(0, 255, 0), paintedPixel(widget));
  }

  TEST_F(RenderWidgetTest, ShouldIgnoreCompletionFromStoppedRenderThread) {
    auto first = std::make_shared<BlockingEngine>();
    RenderWidget widget(nullptr, first);
    widget.setBufferSize(QSize(2, 2));
    widget.setDisplayMode(RenderWidget::DisplayMode::PeriodicUpdate);

    int finishedCount = 0;
    QObject::connect(&widget, &RenderWidget::finished, [&finishedCount]() {
      ++finishedCount;
    });

    widget.render();
    ASSERT_TRUE(first->entered.tryAcquire(1, 5000));
    widget.stop();

    auto second = std::make_shared<BlockingEngine>();
    widget.setEngine(second);
    widget.render();
    ASSERT_TRUE(second->entered.tryAcquire(1, 5000));

    QCoreApplication::processEvents();

    EXPECT_TRUE(widget.isRendering());
    EXPECT_EQ(0, finishedCount);
  }

  TEST_F(RenderWidgetTest, ShouldStartReplacementRenderWithoutWaitingForCancelledSnapshot) {
    auto engine = std::make_shared<CloneableBlockingEngine>();
    RenderWidget widget(nullptr, engine);
    widget.setBufferSize(QSize(2, 2));

    int finishedCount = 0;
    QObject::connect(&widget, &RenderWidget::finished, [&finishedCount]() {
      ++finishedCount;
    });

    widget.render();
    ASSERT_EQ(1u, engine->state->gates.size());
    ASSERT_TRUE(engine->state->gates[0]->entered.tryAcquire(1, 5000));

    widget.render();

    ASSERT_EQ(2u, engine->state->gates.size());
    EXPECT_EQ(1, engine->state->gates[0]->cancelCalls.load());
    ASSERT_TRUE(engine->state->gates[0]->finished.tryAcquire(1, 5000));
    QCoreApplication::processEvents();
    EXPECT_EQ(0, finishedCount);

    EXPECT_TRUE(engine->state->gates[1]->entered.tryAcquire(1, 5000));
    EXPECT_TRUE(widget.isRendering());

    engine->state->gates[1]->release.release();
    ASSERT_TRUE(engine->state->gates[1]->finished.tryAcquire(1, 5000));
    QCoreApplication::processEvents();
    EXPECT_EQ(1, finishedCount);
  }

  TEST_F(RenderWidgetTest, ShouldNotStartUnlimitedReplacementRendersWhileRetiredSnapshotDrains) {
    auto engine = std::make_shared<CloneableBlockingEngine>();
    RenderWidget widget(nullptr, engine);
    widget.setBufferSize(QSize(2, 2));

    widget.render();
    ASSERT_EQ(1u, engine->state->gates.size());
    ASSERT_TRUE(engine->state->gates[0]->entered.tryAcquire(1, 5000));
    engine->state->gates[0]->releaseOnCancel.store(false);

    widget.render();
    ASSERT_EQ(2u, engine->state->gates.size());
    ASSERT_TRUE(engine->state->gates[1]->entered.tryAcquire(1, 5000));

    widget.render();

    EXPECT_EQ(2u, engine->state->gates.size());
    EXPECT_EQ(1, engine->state->gates[1]->cancelCalls.load());

    engine->state->gates[0]->release.release();
    engine->state->gates[1]->release.release();
  }

  TEST_F(RenderWidgetTest, ShouldAcceptStopBeforeRender) {
    // Calling stop() with no in-flight render thread must not crash —
    // covers the "user cancels before starting" race that would
    // otherwise dereference a null thread pointer.
    auto rt = std::make_shared<engine::raytracer::Raytracer>(nullptr);
    RenderWidget widget(nullptr, rt);
    widget.stop();
  }
}
