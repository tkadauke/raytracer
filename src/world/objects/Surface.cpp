#include "world/objects/Surface.h"
#include "world/objects/Material.h"
#include "raytracer/primitives/Instance.h"

#include <iostream>
using namespace std;

Surface::Surface(Element* parent)
  : Element(parent), m_scale(Vector3d::one()), m_visible(true)
{
}

raytracer::Primitive* Surface::applyTransform(raytracer::Primitive* primitive) const {
  auto result = new raytracer::Instance(primitive);
  const Matrix4d matrix =
    Matrix4d::translate(position()) *
    Matrix3d::rotate(rotation()) *
    Matrix3d::scale(scale());

  result->setMatrix(matrix);
  return result;
}

raytracer::Primitive* Surface::toRaytracer() const {
  auto primitive = toRaytracerPrimitive();
  if (material()) {
    primitive->setMaterial(material()->toRaytracerMaterial());
  }
  return applyTransform(primitive);
}

#include "Surface.moc"
