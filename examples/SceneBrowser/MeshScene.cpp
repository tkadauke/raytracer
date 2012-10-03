#include "SceneFactory.h"

#include "surfaces/Mesh.h"
#include "surfaces/Sphere.h"
#include "surfaces/FlatMeshTriangle.h"
#include "surfaces/SmoothMeshTriangle.h"
#include "surfaces/Grid.h"
#include "surfaces/Instance.h"
#include "formats/ply/PlyFile.h"
#include "Light.h"
#include "materials/ReflectiveMaterial.h"
#include "materials/MatteMaterial.h"

#include <fstream>

using namespace std;

class MeshScene : public Scene {
public:
  MeshScene();

private:
  MatteMaterial m_red;
  ReflectiveMaterial m_silver;
  Mesh m_mesh;
};

MeshScene::MeshScene()
  : Scene(),
    m_red(Colord(1, 0, 0)),
    m_silver(Colord(0.6, 0.6, 0.6))
{
  setAmbient(Colord(0.1, 0.1, 0.1));
  
  ifstream stream("test/fixtures/shark.ply");
  PlyFile file(stream, m_mesh);
  m_mesh.computeNormals(false);
  
  m_silver.setSpecularColor(Colord(0.5, 0.5, 0.5));
  
  Grid* grid = new Grid;
  m_mesh.addSmoothTrianglesTo(grid, &m_silver);
  grid->setup();
  Instance* instance = new Instance(grid);
  instance->setMatrix(Matrix4d::translate(Vector3d(0, 0, 0)) * Matrix3d::scale(0.07));
  instance->setMaterial(&m_silver);
  add(instance);
  
  Sphere* sphere = new Sphere(Vector3d(0, 0, 200), 100);
  sphere->setMaterial(&m_red);
  add(sphere);
  
  Light* light1 = new Light(Vector3d(-100, -100, -100), Colord(0.6, 0.6, 0.6));
  Light* light2 = new Light(Vector3d(100, -100, 100), Colord(0.6, 0.6, 0.6));
  addLight(light1);
  addLight(light2);
}

static bool dummy = SceneFactory::self().registerClass<MeshScene>("Mesh (Shark)");
