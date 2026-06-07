#include "world/objects/Camera.h"

#include "render/cameras/Camera.h"

Camera::Camera(Element* parent)
    : Element(parent),
      m_position(Vector3d(0, 0, -1)),
      m_target(Vector3d::null) {
}

void Camera::applyCameraProperties(const std::shared_ptr<render::Camera>& camera) const {
  if (camera) {
    attachRuntimeAnimationTracks(*camera);
  }
}
