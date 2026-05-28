#pragma once

#include <memory>

#include "render/primitives/SpatialIndex.h"

namespace render {
  /**
    * Named acceleration choices for primitive containers. Callers use this
    * enum plus SpatialIndex without depending on the concrete container class.
    */
  enum class SpatialIndexKind {
    Linear,
    Grid,
    BVH,
  };

  /**
    * Create a primitive container implementing the SpatialIndex contract.
    *
    * Linear returns the plain Composite fallback. Grid and BVH return the
    * existing accelerated containers behind the same abstraction.
    */
  std::shared_ptr<SpatialIndex> makeSpatialIndex(SpatialIndexKind kind);

  /**
    * Return the Primitive side of an index created by makeSpatialIndex().
    * Spatial indexes are still renderable scene primitives; this helper keeps
    * callers from depending on the concrete multiple-inheritance type.
    */
  std::shared_ptr<Primitive> spatialIndexPrimitive(const std::shared_ptr<SpatialIndex>& index);
}
