#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "surfaces/Sphere.h"
#include "surfaces/Plane.h"
#include "Light.h"
#include "materials/Material.h"
#include "materials/MatteMaterial.h"
#include "materials/ReflectiveMaterial.h"
#include "widgets/QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

  Sphere* sphere = new Sphere(Vector3d(0, 1, 0), 1);
  ReflectiveMaterial m1;
  m1.setDiffuseColor(Colord(1, 0, 0));
  m1.setHighlightColor(Colord(0.2, 0.2, 0.2));
  m1.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere->setMaterial(&m1);
  
  Sphere* sphere2 = new Sphere(Vector3d(-2.5, 1, 0), 1);
  ReflectiveMaterial m2;
  m2.setDiffuseColor(Colord(0, 1, 0));
  m2.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere2->setMaterial(&m2);
  
  Sphere* sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  ReflectiveMaterial m3;
  m3.setDiffuseColor(Colord(0, 0, 1));
  m3.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&m3);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  MatteMaterial m4(Colord(0, 0, 1));
  plane->setMaterial(&m4);
  
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
