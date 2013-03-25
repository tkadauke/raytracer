#include "SceneFactory.h"

#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/Light.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/materials/ReflectiveMaterial.h"

class MetalSpheresScene : public Scene {
public:
  MetalSpheresScene();

private:
  ReflectiveMaterial m_red, m_green, m_blue;
  MatteMaterial m_background;
};

MetalSpheresScene::MetalSpheresScene()
  : Scene(),
    m_background(Colord(0, 0, 1))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  Sphere* sphere = new Sphere(Vector3d(0, 1, 0), 1);
  m_red.setDiffuseColor(Colord(1, 0, 0));
  m_red.setHighlightColor(Colord(0.2, 0.2, 0.2));
  m_red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere->setMaterial(&m_red);
  
  Sphere* sphere2 = new Sphere(Vector3d(-2.5, 1, 0), 1);
  m_green.setDiffuseColor(Colord(0, 1, 0));
  m_green.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere2->setMaterial(&m_green);
  
  Sphere* sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  m_blue.setDiffuseColor(Colord(0, 0, 1));
  m_blue.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&m_blue);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_background);
  
  add(sphere);
  add(sphere2);
  add(sphere3);
  add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<MetalSpheresScene>("Metal Spheres");
