#include <QVBoxLayout>
#include <QSpacerItem>

#include <QMouseEvent>

#include "Display.h"
#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Primitive.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/Light.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "core/math/HitPointInterval.h"

#include "widgets/world/PropertyEditorWidget.h"

#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

using namespace std;

Display::Display(QWidget* parent)
  : QtDisplay(parent, std::make_shared<raytracer::Raytracer>(nullptr))
{
}

Display::~Display() {
}

void Display::setScene(Scene* scene) {
  if (m_raytracer->scene()) {
    stop();
    delete m_raytracer->scene();
  }
  
  auto raytracerScene = scene->toRaytracerScene();
  
  auto light1 = new raytracer::Light(Vector3d(-30, -30, -10), Colord(0.4, 0.4, 0.4));
  raytracerScene->addLight(light1);
  
  m_raytracer->setScene(raytracerScene);
  render();
}

#include "Display.moc"
