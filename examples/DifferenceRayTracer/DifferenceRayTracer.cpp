#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/Light.h"
#include "raytracer/primitives/Difference.h"
#include "raytracer/primitives/Intersection.h"
#include "raytracer/primitives/Union.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"
#include "widgets/QtDisplay.h"

#include <QApplication>

using namespace raytracer;

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

  Sphere* sphere1 = new Sphere(Vector3d(0, 1, 0), 1);
  Sphere* sphere2 = new Sphere(Vector3d(1.8, 1, 0), 1);
  Sphere* sphere3 = new Sphere(Vector3d(-1.8, 1, 0), 1);
  Sphere* sphere4 = new Sphere(Vector3d(0, 1, 1.8), 1);
  Sphere* sphere5 = new Sphere(Vector3d(0, 1, -1.8), 1);
  Composite* d = new Difference();
  d->add(sphere1);
  d->add(sphere2);
  d->add(sphere3);
  d->add(sphere4);
  d->add(sphere5);
  
  TransparentMaterial glass;
  glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  glass.setRefractionIndex(1.52);
  
  d->setMaterial(&glass);
  
  Sphere* sphere6 = new Sphere(Vector3d(2.5, 1, 0), 1);
  
  ReflectiveMaterial red;
  red.setDiffuseColor(Colord(1, 0, 0));
  red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere6->setMaterial(&red);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  
  PhongMaterial blue(Colord(0, 0, 1));
  plane->setMaterial(&blue);
  
  scene->add(d);
  scene->add(sphere6);
  scene->add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}
