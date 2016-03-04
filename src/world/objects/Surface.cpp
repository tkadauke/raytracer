#include "world/objects/Surface.h"
#include "world/objects/Material.h"
#include "raytracer/primitives/Instance.h"
#include "raytracer/primitives/Composite.h"

Surface::Surface(Element* parent)
  : Element(parent),
    m_scale(Vector3d::one()),
    m_material(nullptr),
    m_visible(true)
{
}

Matrix4d Surface::localTransform() const {
  return Matrix4d::translate(position()) *
         Matrix3d::rotate(rotation()) *
         Matrix3d::scale(scale());
}

Matrix4d Surface::globalTransform() const {
  Matrix4d parentTransform;
  if (Surface* p = dynamic_cast<Surface*>(parent())) {
    parentTransform = p->globalTransform();
  }
  return parentTransform * localTransform();
}

void Surface::setMatrix(const Matrix4d& matrix) {
  setPosition(matrix.translationVector());
  setRotation(Matrix3d(matrix).rotationVector());
  setScale(Matrix3d(matrix).scaleVector());
}

std::shared_ptr<raytracer::Primitive> Surface::applyTransform(std::shared_ptr<raytracer::Primitive> primitive) const {
  auto result = std::make_shared<raytracer::Instance>(primitive);
  result->setMatrix(localTransform());
  return result;
}

std::shared_ptr<raytracer::Primitive> Surface::toRaytracer() const {
  auto primitive = toRaytracerPrimitive();
  if (!primitive) {
    return primitive;
  }
  
  if (material()) {
    primitive->setMaterial(material()->toRaytracerMaterial());
  } else {
    primitive->setMaterial(Material::defaultMaterial()->toRaytracerMaterial());
  }
  
  if (childElements().size() > 0) {
    auto composite = std::dynamic_pointer_cast<raytracer::Composite>(primitive);
    if (!composite) {
      composite = std::make_shared<raytracer::Composite>();
      composite->add(primitive);
    }
    
    for (const auto& child : childElements()) {
      Surface* surface = qobject_cast<Surface*>(child);
      if (surface) {
        composite->add(surface->toRaytracer());
      }
    }

    return applyTransform(composite);
  } else {
    return applyTransform(primitive);
  }
}

bool Surface::canHaveChild(Element* child) const {
  return dynamic_cast<Surface*>(child) != nullptr;
}

void Surface::leaveParent() {
  setMatrix(globalTransform());
}

void Surface::joinParent() {
  Matrix4d matrix;
  if (Surface* p = dynamic_cast<Surface*>(parent())) {
    matrix = p->globalTransform().inverted();
  }
  
  setMatrix(matrix * localTransform());
}

#include "Surface.moc"
