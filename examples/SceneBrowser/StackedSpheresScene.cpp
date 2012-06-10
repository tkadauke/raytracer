#include "SceneFactory.h"

#include "surfaces/Sphere.h"
#include "surfaces/Plane.h"
#include "Light.h"
#include "surfaces/Intersection.h"
#include "surfaces/Union.h"
#include "materials/Material.h"
#include "materials/PhongMaterial.h"
#include "materials/ReflectiveMaterial.h"
#include "materials/TransparentMaterial.h"

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
    m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  Sphere* sphere1 = new Sphere(Vector3d(0, 1, 0), 1);
  Sphere* sphere2 = new Sphere(Vector3d(0, -0.3, 0), 1);
  Union* u = new Union();
  u->add(sphere1);
  u->add(sphere2);
  
  m_glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  m_glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  m_glass.setRefractionIndex(1.52);
  
  u->setMaterial(&m_glass);
  
  Sphere* sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  m_red.setDiffuseColor(Colord(1, 0, 0));
  m_red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&m_red);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_blue);
  
  add(u);
  add(sphere3);
  add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<StackedSpheresScene>("Stacked Spheres");
