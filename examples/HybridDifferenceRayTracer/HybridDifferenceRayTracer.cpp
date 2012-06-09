#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "surfaces/Grid.h"
#include "surfaces/Sphere.h"
#include "surfaces/Box.h"
#include "surfaces/Plane.h"
#include "Light.h"
#include "surfaces/Difference.h"
#include "surfaces/Intersection.h"
#include "surfaces/Union.h"
#include "materials/Material.h"
#include "materials/PhongMaterial.h"
#include "materials/ReflectiveMaterial.h"
#include "materials/TransparentMaterial.h"
#include "widgets/QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));
  Grid* grid = new Grid;

  Box* box = new Box(Vector3d(0, 1, 0), Vector3d(1, 1, 1));
  Sphere* sphere2 = new Sphere(Vector3d(1.8, 1, 0), 1);
  Sphere* sphere3 = new Sphere(Vector3d(-1.8, 1, 0), 1);
  Sphere* sphere4 = new Sphere(Vector3d(0, 1, 1.8), 1);
  Sphere* sphere5 = new Sphere(Vector3d(0, 1, -1.8), 1);
  Composite* d = new Difference();
  d->add(box);
  d->add(sphere2);
  d->add(sphere3);
  d->add(sphere4);
  d->add(sphere5);
  
  TransparentMaterial glass;
  glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  glass.setRefractionIndex(1.52);
  
  d->setMaterial(&glass);
  
  grid->add(d);

  Sphere* sphere = new Sphere(Vector3d(0, -1, 3), 1);
  sphere2 = new Sphere(Vector3d(0, -1, 3), 0.95);
  box = new Box(Vector3d(0, -2, 3), Vector3d(1, 0.5, 1));
  d = new Difference();
  d->add(sphere);
  d->add(sphere2);
  d->add(box);
  grid->add(d);
  
  d->setMaterial(&glass);
  
  Sphere* sphere6 = new Sphere(Vector3d(2.5, 1, 0), 1);
  ReflectiveMaterial red;
  red.setDiffuseColor(Colord(1, 0, 0));
  red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere6->setMaterial(&red);
  grid->add(sphere6);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  PhongMaterial blue(Colord(0, 0, 1));
  plane->setMaterial(&blue);
  scene->add(plane);
  grid->setup();
  scene->add(grid);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}
