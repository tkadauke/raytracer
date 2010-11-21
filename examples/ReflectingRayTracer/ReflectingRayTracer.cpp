#include "Raytracer.h"
#include "Scene.h"
#include "Sphere.h"
#include "Plane.h"
#include "Light.h"
#include "QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

  Sphere* sphere = new Sphere(Vector3d(0, 1, 0), 1);
  sphere->material().setDiffuseColor(Colord(1, 0, 0));
  sphere->material().setHighlightColor(Colord(0.2, 0.2, 0.2));
  sphere->material().setSpecularColor(Colord(0.2, 0.2, 0.2));
  
  Sphere* sphere2 = new Sphere(Vector3d(-2.5, 1, 0), 1);
  sphere2->material().setDiffuseColor(Colord(0, 1, 0));
  sphere2->material().setSpecularColor(Colord(0.2, 0.2, 0.2));
  
  Sphere* sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  sphere3->material().setDiffuseColor(Colord(0, 0, 1));
  sphere3->material().setSpecularColor(Colord(0.2, 0.2, 0.2));
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->material().setDiffuseColor(Colord(0, 0, 1));
  
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
