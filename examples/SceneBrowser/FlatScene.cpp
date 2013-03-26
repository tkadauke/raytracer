#include "SceneFactory.h"

#include "raytracer/primitives/Triangle.h"
#include "raytracer/primitives/Disk.h"
#include "raytracer/primitives/Rectangle.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/Light.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/MatteMaterial.h"

using namespace raytracer;

class FlatScene : public Scene {
public:
  FlatScene();

private:
  MatteMaterial m_red, m_green, m_blue;
};

FlatScene::FlatScene()
  : Scene(),
    m_red(Colord(1, 0, 0)),
    m_green(Colord(0, 1, 0)),
    m_blue(Colord(0, 0, 1))
{
  setAmbient(Colord(0.4, 0.4, 0.4));
  
  Triangle* triangle = new Triangle(Vector3d(0, 2, 0), Vector3d(2, 2, 0), Vector3d(0, 0, 0));
  triangle->setMaterial(&m_red);
  add(triangle);
  
  Disk* disk = new Disk(Vector3d(3, 1, 0), Vector3d(0, 0, -1), 1);
  disk->setMaterial(&m_green);
  add(disk);
  
  Rectangle* rectangle = new Rectangle(Vector3d(-3, 2, 0), Vector3d(2, 0, 0), Vector3d(0, -2, 0));
  rectangle->setMaterial(&m_blue);
  add(rectangle);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->setMaterial(&m_blue);
  
  add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  addLight(light1);
}

static bool dummy = SceneFactory::self().registerClass<FlatScene>("Flat");
