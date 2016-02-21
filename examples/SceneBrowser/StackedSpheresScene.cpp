#include "SceneFactory.h"

#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/Light.h"
#include "raytracer/primitives/Intersection.h"
#include "raytracer/primitives/Union.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

using namespace raytracer;

class StackedSpheresScene : public Scene {
public:
  StackedSpheresScene();

private:
  ReflectiveMaterial m_red;
  TransparentMaterial m_glass;
  PhongMaterial m_blue;
};

StackedSpheresScene::StackedSpheresScene()
  : Scene(),
    m_blue(new ConstantColorTexture(Colord(0, 0, 1)))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  auto sphere1 = new Sphere(Vector3d(0, 1, 0), 1);
  auto sphere2 = new Sphere(Vector3d(0, -0.3, 0), 1);
  auto u = new Union();
  u->add(sphere1);
  u->add(sphere2);
  
  m_glass.setDiffuseTexture(new ConstantColorTexture(Colord(0.1, 0.1, 0.1)));
  m_glass.setRefractionIndex(1.52);
  
  u->setMaterial(&m_glass);
  
  auto sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  m_red.setDiffuseTexture(new ConstantColorTexture(Colord(1, 0, 0)));
  m_red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&m_red);
  
  auto plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_blue);
  
  add(u);
  add(sphere3);
  add(plane);
  
  auto light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<StackedSpheresScene>("Stacked Spheres");
