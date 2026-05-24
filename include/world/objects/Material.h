#pragma once
#include <memory>

#include <QString>

#include "world/objects/Element.h"

namespace render {
  class Material;
}

/**
  * Abstract base class for materials.
  */
class Material : public Element {
  Q_OBJECT
  Q_PROPERTY(QString sidedness READ sidednessName WRITE setSidednessName)

public:
  enum class Sidedness { Front, Back, TwoSided };

  /**
    * Returns the default material, which is a black matte material.
    */
  static Material* defaultMaterial();

  /**
    * Default constructor.
    */
  explicit Material(Element* parent = nullptr);

  inline Sidedness sidedness() const {
    return m_sidedness;
  }

  void setSidedness(Sidedness sidedness);

  QString sidednessName() const;
  void setSidednessName(const QString& sidedness);

  /**
    * Converts this material to the corresponding class in the raytracer
    * namespace.
    */
  virtual std::shared_ptr<render::Material> toRaytracerMaterial() const = 0;

protected:
  void applyMaterialProperties(const std::shared_ptr<render::Material>& material) const;

private:
  Sidedness m_sidedness;
};
