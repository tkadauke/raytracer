#include "Raytracer.h"
#include "Scene.h"
#include "Sphere.h"
#include "Plane.h"
#include "Light.h"
#include "Union.h"
#include "Material.h"
#include "QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

  Sphere* sphere1 = new Sphere(Vector3d(0, 1, 0), 1);
  Sphere* sphere2 = new Sphere(Vector3d(0, -0.3, 0), 1);
  Union* u = new Union();
  u->add(sphere1);
  u->add(sphere2);
  
  Material glass;
  glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  glass.setRefractionIndex(1.52);
  
  u->setMaterial(&glass);
  
  Sphere* sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  Material red;
  red.setDiffuseColor(Colord(1, 0, 0));
  red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&red);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  Material blue;
  blue.setDiffuseColor(Colord(0, 0, 1));
  plane->setMaterial(&blue);
  
  scene->add(u);
  scene->add(sphere3);
  scene->add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}
