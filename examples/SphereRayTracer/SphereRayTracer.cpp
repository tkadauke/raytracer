#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "surfaces/Sphere.h"
#include "surfaces/Plane.h"
#include "Light.h"
#include "materials/Material.h"
#include "materials/MatteMaterial.h"
#include "widgets/QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.1, 0.1, 0.1));

  Sphere* sphere = new Sphere(Vector3d(0, 0, 0), 1);
  MatteMaterial m1(Colord(1, 0, 0));
  sphere->setMaterial(&m1);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 1);
  MatteMaterial m2(Colord(0, 0, 1));
  plane->setMaterial(&m2);
  
  Sphere* sphere2 = new Sphere(Vector3d(-2, -2, 2), 1);
  MatteMaterial m3(Colord(0, 1, 0));
  sphere2->setMaterial(&m3);
  
  Sphere* sphere3 = new Sphere(Vector3d(2, -2, 2), 1);
  sphere3->setMaterial(&m2);
  
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
