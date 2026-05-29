#include <QVBoxLayout>
#include <QSpacerItem>

#include <QMetaObject>
#include <QMouseEvent>
#include <QPointer>

#include <utility>

#include "Display.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphExecutionObserver.h"
#include "engine/graph/RenderPlan.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/raytracer/Raytracer.h"
#include "render/State.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/lights/PointLight.h"
#include "render/cameras/PinholeCamera.h"
#include "core/math/HitPointInterval.h"

#include "widgets/world/PropertyEditorWidget.h"

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"
#include "world/objects/Sphere.h"
#include "world/objects/StepVisibilityEvaluator.h"

using namespace std;

namespace {
  class ModelerRenderGraphExecutionObserver : public engine::graph::RenderGraphExecutionObserver {
  public:
    explicit ModelerRenderGraphExecutionObserver(RenderDisplay* display)
        : m_display(display) {
    }

    void renderStarted(std::uint64_t generation) override {
      invoke([generation](RenderDisplay& display) {
        display.notifyRenderGraphExecutionStarted(generation);
      });
    }

    void passStarted(const engine::graph::RenderPassId& passId) override {
      invoke([passId](RenderDisplay& display) {
        display.notifyRenderGraphPassStarted(QString::fromStdString(passId), 0);
      });
    }

    void passStarted(const engine::graph::RenderPassId& passId, std::uint64_t generation) override {
      invoke([passId, generation](RenderDisplay& display) {
        display.notifyRenderGraphPassStarted(QString::fromStdString(passId), generation);
      });
    }

    void passFinished(const engine::graph::RenderPassId& passId) override {
      invoke([passId](RenderDisplay& display) {
        display.notifyRenderGraphPassFinished(QString::fromStdString(passId), 0);
      });
    }

    void passFinished(const engine::graph::RenderPassId& passId,
                      std::uint64_t generation) override {
      invoke([passId, generation](RenderDisplay& display) {
        display.notifyRenderGraphPassFinished(QString::fromStdString(passId), generation);
      });
    }

    void passFailed(const engine::graph::RenderPassId& passId,
                    const std::string& message) override {
      invoke([passId, message](RenderDisplay& display) {
        display.notifyRenderGraphPassFailed(QString::fromStdString(passId),
                                            QString::fromStdString(message), 0);
      });
    }

    void passFailed(const engine::graph::RenderPassId& passId, const std::string& message,
                    std::uint64_t generation) override {
      invoke([passId, message, generation](RenderDisplay& display) {
        display.notifyRenderGraphPassFailed(QString::fromStdString(passId),
                                            QString::fromStdString(message), generation);
      });
    }

  private:
    template<class Callback>
    void invoke(Callback callback) {
      QPointer<RenderDisplay> display = m_display;
      if (!display)
        return;

      QMetaObject::invokeMethod(
        display,
        [display, callback] {
          if (display)
            callback(*display);
        },
        Qt::QueuedConnection);
    }

    QPointer<RenderDisplay> m_display;
  };
}

RenderDisplay::RenderDisplay(QWidget* parent)
    : QtDisplay(parent, std::make_shared<engine::graph::GraphRenderEngine>(nullptr)),
      m_previewPostProcessAA(engine::graph::RenderPostProcessAA::None),
      m_previewViewMode(engine::graph::RenderViewMode::Beauty) {
  m_graphEngine = std::dynamic_pointer_cast<engine::graph::GraphRenderEngine>(m_engine);
  m_graphExecutionObserver = std::make_shared<ModelerRenderGraphExecutionObserver>(this);
  if (m_graphEngine) {
    m_graphEngine->setExecutionObserver(m_graphExecutionObserver);
    m_graphEngine->setExecutionTraceEnabled(true);
  }
  m_raytracerEngine = std::make_shared<engine::raytracer::Raytracer>(nullptr);
  applyPreviewPolicy(EngineKind::Raytracer);
}

RenderDisplay::~RenderDisplay() {
}

void RenderDisplay::setEngineKind(EngineKind kind) {
  if (m_engineKind == kind)
    return;

  m_engineKind = kind;

  // Stop any in-flight render before the graph is recompiled for a different
  // executor. The MainWindow handles compilation through
  // renderGraphInputsChanged().
  stop();
  applyPreviewPolicy(kind);
  emit renderGraphInputsChanged();
  render();
}

RenderDisplay::EngineKind RenderDisplay::engineKind() const {
  return m_engineKind;
}

void RenderDisplay::setRasterizerPreviewShadowsEnabled(bool enabled) {
  if (m_rasterizerPreviewShadowsEnabled == enabled)
    return;

  m_rasterizerPreviewShadowsEnabled = enabled;
  emit renderGraphInputsChanged();
  if (m_engineKind == EngineKind::Rasterizer)
    render();
}

bool RenderDisplay::rasterizerPreviewShadowsEnabled() const {
  return m_rasterizerPreviewShadowsEnabled;
}

void RenderDisplay::setRasterizerPreviewBackend(engine::raster::RasterBackend backend) {
  if (m_rasterizerPreviewBackend == backend)
    return;

  m_rasterizerPreviewBackend = backend;
  emit renderGraphInputsChanged();
  if (m_engineKind == EngineKind::Rasterizer)
    render();
}

engine::raster::RasterBackend RenderDisplay::rasterizerPreviewBackend() const {
  return m_rasterizerPreviewBackend;
}

void RenderDisplay::setPreviewPostProcessAA(engine::graph::RenderPostProcessAA aa) {
  if (m_previewPostProcessAA == aa)
    return;

  m_previewPostProcessAA = aa;
  emit renderGraphInputsChanged();
  render();
}

engine::graph::RenderPostProcessAA RenderDisplay::previewPostProcessAA() const {
  return m_previewPostProcessAA;
}

void RenderDisplay::setPreviewViewMode(engine::graph::RenderViewMode viewMode) {
  if (m_previewViewMode == viewMode)
    return;

  m_previewViewMode = viewMode;
  emit renderGraphInputsChanged();
  render();
}

engine::graph::RenderViewMode RenderDisplay::previewViewMode() const {
  return m_previewViewMode;
}

void RenderDisplay::setWireframeOverlayEnabled(bool enabled) {
  if (m_wireframeOverlayEnabled == enabled)
    return;

  m_wireframeOverlayEnabled = enabled;
  emit renderGraphInputsChanged();
  render();
}

bool RenderDisplay::wireframeOverlayEnabled() const {
  return m_wireframeOverlayEnabled;
}

void RenderDisplay::setScene(Scene* scene) {
  setScene(scene, StepPlaybackStyle());
}

void RenderDisplay::setScene(Scene* scene, const StepPlaybackStyle& playbackStyle,
                             CameraPolicy cameraPolicy) {
  // In-flight preview renders use an engine snapshot, so replacing
  // the control engine's scene does not tear the scene out from
  // under the worker that is finishing the previous frame.
  m_engine->setScene(scene->toRaytracerScene(playbackStyle));
  if (m_graphEngine) {
    m_graphEngine->setSceneAnalysis(scene->renderGraphAnalysis());
    bindSceneCameras(*scene);
  }
  const bool needsSceneCamera =
    cameraPolicy == CameraPolicy::ResetToSceneCamera || !m_engine->camera();
  if (needsSceneCamera) {
    if (auto* camera = scene->activeCamera()) {
      m_engine->setCamera(camera->toRaytracer());
      setInteractiveCameraPose(camera->position(), camera->target());
      if (m_graphEngine && !camera->id().isEmpty()) {
        m_graphEngine->setSceneCamera(camera->id().toStdString(), m_engine->camera());
      }
    }
  }
  setInteractive(true);
  render();
}

void RenderDisplay::notifyRenderGraphExecutionStarted(std::uint64_t generation) {
  m_graphExecutionGeneration = generation;
  m_waitingForGraphExecutionStart = false;
  emit renderGraphExecutionStarted();
}

void RenderDisplay::notifyRenderGraphPassStarted(const QString& passId, std::uint64_t generation) {
  if (m_waitingForGraphExecutionStart ||
      (generation != 0 && generation != m_graphExecutionGeneration)) {
    return;
  }
  emit renderGraphPassStarted(passId);
}

void RenderDisplay::notifyRenderGraphPassFinished(const QString& passId, std::uint64_t generation) {
  if (m_waitingForGraphExecutionStart ||
      (generation != 0 && generation != m_graphExecutionGeneration)) {
    return;
  }
  emit renderGraphPassFinished(passId);
}

void RenderDisplay::notifyRenderGraphPassFailed(const QString& passId, const QString& message,
                                                std::uint64_t generation) {
  if (m_waitingForGraphExecutionStart ||
      (generation != 0 && generation != m_graphExecutionGeneration)) {
    return;
  }
  emit renderGraphPassFailed(passId, message);
}

std::shared_ptr<const engine::graph::RenderGraphExecutionTrace>
RenderDisplay::lastRenderGraphExecutionTrace() const {
  return m_graphEngine ? m_graphEngine->lastExecutionTrace() : nullptr;
}

std::shared_ptr<const engine::graph::RenderGraphExecutionTrace>
RenderDisplay::lastRenderGraphExecutionTraceForPlan(const engine::graph::RenderPlan& plan) const {
  return m_graphEngine ? m_graphEngine->lastExecutionTraceForPlan(plan) : nullptr;
}

void RenderDisplay::setRenderGraphIntent(const engine::graph::RenderIntent& intent) {
  if (m_graphEngine)
    m_graphEngine->setIntent(intent);
}

void RenderDisplay::setRenderGraphPlan(const engine::graph::RenderPlan& plan) {
  if (m_graphEngine) {
    m_graphEngine->setPlan(plan);
  }
  setRenderGraphPreviewEnabled(true);
}

void RenderDisplay::setRenderGraphPreviewEnabled(bool enabled) {
  if (m_renderGraphPreviewEnabled == enabled)
    return;

  m_renderGraphPreviewEnabled = enabled;
  if (!enabled)
    stop();
}

bool RenderDisplay::renderGraphPreviewEnabled() const {
  return m_renderGraphPreviewEnabled;
}

void RenderDisplay::setPreviewTonemap(std::shared_ptr<render::Tonemap> tonemap) {
  m_engine->setTonemap(std::move(tonemap));
  render();
}

void RenderDisplay::render() {
  if (!m_renderGraphPreviewEnabled)
    return;

  m_waitingForGraphExecutionStart = true;
  emit renderGraphExecutionStarted();
  QtDisplay::render();
}

void RenderDisplay::resizeEvent(QResizeEvent*) {
  stop();
  setBufferSize(size());
  emit renderGraphInputsChanged();
  render();
}

void RenderDisplay::applyPreviewPolicy(EngineKind kind) {
  setShowProgressIndicators(false);
  if (m_raytracerEngine) {
    m_raytracerEngine->setShowProgressIndicators(false);
  }

  if (kind == EngineKind::Raytracer) {
    setDisplayMode(RenderWidget::DisplayMode::PeriodicUpdate);
    setClearBackBufferOnRenderStart(false);
    setProgressUpdateIntervalMs(16);
    setCancelRenderOnInteraction(true);
    return;
  }

  setDisplayMode(RenderWidget::DisplayMode::DoubleBuffer);
  setClearBackBufferOnRenderStart(true);
  setProgressUpdateIntervalMs(0);
  setCancelRenderOnInteraction(false);
}

void RenderDisplay::bindSceneCameras(const Scene& scene) {
  if (!m_graphEngine)
    return;

  m_graphEngine->clearSceneCameras();
  for (const Camera* camera : scene.cameras()) {
    if (!camera->id().isEmpty()) {
      m_graphEngine->setSceneCamera(camera->id().toStdString(), camera->toRaytracer());
    }
  }
}

void RenderDisplay::mousePressEvent(QMouseEvent* event) {
  QtDisplay::mousePressEvent(event);

  if (event->modifiers() & Qt::ControlModifier) {
    // The Ctrl-click ray-state probe is raytracer-specific (no
    // ray recursion in wireframe / future raster engines), so it
    // only fires when the preview graph is using the raytracer executor.
    if (m_engineKind != EngineKind::Raytracer)
      return;

    m_raytracerEngine->setScene(m_engine->scene());
    m_raytracerEngine->setCamera(m_engine->camera());
    auto rt = m_raytracerEngine;

    Rayd ray = m_engine->camera()->rayForPixel(event->pos().x(), event->pos().y());
    if (ray.direction().isDefined()) {
      auto state = rt->rayState(ray);

      cout << state.hitPoint.primitive() << " - " << state.hitPoint << endl;
      cout << "numRays: " << state.numRays << endl;
      cout << "maxRecursionDepth: " << state.maxRecursionDepth << endl;
      cout << "intersectionHits: " << state.intersectionHits << endl;
      cout << "intersectionMisses: " << state.intersectionMisses << endl;
      cout << "shadowIntersectionHits: " << state.shadowIntersectionHits << endl;
      cout << "shadowIntersectionMisses: " << state.shadowIntersectionMisses << endl;

      for (const auto& event : *state.events) {
        cout << event << endl;
      }
    }
  }
}
