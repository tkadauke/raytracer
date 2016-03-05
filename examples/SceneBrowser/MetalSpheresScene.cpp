#include "SceneFactory.h"

#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/lights/PointLight.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

using namespace raytracer;

class MetalSpheresScene : public Scene {
public:
  MetalSpheresScene();

private:
  ReflectiveMaterial m_red, m_green, m_blue;
  MatteMaterial m_background;
};

MetalSpheresScene::MetalSpheresScene()
  : Scene(),
    m_background(std::make_shared<ConstantColorTexture>(Colord(0, 0, 1)))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  auto sphere = std::make_shared<Sphere>(Vector3d(0, 1, 0), 1);
  m_red.setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0)));
  m_red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  m_red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere->setMaterial(&m_red);
  
  auto sphere2 = std::make_shared<Sphere>(Vector3d(-2.5, 1, 0), 1);
  m_green.setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(0, 1, 0)));
  m_green.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere2->setMaterial(&m_green);
  
  auto sphere3 = std::make_shared<Sphere>(Vector3d(2.5, 1, 0), 1);
  m_blue.setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(0, 0, 1)));
  m_blue.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&m_blue);
  
  auto plane = std::make_shared<Plane>(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_background);
  
  add(sphere);
  add(sphere2);
  add(sphere3);
  add(plane);
  
  auto light1 = new PointLight(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<MetalSpheresScene>("Metal Spheres");
