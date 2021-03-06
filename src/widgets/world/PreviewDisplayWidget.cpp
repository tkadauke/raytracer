#include "widgets/world/PreviewDisplayWidget.h"
#include "world/objects/Material.h"
#include "world/objects/Camera.h"
#include "world/objects/Scene.h"
#include "raytracer/Primitives/Scene.h"
#include "raytracer/Primitives/Sphere.h"
#include "raytracer/Primitives/Plane.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/Raytracer.h"
#include "raytracer/lights/DirectionalLight.h"
#include "raytracer/textures/CheckerBoardTexture.h"
#include "raytracer/textures/ConstantColorTexture.h"
#include "raytracer/textures/mappings/PlanarMapping2D.h"
#include "raytracer/cameras/PinholeCamera.h"

PreviewDisplayWidget::PreviewDisplayWidget(QWidget* parent)
  : QtDisplay(parent, std::make_shared<raytracer::Raytracer>(nullptr))
{
}

PreviewDisplayWidget::~PreviewDisplayWidget() {
}

QSize PreviewDisplayWidget::sizeHint() const {
  return QSize(256, 25);
}

void PreviewDisplayWidget::clear() {
  updateScene([&]() {
    m_raytracer->setScene(nullptr);
  });
}

void PreviewDisplayWidget::setMaterial(Material* material, Scene* s) {
  setInteractive(true);
  updateScene([&]() {
    m_raytracer->setScene(sphereOnPlane(material, s));
    m_raytracer->setCamera(std::make_shared<raytracer::PinholeCamera>());
  });
}

void PreviewDisplayWidget::setCamera(Camera* camera, Scene* scene) {
  setInteractive(false);
  updateScene([&]() {
    m_raytracer->setScene(scene->toRaytracerScene());
    m_raytracer->setCamera(camera->toRaytracer());
  });
}

void PreviewDisplayWidget::updateScene(const std::function<void()>& setup) {
  if (m_raytracer->scene()) {
    stop();
    delete m_raytracer->scene();
  }

  setup();

  render();
}

raytracer::Scene* PreviewDisplayWidget::sphereOnPlane(Material* material, Scene* s) const {
  auto mat = material->toRaytracerMaterial();
  auto scene = new raytracer::Scene;

  scene->setAmbient(s->ambient());
  scene->setBackground(s->background());

  auto sphere = std::make_shared<raytracer::Sphere>(Vector3d(0, 0, 0), 2);
  sphere->setMaterial(mat);

  auto planeMaterial = std::make_shared<raytracer::MatteMaterial>(
    std::make_shared<raytracer::CheckerBoardTexture>(
      new raytracer::PlanarMapping2D,
      std::make_shared<raytracer::ConstantColorTexture>(Colord::black()),
      std::make_shared<raytracer::ConstantColorTexture>(Colord::white())
    )
  );

  auto plane = std::make_shared<raytracer::Plane>(Vector3d(0, -1, 0), 2);
  plane->setMaterial(planeMaterial);

  scene->add(sphere);
  scene->add(plane);

  auto light = std::make_shared<raytracer::DirectionalLight>(Vector3d(-0.5, -1, -0.5), Colord(1, 1, 1));
  scene->addLight(light);

  return scene;
}

#include "PreviewDisplayWidget.moc"
