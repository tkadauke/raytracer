#include "world/objects/Surface.h"
#include "world/objects/Material.h"
#include "raytracer/primitives/Instance.h"

Surface::Surface(Element* parent)
  : Element(parent),
    m_scale(Vector3d::one()),
    m_material(nullptr),
    m_visible(true)
{
}

std::shared_ptr<raytracer::Primitive> Surface::applyTransform(std::shared_ptr<raytracer::Primitive> primitive) const {
  auto result = std::make_shared<raytracer::Instance>(primitive);
  const Matrix4d matrix =
    Matrix4d::translate(position()) *
    Matrix3d::rotate(rotation()) *
    Matrix3d::scale(scale());

  result->setMatrix(matrix);
  return result;
}

std::shared_ptr<raytracer::Primitive> Surface::toRaytracer() const {
  auto primitive = toRaytracerPrimitive();
  if (material()) {
    primitive->setMaterial(material()->toRaytracerMaterial());
  } else {
    primitive->setMaterial(Material::defaultMaterial()->toRaytracerMaterial());
  }
  return applyTransform(primitive);
}

#include "Surface.moc"
