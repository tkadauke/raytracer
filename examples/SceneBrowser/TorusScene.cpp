#include "SceneFactory.h"

#include "raytracer/primitives/Plane.h"
#include "raytracer/primitives/Torus.h"
#include "raytracer/primitives/Instance.h"
#include "raytracer/lights/Light.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

using namespace raytracer;

class TorusScene : public Scene {
public:
  TorusScene();

private:
  TransparentMaterial m_glass;
  MatteMaterial m_blue;
};

TorusScene::TorusScene()
  : Scene(),
    m_blue(std::make_shared<ConstantColorTexture>(Colord(0, 0, 1)))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  m_glass.setSpecularColor(Colord(0.5, 0.5, 0.5));
  m_glass.setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(0.0, 0.1, 0.1)));
  m_glass.setAmbientCoefficient(0.2);
  m_glass.setDiffuseCoefficient(0);
  m_glass.setRefractionIndex(1.52);
  
  auto torus = std::make_shared<Torus>(2, 1);
  auto instance = std::make_shared<Instance>(torus);
  instance->setMatrix(Matrix3d::rotateX(Angled::fromDegrees(90)));
  instance->setMaterial(&m_glass);
  add(instance);
  
  auto plane = std::make_shared<Plane>(Vector3d(0, -1, 0), 4);
  plane->setMaterial(&m_blue);
  
  add(plane);
  
  auto light1 = new Light(Vector3d(-18, -18, -6), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<TorusScene>("Torus");
