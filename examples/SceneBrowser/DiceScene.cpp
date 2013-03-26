#include "SceneFactory.h"

#include "raytracer/primitives/Scene.h"
#include "raytracer/primitives/Grid.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Box.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/Light.h"
#include "raytracer/primitives/Difference.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"

using namespace raytracer;

class DiceScene : public Scene {
public:
  DiceScene();

private:
  ReflectiveMaterial m_red;
  TransparentMaterial m_glass;
  PhongMaterial m_blue;
};

DiceScene::DiceScene()
  : Scene(),
    m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  Grid* grid = new Grid;
  
  Box* box = new Box(Vector3d(0, 1, 0), Vector3d(1, 1, 1));
  Sphere* sphere2 = new Sphere(Vector3d(1.8, 1, 0), 1);
  Sphere* sphere3 = new Sphere(Vector3d(-1.8, 1, 0), 1);
  Sphere* sphere4 = new Sphere(Vector3d(0, 1, 1.8), 1);
  Sphere* sphere5 = new Sphere(Vector3d(0, 1, -1.8), 1);
  Composite* d = new Difference();
  d->add(box);
  d->add(sphere2);
  d->add(sphere3);
  d->add(sphere4);
  d->add(sphere5);
  
  m_glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  m_glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  m_glass.setRefractionIndex(1.52);
  
  d->setMaterial(&m_glass);
  
  grid->add(d);
  
  Sphere* sphere6 = new Sphere(Vector3d(2.5, 1, 0), 1);
  m_red.setDiffuseColor(Colord(1, 0, 0));
  m_red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere6->setMaterial(&m_red);
  grid->add(sphere6);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_blue);
  add(plane);
  grid->setup();
  add(grid);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<DiceScene>("Dice");
