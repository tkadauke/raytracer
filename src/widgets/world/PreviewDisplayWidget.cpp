#include "widgets/world/PreviewDisplayWidget.h"
#include "world/objects/Material.h"
#include "raytracer/Primitives/Scene.h"
#include "raytracer/Primitives/Sphere.h"
#include "raytracer/Primitives/Plane.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/Raytracer.h"
#include "raytracer/Light.h"
#include "raytracer/textures/CheckerBoardTexture.h"
#include "raytracer/textures/ConstantColorTexture.h"
#include "raytracer/textures/mappings/PlanarMapping2D.h"

PreviewDisplayWidget::PreviewDisplayWidget(QWidget* parent)
  : QtDisplay(parent, std::make_shared<raytracer::Raytracer>(nullptr)), m_material(nullptr)
{
}

PreviewDisplayWidget::~PreviewDisplayWidget() {
}

QSize PreviewDisplayWidget::sizeHint() const {
  return QSize(256, 25);
}

void PreviewDisplayWidget::setMaterial(Material* material) {
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

raytracer::Scene* PreviewDisplayWidget::sphereOnPlane() {
  auto mat = m_material->toRaytracerMaterial();
  auto scene = new raytracer::Scene;
  
  scene->setAmbient(Colord(0.4, 0.4, 0.4));

  auto sphere = std::make_shared<raytracer::Sphere>(Vector3d(0, 0, 0), 2);
  sphere->setMaterial(mat);

  auto planeMaterial = new raytracer::MatteMaterial(
    std::make_shared<raytracer::CheckerBoardTexture>(
      new raytracer::PlanarMapping2D,
      std::make_shared<raytracer::ConstantColorTexture>(Colord::black()),
      std::make_shared<raytracer::ConstantColorTexture>(Colord::white())
    )
  );
  
  auto plane = std::make_shared<raytracer::Plane>(Vector3d(0, 1, 0), -2);
  plane->setMaterial(planeMaterial);

  scene->add(sphere);
  scene->add(plane);

  auto light = new raytracer::Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light);
  
  return scene;
}

#include "PreviewDisplayWidget.moc"
