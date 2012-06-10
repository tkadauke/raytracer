#include "SceneFactory.h"

#include "surfaces/Disk.h"
#include "surfaces/Plane.h"
#include "Light.h"
#include "materials/Material.h"
#include "materials/MatteMaterial.h"

class DiskScene : public Scene {
public:
  DiskScene();

private:
  MatteMaterial m_red, m_green, m_blue;
};

DiskScene::DiskScene()
  : Scene(),
    m_red(Colord(1, 0, 0)),
    m_green(Colord(0, 1, 0)),
    m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  Disk* disk1 = new Disk(Vector3d(0, 1, 0), Vector3d(0, 0, -1), 1);
  disk1->setMaterial(&m_red);
  add(disk1);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_blue);
  
  add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<DiskScene>("Disk");
