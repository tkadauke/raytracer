#include "SceneFactory.h"

#include "render/primitives/Sphere.h"
#include "render/primitives/Plane.h"
#include "render/lights/PointLight.h"
#include "render/primitives/Intersection.h"
#include "render/primitives/Union.h"
#include "render/materials/Material.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/textures/ConstantColorTexture.h"

using namespace render;
using namespace render;

class StackedSpheresScene : public Scene {
public:
  StackedSpheresScene();

private:
  std::shared_ptr<ReflectiveMaterial> m_red;
  std::shared_ptr<TransparentMaterial> m_glass;
  std::shared_ptr<PhongMaterial> m_blue;
};

StackedSpheresScene::StackedSpheresScene()
  : Scene()
{
  m_red = std::make_shared<ReflectiveMaterial>();
  m_glass = std::make_shared<TransparentMaterial>();
  m_blue = std::make_shared<PhongMaterial>(std::make_shared<ConstantColorTexture>(Colord(0, 0, 1)));

  setAmbient(Colord(0.4, 0.4, 0.4));

  auto sphere1 = std::make_shared<Sphere>(Vector3d(0, 1, 0), 1);
  auto sphere2 = std::make_shared<Sphere>(Vector3d(0, -0.3, 0), 1);
  auto u = std::make_shared<Union>();
  u->add(sphere1);
  u->add(sphere2);

  m_glass->setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(0.1, 0.1, 0.1)));
  m_glass->setRefractionIndex(1.52);

  u->setMaterial(m_glass);

  auto sphere3 = std::make_shared<Sphere>(Vector3d(2.5, 1, 0), 1);
  m_red->setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0)));
  m_red->setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(m_red);

  auto plane = std::make_shared<Plane>(Vector3d(0, -1, 0), 2);
  plane->setMaterial(m_blue);

  add(u);
  add(sphere3);
  add(plane);

  auto light1 = std::make_shared<PointLight>(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<StackedSpheresScene>("Stacked Spheres");
