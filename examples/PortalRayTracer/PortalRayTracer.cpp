#include "Raytracer.h"
#include "surfaces/Scene.h"
#include "surfaces/Box.h"
#include "surfaces/Sphere.h"
#include "surfaces/Plane.h"
#include "Light.h"
#include "materials/Material.h"
#include "materials/ReflectiveMaterial.h"
#include "materials/TransparentMaterial.h"
#include "materials/PortalMaterial.h"
#include "widgets/QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.1, 0.1, 0.1));
  
  PortalMaterial portal(Matrix4d::translate(Vector3d(0, 0, 3)) * Matrix3d::rotateZ(0.1), Colord(0.8, 0.8, 0.8));

  TransparentMaterial glass;
  glass.setDiffuseColor(Colord(0.1, 0.1, 0.1));
  glass.setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  glass.setRefractionIndex(1.52);

  Box* box = new Box(Vector3d(0, 0, 0), Vector3d(1, 1, 0.1));
  box->setMaterial(&portal);
  scene->add(box);

  box = new Box(Vector3d(0, 0, -1), Vector3d(0.5, 0.5, 0.1));
  box->setMaterial(&glass);
  scene->add(box);

  Sphere* sphere = new Sphere(Vector3d(0, 0, 0), 1);
  sphere->setMaterial(&portal);
  // scene->add(sphere);
  
  ReflectiveMaterial blue(Colord(0, 0, 1));
  blue.setSpecularColor(Colord(1, 1, 1));
  Plane* plane = new Plane(Vector3d(0, -1, 0), 1);
  plane->setMaterial(&blue);
  
  scene->add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -3), Colord(0.4, 0.4, 0.4));
  Light* light2 = new Light(Vector3d( 3, -3, -3), Colord(0.4, 0.4, 0.4));
  Light* light3 = new Light(Vector3d(-3, -3,  3), Colord(0.4, 0.4, 0.4));
  Light* light4 = new Light(Vector3d( 3, -3,  3), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  scene->addLight(light2);
  scene->addLight(light3);
  scene->addLight(light4);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}