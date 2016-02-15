#include "SceneFactory.h"

#include "raytracer/primitives/Plane.h"
#include "raytracer/primitives/Torus.h"
#include "raytracer/primitives/Instance.h"
#include "raytracer/Light.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"

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
    m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.8, 0.8, 0.8));
  
  m_glass.setHighlightColor(Colord(0.5, 0.5, 0.5));
  m_glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  m_glass.setAbsorbanceColor(Colord(0.4, 0.2, 0.2));
  m_glass.setRefractionIndex(1.52);
  
  auto torus = new Torus(2, 1);
  auto instance = new Instance(torus);
  instance->setMatrix(Matrix3d::rotateX(Angled::fromDegrees(90)));
  instance->setMaterial(&m_glass);
  add(instance);
  
  auto plane = new Plane(Vector3d(0, -1, 0), 4);
  plane->setMaterial(&m_blue);
  
  add(plane);
  
  auto light1 = new Light(Vector3d(-18, -18, -6), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<TorusScene>("Torus");
