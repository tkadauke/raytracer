#include "world/objects/Camera.h"

Camera::Camera(Element* parent)
  : Element(parent),
    m_position(Vector3d(0, 0, -1)),
    m_target(Vector3d::null())
{
}

#include "Camera.moc"
