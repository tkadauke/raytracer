#include "SceneFactory.h"

#include "core/geometry/Mesh.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/SmoothMeshTriangle.h"
#include "raytracer/primitives/Grid.h"
#include "raytracer/primitives/Instance.h"
#include "core/formats/ply/PlyFile.h"
#include "raytracer/lights/PointLight.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

#include <fstream>

using namespace std;
using namespace raytracer;

class MeshScene : public Scene {
public:
  MeshScene();

private:
  std::shared_ptr<MatteMaterial> m_red;
  std::shared_ptr<ReflectiveMaterial> m_silver;
  Mesh m_mesh;
};

MeshScene::MeshScene()
  : Scene()
{
  m_red = std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0)));
  m_silver = std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord(0.6, 0.6, 0.6)));

  setAmbient(Colord(0.1, 0.1, 0.1));

  ifstream stream("test/fixtures/shark.ply");
  PlyFile file(stream, m_mesh);
  m_mesh.computeNormals(false);

  m_silver->setSpecularColor(Colord(0.5, 0.5, 0.5));

  auto grid = std::make_shared<Grid>();
  SmoothMeshTriangle::build(&m_mesh, grid.get(), m_silver);
  grid->setup();
  auto instance = std::make_shared<Instance>(grid);
  instance->setMatrix(Matrix4d::translate(Vector3d(0, 0, 0)) * Matrix3d::scale(0.07));
  instance->setMaterial(m_silver);
  add(instance);

  auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 200), 100);
  sphere->setMaterial(m_red);
  add(sphere);

  auto light1 = std::make_shared<PointLight>(Vector3d(-100, -100, -100), Colord(0.6, 0.6, 0.6));
  auto light2 = std::make_shared<PointLight>(Vector3d(100, -100, 100), Colord(0.6, 0.6, 0.6));
  addLight(light1);
  addLight(light2);
}

static bool dummy = SceneFactory::self().registerClass<MeshScene>("Mesh (Shark)");
