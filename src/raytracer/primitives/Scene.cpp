#include "raytracer/primitives/Scene.h"
#include "raytracer/Light.h"

using namespace raytracer;

Scene::~Scene() {
  for (const auto& light : m_lights) {
    delete light;
  }
}
