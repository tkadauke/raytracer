#pragma once

#include <memory>

#include "world/objects/Light.h"

/**
  * Represents a one-sided rectangular area light.
  *
  * The editable rectangle is centered at the transform position, spans local X
  * by `width`, spans local Z by `height`, and emits toward local negative Y.
  */
class RectangularAreaLight : public Light {
  Q_OBJECT
  Q_PROPERTY(double width READ width WRITE setWidth)
  Q_PROPERTY(double height READ height WRITE setHeight)

public:
  explicit RectangularAreaLight(Element* parent = nullptr);

  double width() const;
  void setWidth(double width);

  double height() const;
  void setHeight(double height);

  std::shared_ptr<render::Light> toRaytracer() const override;

private:
  double m_width;
  double m_height;
};
