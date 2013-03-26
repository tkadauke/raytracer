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

using namespace raytracer;

class LensScene : public Scene {
public:
  LensScene();

private:
  ReflectiveMaterial m_red;
  TransparentMaterial m_glass;
  PhongMaterial m_blue;
};

LensScene::LensScene()
  : Scene(),
    m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  Sphere* sphere1 = new Sphere(Vector3d(0, 1, 0), 1);
  Sphere* sphere2 = new Sphere(Vector3d(1, 1, 0), 1);
  Composite* i = new Intersection();
  i->add(sphere1);
  i->add(sphere2);
  
  m_glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  m_glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  m_glass.setRefractionIndex(1.52);
  
  i->setMaterial(&m_glass);
  
  Sphere* sphere3 = new Sphere(Vector3d(3, 1, 0), 1);
  m_red.setDiffuseColor(Colord(1, 0, 0));
  m_red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&m_red);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_blue);
  
  add(i);
  add(sphere3);
  add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<LensScene>("Lens");
