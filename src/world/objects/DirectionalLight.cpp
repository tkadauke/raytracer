#include "world/objects/ElementFactory.h"
#include "world/objects/DirectionalLight.h"
#include "render/lights/DirectionalLight.h"

DirectionalLight::DirectionalLight(Element* parent)
    : Light(parent),
      m_direction(-0.5, -1, -0.5) {
}

std::shared_ptr<render::Light> DirectionalLight::toRaytracer() const {
  auto light = make_named<render::DirectionalLight>(Matrix3d(globalTransform()) * direction(),
                                                    color() * intensity());
  attachRuntimeAnimationTracks(*light);
  return light;
}

static bool dummy = ElementFactory::self().registerClass<DirectionalLight>("DirectionalLight");
