#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Scene.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "raytracer/Light.h"
#include "widgets/QtDisplay.h"

#include <QApplication>

using namespace raytracer;

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  ::Scene* scene = new ::Scene;
  Sphere* sphere = new Sphere;
  sphere->setRadius(1);
  scene->add(sphere);
  
  raytracer::Scene* raytracerScene = scene->toRaytracerScene();
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  raytracerScene->addLight(light1);
  
  Raytracer* raytracer = new Raytracer(raytracerScene);
  QtDisplay* display = new QtDisplay(raytracer);
  
  display->show();
  
  return app.exec();
}
