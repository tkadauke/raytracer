#include "SceneFactory.h"

#include "surfaces/Box.h"
#include "surfaces/Plane.h"
#include "Light.h"
#include "materials/Material.h"
#include "materials/ReflectiveMaterial.h"
#include "materials/TransparentMaterial.h"

class GlassBoxesScene : public Scene {
public:
  GlassBoxesScene();

private:
  TransparentMaterial m_glass;
  ReflectiveMaterial m_blue;
};

GlassBoxesScene::GlassBoxesScene()
  : Scene(), m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.1, 0.1, 0.1));
  
  m_glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  m_glass.setAbsorbanceColor(Colord(0.4, 0.2, 0.2));
  m_glass.setRefractionIndex(1.52);

  Box* box = new Box(Vector3d(0, 0, 0), Vector3d(0.1, 1, 1));
  box->setMaterial(&m_glass);
  add(box);

  box = new Box(Vector3d(0.5, 0, 0), Vector3d(0.1, 1, 1));
  box->setMaterial(&m_glass);
  add(box);
  
  box = new Box(Vector3d(1, 0, 0), Vector3d(0.1, 1, 1));
  box->setMaterial(&m_glass);
  add(box);
  
  m_blue.setSpecularColor(Colord(1, 1, 1));
  Plane* plane = new Plane(Vector3d(0, -1, 0), 1);
  plane->setMaterial(&m_blue);
  
  add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -3), Colord(0.4, 0.4, 0.4));
  Light* light2 = new Light(Vector3d( 3, -3, -3), Colord(0.4, 0.4, 0.4));
  Light* light3 = new Light(Vector3d(-3, -3,  3), Colord(0.4, 0.4, 0.4));
  Light* light4 = new Light(Vector3d( 3, -3,  3), Colord(0.4, 0.4, 0.4));
  addLight(light1);
  addLight(light2);
  addLight(light3);
  addLight(light4);
}

static bool dummy = SceneFactory::self().registerClass<GlassBoxesScene>("Glass Boxes");
