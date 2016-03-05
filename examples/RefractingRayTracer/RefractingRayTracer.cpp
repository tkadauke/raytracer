#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Plane.h"
#include "raytracer/lights/Light.h"
#include "raytracer/materials/Material.h"
#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"
#include "widgets/QtDisplay.h"

#include <QApplication>

using namespace raytracer;

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  auto scene = new Scene(Colord(0.4, 0.4, 0.4));

  auto sphere = std::make_shared<Sphere>(Vector3d(0, 1, 0), 1);
  TransparentMaterial glass;
  glass.setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(0.1, 0.1, 0.1)));
  glass.setRefractionIndex(1.52);
  sphere->setMaterial(&glass);
  
  auto sphere2 = std::make_shared<Sphere>(Vector3d(-2.5, 1, 0), 1);
  ReflectiveMaterial green;
  green.setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(0, 1, 0)));
  green.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere2->setMaterial(&green);
  
  auto sphere3 = std::make_shared<Sphere>(Vector3d(2.5, 1, 0), 1);
  ReflectiveMaterial red;
  red.setDiffuseTexture(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0)));
  red.setSpecularColor(Colord(0.2, 0.2, 0.2));
  sphere3->setMaterial(&red);
  
  auto plane = std::make_shared<Plane>(Vector3d(0, -1, 0), 2);
  PhongMaterial blue(std::make_shared<ConstantColorTexture>(Colord(0, 0, 1)));
  plane->setMaterial(&blue);
  
  scene->add(sphere);
  scene->add(sphere2);
  scene->add(sphere3);
  scene->add(plane);
  
  auto light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  
  auto raytracer = std::make_shared<Raytracer>(scene);
  auto display = new QtDisplay(0, raytracer);
  
  display->show();
  
  return app.exec();
}
