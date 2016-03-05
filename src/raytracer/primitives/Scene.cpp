#include "raytracer/primitives/Scene.h"
#include "raytracer/lights/Light.h"

using namespace raytracer;

Scene::~Scene() {
  for (const auto& light : m_lights) {
    delete light;
  }
}
