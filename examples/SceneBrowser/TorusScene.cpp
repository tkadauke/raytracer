#include "SceneFactory.h"

#include "surfaces/Plane.h"
#include "surfaces/Torus.h"
#include "Light.h"
#include "materials/Material.h"
#include "materials/MatteMaterial.h"
#include "materials/TransparentMaterial.h"

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
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  m_glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  m_glass.setAbsorbanceColor(Colord(0.4, 0.2, 0.2));
  m_glass.setRefractionIndex(1.52);
  
  Torus* torus = new Torus(2, 1);
  torus->setMaterial(&m_glass);
  add(torus);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_blue);
  
  add(plane);
  
  Light* light1 = new Light(Vector3d(-18, -18, -6), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<TorusScene>("Torus");
