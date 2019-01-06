#include "SceneFactory.h"

#include "raytracer/primitives/Box.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/lights/PointLight.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

using namespace raytracer;

class GlassBoxesScene : public Scene {
public:
  GlassBoxesScene();

private:
  std::shared_ptr<TransparentMaterial> m_glass;
  std::shared_ptr<ReflectiveMaterial> m_blue;
};

GlassBoxesScene::GlassBoxesScene()
  : Scene()
{
  m_glass = std::make_shared<TransparentMaterial>();
  m_blue = std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord(0, 0, 1)));
  setAmbient(Colord(0.1, 0.1, 0.1));

  m_glass->setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(0.1, 0.1, 0.1)));
  m_glass->setRefractionIndex(1.52);

  auto box = std::make_shared<Box>(Vector3d(0, 0, 0), Vector3d(0.1, 1, 1));
  box->setMaterial(m_glass);
  add(box);

  box = std::make_shared<Box>(Vector3d(0.5, 0, 0), Vector3d(0.1, 1, 1));
  box->setMaterial(m_glass);
  add(box);

  box = std::make_shared<Box>(Vector3d(1, 0, 0), Vector3d(0.1, 1, 1));
  box->setMaterial(m_glass);
  add(box);

  m_blue->setSpecularColor(Colord(1, 1, 1));
  auto plane = std::make_shared<Plane>(Vector3d(0, -1, 0), 1);
  plane->setMaterial(m_blue);

  add(plane);

  auto light1 = std::make_shared<PointLight>(Vector3d(-3, -3, -3), Colord(0.4, 0.4, 0.4));
  auto light2 = std::make_shared<PointLight>(Vector3d( 3, -3, -3), Colord(0.4, 0.4, 0.4));
  auto light3 = std::make_shared<PointLight>(Vector3d(-3, -3,  3), Colord(0.4, 0.4, 0.4));
  auto light4 = std::make_shared<PointLight>(Vector3d( 3, -3,  3), Colord(0.4, 0.4, 0.4));
  addLight(light1);
  addLight(light2);
  addLight(light3);
  addLight(light4);
}

static bool dummy = SceneFactory::self().registerClass<GlassBoxesScene>("Glass Boxes");
