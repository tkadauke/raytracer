#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "surfaces/Sphere.h"
#include "surfaces/Plane.h"
#include "Light.h"
#include "surfaces/Union.h"
#include "materials/Material.h"
#include "materials/PhongMaterial.h"
#include "materials/ReflectiveMaterial.h"
#include "materials/TransparentMaterial.h"

#include "widgets/FishEyeCameraParameterWidget.h"
#include "examples/FishEyeRayTracer/Display.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

  Sphere* sphere1 = new Sphere(Vector3d(0, 1, 0), 1);
  Sphere* sphere2 = new Sphere(Vector3d(0, -0.3, 0), 1);
  Union* u = new Union();
  u->add(sphere1);
  u->add(sphere2);
  
  TransparentMaterial glass;
  glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  glass.setRefractionIndex(1.52);
  
  u->setMaterial(&glass);
  
  Sphere* sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  ReflectiveMaterial red;
  red.setDiffuseColor(Colord(1, 0, 0));
  red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&red);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  PhongMaterial blue(Colord(0, 0, 1));
  plane->setMaterial(&blue);
  
  scene->add(u);
  scene->add(sphere3);
  scene->add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  
  Display* display = new Display(scene);
  display->show();
  
  return app.exec();
}
