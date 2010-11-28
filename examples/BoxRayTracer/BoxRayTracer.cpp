#include "Raytracer.h"
#include "Scene.h"
#include "Box.h"
#include "Plane.h"
#include "Light.h"
#include "QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.1, 0.1, 0.1));

  Box* box = new Box(Vector3d(0, 0, 0), Vector3d(0.1, 1, 1));
  box->material().setDiffuseColor(Colord(0.1, 0.1, 0.1));
  box->material().setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  box->material().setRefractionIndex(1.52);
  scene->add(box);

  box = new Box(Vector3d(0.5, 0, 0), Vector3d(0.1, 1, 1));
  box->material().setDiffuseColor(Colord(0.1, 0.1, 0.1));
  box->material().setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  box->material().setRefractionIndex(1.52);
  scene->add(box);
  
  box = new Box(Vector3d(1, 0, 0), Vector3d(0.1, 1, 1));
  box->material().setDiffuseColor(Colord(0.1, 0.1, 0.1));
  box->material().setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  box->material().setRefractionIndex(1.52);
  scene->add(box);
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 1);
  plane->material().setDiffuseColor(Colord(0, 0, 1));
  
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
