#include "Raytracer.h"
#include "Scene.h"
#include "Sphere.h"
#include "Box.h"
#include "Plane.h"
#include "Light.h"
#include "Difference.h"
#include "Intersection.h"
#include "Union.h"
#include "QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

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
  d->material().setDiffuseColor(Colord(0.1, 0.1, 0.1));
  d->material().setSpecularColor(Colord(0.1, 0.1, 0.1));
  d->material().setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  d->material().setRefractionIndex(1.52);
  
  scene->add(d);

  Sphere* sphere = new Sphere(Vector3d(0, -1, 3), 1);
  sphere2 = new Sphere(Vector3d(0, -1, 3), 0.95);
  box = new Box(Vector3d(0, -2, 3), Vector3d(1, 0.5, 1));
  d = new Difference();
  d->add(sphere);
  d->add(sphere2);
  d->add(box);
  scene->add(d);
  d->material().setDiffuseColor(Colord(0.1, 0.1, 0.1));
  d->material().setSpecularColor(Colord(0.1, 0.1, 0.1));
  d->material().setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  d->material().setRefractionIndex(1.52);
  
  Sphere* sphere6 = new Sphere(Vector3d(2.5, 1, 0), 1);
  sphere6->material().setDiffuseColor(Colord(1, 0, 0));
  sphere6->material().setSpecularColor(Colord(0.2, 0.2, 0.2));
  scene->add(sphere6);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->material().setDiffuseColor(Colord(0, 0, 1));
  scene->add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}
