#include "SceneFactory.h"

#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/lights/PointLight.h"
#include "raytracer/primitives/Intersection.h"
#include "raytracer/primitives/Union.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

using namespace raytracer;

class LensScene : public Scene {
public:
  LensScene();

private:
  std::shared_ptr<ReflectiveMaterial> m_red;
  std::shared_ptr<TransparentMaterial> m_glass;
  std::shared_ptr<PhongMaterial> m_blue;
};

LensScene::LensScene()
  : Scene()
{
  m_red = std::make_shared<ReflectiveMaterial>();
  m_glass = std::make_shared<TransparentMaterial>();
  m_blue = std::make_shared<PhongMaterial>(std::make_shared<ConstantColorTexture>(Colord(0, 0, 1)));

  setAmbient(Colord(0.4, 0.4, 0.4));

  auto sphere1 = std::make_shared<Sphere>(Vector3d(0, 1, 0), 1);
  auto sphere2 = std::make_shared<Sphere>(Vector3d(1, 1, 0), 1);
  auto i = std::make_shared<Intersection>();
  i->add(sphere1);
  i->add(sphere2);

  m_glass->setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(0.1, 0.1, 0.1)));
  m_glass->setRefractionIndex(1.52);

  i->setMaterial(m_glass);

  auto sphere3 = std::make_shared<Sphere>(Vector3d(3, 1, 0), 1);
  m_red->setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0)));
  m_red->setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(m_red);

  auto plane = std::make_shared<Plane>(Vector3d(0, -1, 0), 2);
  plane->setMaterial(m_blue);

  add(i);
  add(sphere3);
  add(plane);

  auto light1 = std::make_shared<PointLight>(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<LensScene>("Lens");
