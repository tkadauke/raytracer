#include "raytracer/primitives/Scene.h"
#include "Light.h"

Scene::~Scene() {
  for (Lights::const_iterator i = m_lights.begin(); i != m_lights.end(); ++i) {
    delete *i;
  }
}
