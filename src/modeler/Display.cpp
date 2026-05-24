#include <QVBoxLayout>
#include <QSpacerItem>

#include <QMouseEvent>

#include <utility>

#include "Display.h"
#include "engine/graph/GraphRenderEngine.h"
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

using namespace std;

RenderDisplay::RenderDisplay(QWidget* parent)
    : QtDisplay(parent, std::make_shared<engine::graph::GraphRenderEngine>(nullptr)),
      m_previewPostProcessAA(engine::graph::RenderPostProcessAA::None) {
  m_graphEngine = std::dynamic_pointer_cast<engine::graph::GraphRenderEngine>(m_engine);
  m_raytracerEngine = std::make_shared<engine::raytracer::Raytracer>(nullptr);
  applyPreviewPolicy(EngineKind::Raytracer);
}

RenderDisplay::~RenderDisplay() {
}

void RenderDisplay::setEngineKind(EngineKind kind) {
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
  m_rasterizerPreviewShadowsEnabled = enabled;
  emit renderGraphInputsChanged();
  if (m_engineKind == EngineKind::Rasterizer)
    render();
}

bool RenderDisplay::rasterizerPreviewShadowsEnabled() const {
  return m_rasterizerPreviewShadowsEnabled;
}

void RenderDisplay::setPreviewPostProcessAA(engine::graph::RenderPostProcessAA aa) {
  m_previewPostProcessAA = aa;
  emit renderGraphInputsChanged();
  render();
}

engine::graph::RenderPostProcessAA RenderDisplay::previewPostProcessAA() const {
  return m_previewPostProcessAA;
}

void RenderDisplay::setWireframeOverlayEnabled(bool enabled) {
  m_wireframeOverlayEnabled = enabled;
  emit renderGraphInputsChanged();
  render();
}

bool RenderDisplay::wireframeOverlayEnabled() const {
  return m_wireframeOverlayEnabled;
}

void RenderDisplay::setScene(Scene* scene) {
  // In-flight preview renders use an engine snapshot, so replacing
  // the control engine's scene does not tear the scene out from
  // under the worker that is finishing the previous frame.
  m_engine->setScene(scene->toRaytracerScene());
  if (auto* camera = scene->activeCamera()) {
    m_engine->setCamera(camera->toRaytracer());
    setInteractiveCameraPose(camera->position(), camera->target());
  }
  setInteractive(true);
  render();
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
