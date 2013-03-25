#include "SceneFactory.h"

#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/Light.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/MatteMaterial.h"

class FloatingSpheresScene : public Scene {
public:
  FloatingSpheresScene();

private:
  MatteMaterial m_red, m_green, m_blue;
};

FloatingSpheresScene::FloatingSpheresScene()
  : Scene(),
    m_red(Colord(1, 0, 0)),
    m_green(Colord(0, 1, 0)),
    m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.1, 0.1, 0.1));
  
  Sphere* sphere = new Sphere(Vector3d(0, 0, 0), 1);
  sphere->setMaterial(&m_red);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 1);
  plane->setMaterial(&m_blue);
  
  Sphere* sphere2 = new Sphere(Vector3d(-2, -2, 2), 1);
  sphere2->setMaterial(&m_green);
  
  Sphere* sphere3 = new Sphere(Vector3d(2, -2, 2), 1);
  sphere3->setMaterial(&m_blue);
  
  add(sphere);
  add(sphere2);
  add(sphere3);
  add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  Light* light2 = new Light(Vector3d( 3, -3, -1), Colord(0.4, 0.4, 0.4));
  Light* light3 = new Light(Vector3d(-3, -3,  1), Colord(0.4, 0.4, 0.4));
  Light* light4 = new Light(Vector3d( 3, -3,  1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
  addLight(light2);
  addLight(light3);
  addLight(light4);
}

static bool dummy = SceneFactory::self().registerClass<FloatingSpheresScene>("Floating Spheres");
