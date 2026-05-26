#pragma once

#include <memory>

#include "world/objects/Surface.h"

namespace render {
  class Primitive;
}

/**
  * Transient adapter for geometry produced by an authoring import pipeline.
  *
  * This class is intentionally not registered with ElementFactory. It is not a
  * durable scene JSON type; importers attach generated instances so the runtime
  * scene receives ordinary render::Primitive trees.
  */
class CompiledPrimitive : public Surface {
  Q_OBJECT

public:
  explicit CompiledPrimitive(std::shared_ptr<render::Primitive> primitive,
                             Element* parent = nullptr);

  std::shared_ptr<render::Primitive> toRaytracerPrimitive() const override;

private:
  std::shared_ptr<render::Primitive> m_primitive;
};
