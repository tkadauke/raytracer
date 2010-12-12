#include "Raytracer.h"
#include "Scene.h"
#include "Sphere.h"
#include "Plane.h"
#include "Light.h"
#include "Union.h"

#include "widgets/SphericalCameraParameterWidget.h"
#include "examples/SphericalRayTracer/Display.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  Scene* scene = new Scene(Colord(0.4, 0.4, 0.4));

  Sphere* sphere1 = new Sphere(Vector3d(0, 1, 0), 1);
  Sphere* sphere2 = new Sphere(Vector3d(0, -0.3, 0), 1);
  Union* u = new Union();
  u->add(sphere1);
  u->add(sphere2);
  u->material().setDiffuseColor(Colord(0.1, 0.1, 0.1));
  u->material().setSpecularColor(Colord(0.1, 0.1, 0.1));
  u->material().setAbsorbanceColor(Colord(0.3, 0.2, 0.2));
  u->material().setRefractionIndex(1.52);
  
  Sphere* sphere3 = new Sphere(Vector3d(2.5, 1, 0), 1);
  sphere3->material().setDiffuseColor(Colord(1, 0, 0));
  sphere3->material().setSpecularColor(Colord(0.2, 0.2, 0.2));
  
  Plane* plane = new Plane(Vector3d(0, -1, 0), 2);
  plane->material().setDiffuseColor(Colord(0, 0, 1));
  
  scene->add(u);
  scene->add(sphere3);
  scene->add(plane);
  
  Light* light1 = new Light(Vector3d(-3, -3, -1), Colord(0.4, 0.4, 0.4));
  scene->addLight(light1);
  
  Display* display = new Display(scene);
  display->show();
  
  return app.exec();
}
