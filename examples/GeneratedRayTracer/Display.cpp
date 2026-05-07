#include <QVBoxLayout>
#include <QSpacerItem>

#include <QMouseEvent>

#include "Display.h"
#include "raytracer/Raytracer.h"
#include "render/WireframeEngine.h"
#include "render/State.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/lights/PointLight.h"
#include "render/cameras/PinholeCamera.h"
#include "core/math/HitPointInterval.h"

#include "widgets/world/PropertyEditorWidget.h"

#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

using namespace std;

Display::Display(QWidget* parent)
  : QtDisplay(parent, std::make_shared<raytracer::Raytracer>(nullptr))
{
  // The QtDisplay base now holds the active engine in m_engine. Cache
  // the typed shared_ptr alongside so engine swaps don't have to
  // dynamic_cast the base pointer.
  m_raytracerEngine = std::dynamic_pointer_cast<raytracer::Raytracer>(m_engine);
  m_wireframeEngine = std::make_shared<render::WireframeEngine>(nullptr);
}

Display::~Display() {
}

void Display::setEngineKind(EngineKind kind) {
  // Stop any in-flight render before swapping; the new engine
  // inherits the old engine's scene + camera so the preview keeps
  // looking at the same thing.
  stop();

  std::shared_ptr<render::RenderEngine> next;
  if (kind == EngineKind::Wireframe) {
    m_wireframeEngine->setScene(m_engine->scene());
    m_wireframeEngine->setCamera(m_engine->camera());
    next = m_wireframeEngine;
  } else {
    m_raytracerEngine->setScene(m_engine->scene());
    m_raytracerEngine->setCamera(m_engine->camera());
    next = m_raytracerEngine;
  }
  setEngine(next);
  render();
}

void Display::setScene(Scene* scene) {
  if (m_engine->scene()) {
    stop();
    // Old scene is owned by the engine's shared_ptr; setScene below
    // swaps it out and the previous scene is destroyed.
  }

  m_engine->setScene(scene->toRaytracerScene());
  render();
}

void Display::mousePressEvent(QMouseEvent* event) {
  QtDisplay::mousePressEvent(event);

  if (event->modifiers() & Qt::ControlModifier) {
    // The Ctrl-click ray-state probe is raytracer-specific (no
    // ray recursion in wireframe / future raster engines), so it
    // only fires when the active engine is actually a Raytracer.
    auto rt = std::dynamic_pointer_cast<raytracer::Raytracer>(m_engine);
    if (!rt) return;

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

