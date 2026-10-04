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

  /**
    * Wraps primitive in a CompiledPrimitive, ids it "<parent id>:compiled-geometry",
    * names it "<parent name> Geometry" (or fallbackName when parent has no
    * name), and adds it as a child of parent. This is the common shape of an
    * importer attaching its compiled geometry to the Group/root element it
    * built, once parsing is done.
    *
    * @returns the raw CompiledPrimitive*, owned by parent.
    */
  static CompiledPrimitive* attachTo(Element& parent, std::shared_ptr<render::Primitive> primitive,
                                     const QString& fallbackName);

private:
  std::shared_ptr<render::Primitive> m_primitive;
};
