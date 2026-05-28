#include "render/primitives/SpatialIndexFactory.h"

#include "render/primitives/BVH.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Grid.h"
#include "render/primitives/Primitive.h"

#include <stdexcept>

namespace render {
  std::shared_ptr<SpatialIndex> makeSpatialIndex(SpatialIndexKind kind) {
    switch (kind) {
      case SpatialIndexKind::Linear:
        return std::make_shared<Composite>();
      case SpatialIndexKind::Grid:
        return std::make_shared<Grid>();
      case SpatialIndexKind::BVH:
        return std::make_shared<BVH>();
    }

    throw std::invalid_argument("unknown spatial index kind");
  }

  std::shared_ptr<Primitive> spatialIndexPrimitive(const std::shared_ptr<SpatialIndex>& index) {
    return std::dynamic_pointer_cast<Primitive>(index);
  }
}
