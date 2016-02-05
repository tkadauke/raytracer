#include "world/objects/Surface.h"
#include "raytracer/primitives/Instance.h"

Surface::Surface(Element* parent)
  : Element(parent), m_visible(true), m_scale(Vector3d(1, 1, 1))
{
}

raytracer::Primitive* Surface::applyScale(raytracer::Primitive* primitive) const {
  raytracer::Instance* result = new raytracer::Instance(primitive);
  result->setMatrix(Matrix3d::scale(m_scale));
  return result;
}

#include "Surface.moc"
