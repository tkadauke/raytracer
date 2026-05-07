#include "widgets/world/PreviewDisplayWidget.h"
#include "world/objects/Material.h"
#include "world/objects/Camera.h"
#include "world/objects/Scene.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Plane.h"
#include "render/materials/MatteMaterial.h"
#include "raytracer/Raytracer.h"
#include "render/lights/DirectionalLight.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"
#include "render/cameras/PinholeCamera.h"

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
    m_engine->setScene(nullptr);
  });
}

void PreviewDisplayWidget::setMaterial(Material* material, Scene* s) {
  setInteractive(true);
  updateScene([&]() {
    m_engine->setScene(sphereOnPlane(material, s));
    m_engine->setCamera(std::make_shared<render::PinholeCamera>());
  });
}

void PreviewDisplayWidget::setCamera(Camera* camera, Scene* scene) {
  setInteractive(false);
  updateScene([&]() {
    m_engine->setScene(scene->toRaytracerScene());
    m_engine->setCamera(camera->toRaytracer());
  });
}

void PreviewDisplayWidget::updateScene(const std::function<void()>& setup) {
  // Stop any in-flight render before tearing down the previous scene; the
  // shared_ptr swap in setScene below frees the old one.
  if (m_engine->scene()) {
    stop();
  }

  setup();

  render();
}

std::shared_ptr<render::Scene> PreviewDisplayWidget::sphereOnPlane(Material* material, Scene* s) const {
  auto mat = material->toRaytracerMaterial();
  auto scene = std::make_shared<render::Scene>();

  scene->setAmbient(s->ambient());
  scene->setBackground(s->background());

  auto sphere = std::make_shared<render::Sphere>(Vector3d(0, 0, 0), 2);
  sphere->setMaterial(mat);

  auto planeMaterial = std::make_shared<render::MatteMaterial>(
    std::make_shared<render::CheckerBoardTexture>(
      new render::PlanarMapping2D,
      std::make_shared<render::ConstantColorTexture>(Colord::black()),
      std::make_shared<render::ConstantColorTexture>(Colord::white())
    )
  );

  auto plane = std::make_shared<render::Plane>(Vector3d(0, -1, 0), 2);
  plane->setMaterial(planeMaterial);

  scene->add(sphere);
  scene->add(plane);

  auto light = std::make_shared<render::DirectionalLight>(Vector3d(-0.5, -1, -0.5), Colord(1, 1, 1));
  scene->addLight(light);

  return scene;
}

