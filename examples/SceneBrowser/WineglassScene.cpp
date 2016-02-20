#include "SceneFactory.h"

#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Box.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/Light.h"
#include "raytracer/primitives/Difference.h"
#include "raytracer/primitives/Intersection.h"
#include "raytracer/primitives/Union.h"
#include "raytracer/primitives/Instance.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"

using namespace raytracer;

class WineglassScene : public Scene {
public:
  WineglassScene();

private:
  TransparentMaterial m_glass;
  PhongMaterial m_blue;
};

WineglassScene::WineglassScene()
  : Scene(),
    m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  auto sphere = new Sphere(Vector3d(0, -1, 0), 1);
  auto sphere2 = new Sphere(Vector3d(0, -1, 0), 0.95);
  auto box = new Box(Vector3d(0, -2, 0), Vector3d(1, 0.5, 1));
  auto d = new Difference();
  d->add(sphere);
  d->add(sphere2);
  d->add(box);
  
  m_glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  m_glass.setRefractionIndex(1.52);
  
  auto instance = new Instance(d);
  instance->setMatrix(Matrix4d::translate(Vector3d(1.5, 0, 0)));
  instance->setMaterial(&m_glass);
  
  add(instance);

  instance = new Instance(d);
  instance->setMatrix(Matrix4d::translate(Vector3d(-1.5, 0, 0)));
  instance->setMaterial(&m_glass);
  add(instance);
  
  auto plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_blue);
  add(plane);
  
  auto light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<WineglassScene>("Wine glasses");
