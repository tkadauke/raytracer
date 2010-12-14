#include "Raytracer.h"
#include "Scene.h"
#include "Sphere.h"
#include "Light.h"
#include "Material.h"
#include "QtDisplay.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.1, 0.1, 0.1));

  Sphere* sphere = new Sphere(Vector3d(0, 0, 0), 1);
  Material red;
  red.setDiffuseColor(Colord(1, 0, 0));
  red.setHighlightColor(Colord(0, 0, 0));
  sphere->setMaterial(&red);
  
  scene->add(sphere);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.6, 0.6, 0.6));
  scene->addLight(light1);
  
  Raytracer* raytracer = new Raytracer(scene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}
