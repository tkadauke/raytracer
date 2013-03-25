#include "Raytracer.h"
#include "primitives/Scene.h"
#include "primitives/Sphere.h"
#include "primitives/Plane.h"
#include "Light.h"
#include "materials/Material.h"
#include "materials/PhongMaterial.h"
#include "materials/ReflectiveMaterial.h"
#include "materials/TransparentMaterial.h"
#include "widgets/QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

  Sphere* sphere = new Sphere(Vector3d(0, 1, 0), 1);
  TransparentMaterial glass;
  glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  glass.setRefractionIndex(1.52);
  sphere->setMaterial(&glass);
  
  Sphere* sphere2 = new Sphere(Vector3d(-2.5, 1, 0), 1);
  ReflectiveMaterial green;
  green.setDiffuseColor(Colord(0, 1, 0));
  green.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere2->setMaterial(&green);
  
  Sphere* sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  ReflectiveMaterial red;
  red.setDiffuseColor(Colord(1, 0, 0));
  red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&red);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  PhongMaterial blue(Colord(0, 0, 1));
  plane->setMaterial(&blue);
  
  scene->add(sphere);
  scene->add(sphere2);
  scene->add(sphere3);
  scene->add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}
