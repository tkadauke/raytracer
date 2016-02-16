#include "widgets/world/MaterialDisplayWidget.h"
#include "world/objects/Material.h"
#include "raytracer/Primitives/Scene.h"
#include "raytracer/Primitives/Sphere.h"
#include "raytracer/Primitives/Plane.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/Raytracer.h"
#include "raytracer/Light.h"

MaterialDisplayWidget::MaterialDisplayWidget(QWidget* parent)
  : QtDisplay(parent, new raytracer::Raytracer(0)), m_material(nullptr)
{
}

MaterialDisplayWidget::~MaterialDisplayWidget() {
}

QSize MaterialDisplayWidget::sizeHint() const {
  return QSize(256, 25);
}

void MaterialDisplayWidget::setMaterial(Material* material) {
  if (m_raytracer->scene()) {
    stop();
    delete m_raytracer->scene();
  }
  
  m_material = material;
  
  if (m_material) {
    m_raytracer->setScene(sphereOnPlane());
  } else {
    m_raytracer->setScene(nullptr);
  }
  render();
}

raytracer::Scene* MaterialDisplayWidget::sphereOnPlane() {
  auto mat = m_material->toRaytracerMaterial();
  auto scene = new raytracer::Scene;
  
  scene->setAmbient(Colord(0.4, 0.4, 0.4));

  auto sphere = new raytracer::Sphere(Vector3d(0, 0, 0), 2);
  sphere->setMaterial(mat);

  auto background = new raytracer::MatteMaterial(Colord(0, 0, 1));
  auto plane = new raytracer::Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(background);

  scene->add(sphere);
  scene->add(plane);

  auto light = new raytracer::Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light);
  
  return scene;
}

#include "MaterialDisplayWidget.moc"
