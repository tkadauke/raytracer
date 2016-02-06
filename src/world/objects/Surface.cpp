#include "world/objects/Surface.h"
#include "raytracer/primitives/Instance.h"

#include <iostream>
using namespace std;

Surface::Surface(Element* parent)
  : Element(parent), m_scale(Vector3d(1, 1, 1)), m_visible(true)
{
}

raytracer::Primitive* Surface::applyTransform(raytracer::Primitive* primitive) const {
  raytracer::Instance* result = new raytracer::Instance(primitive);
  const Matrix4d matrix =
    Matrix4d::translate(position()) *
    Matrix3d::scale(scale()) * 
    Matrix3d::rotate(rotation());

  result->setMatrix(matrix);
  return result;
}

#include "Surface.moc"
