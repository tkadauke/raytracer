#include "Raytracer.h"
#include "Scene.h"
#include "Sphere.h"
#include "Plane.h"
#include "Light.h"
#include "QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.1, 0.1, 0.1));

  Sphere* sphere = new Sphere(Vector3d(0, 0, 0), 1);
  sphere->material().setDiffuseColor(Colord(1, 0, 0));
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 1);
  plane->material().setDiffuseColor(Colord(0, 0, 1));
  
  Sphere* sphere2 = new Sphere(Vector3d(-2, -2, 2), 1);
  sphere2->material().setDiffuseColor(Colord(0, 1, 0));
  
  Sphere* sphere3 = new Sphere(Vector3d(2, -2, 2), 1);
  sphere3->material().setDiffuseColor(Colord(0, 0, 1));
  
  scene->add(sphere);
  scene->add(sphere2);
  scene->add(sphere3);
  scene->add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  Light* light2 = new Light(Vector3d( 3, -3, -1), Colord(0.4, 0.4, 0.4));
  Light* light3 = new Light(Vector3d(-3, -3,  1), Colord(0.4, 0.4, 0.4));
  Light* light4 = new Light(Vector3d( 3, -3,  1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  scene->addLight(light2);
  scene->addLight(light3);
  scene->addLight(light4);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}
