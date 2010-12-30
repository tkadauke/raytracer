#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "surfaces/Mesh.h"
#include "surfaces/Sphere.h"
#include "surfaces/FlatMeshTriangle.h"
#include "surfaces/SmoothMeshTriangle.h"
#include "formats/ply/PlyFile.h"
#include "Light.h"
#include "materials/TransparentMaterial.h"
#include "materials/PhongMaterial.h"
#include "widgets/QtDisplay.h"

#include <QApplication>
#include <fstream>

using namespace std;

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.1, 0.1, 0.1));

  ifstream stream("test/fixtures/shark.ply");
  Mesh mesh;
  PlyFile file(stream, mesh);
  mesh.computeNormals();
  
  PhongMaterial red(Colord(1, 0, 0));
  TransparentMaterial glass;
  glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  glass.setRefractionIndex(1.52);
  
  mesh.addSmoothTrianglesTo(scene, &glass);
  
  Sphere* sphere = new Sphere(Vector3d(0, 0, 200), 100);
  sphere->setMaterial(&red);
  scene->add(sphere);
  
  Light* light1 = new Light(Vector3d(-100, -100, -100), Colord(0.6, 0.6, 0.6));
  Light* light2 = new Light(Vector3d(100, -100, 100), Colord(0.6, 0.6, 0.6));
  scene->addLight(light1);
  scene->addLight(light2);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->setDistance(100);
  display->show();
  
  return app.exec();
}
