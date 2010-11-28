#include "Raytracer.h"
#include "Scene.h"
#include "Sphere.h"
#include "Box.h"
#include "Plane.h"
#include "Light.h"
#include "Difference.h"
#include "Intersection.h"
#include "Union.h"
#include "Instance.h"
#include "QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

  Sphere* sphere = new Sphere(Vector3d(0, -1, 0), 1);
  Sphere* sphere2 = new Sphere(Vector3d(0, -1, 0), 0.95);
  Box* box = new Box(Vector3d(0, -2, 0), Vector3d(1, 0.5, 1));
  Composite* d = new Difference();
  d->add(sphere);
  d->add(sphere2);
  d->add(box);
  
  Instance* instance = new Instance(d);
  instance->setMatrix(Matrix4d::translate(Vector3d(1.5, 0, 0)));
  instance->material().setDiffuseColor(Colord(0.1, 0.1, 0.1));
  instance->material().setSpecularColor(Colord(0.1, 0.1, 0.1));
  instance->material().setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  instance->material().setRefractionIndex(1.52);
  
  scene->add(instance);

  instance = new Instance(d);
  instance->setMatrix(Matrix4d::translate(Vector3d(-1.5, 0, 0)));
  instance->material().setDiffuseColor(Colord(0.1, 0.1, 0.1));
  instance->material().setSpecularColor(Colord(0.1, 0.1, 0.1));
  instance->material().setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  instance->material().setRefractionIndex(1.52);
  scene->add(instance);
  
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
