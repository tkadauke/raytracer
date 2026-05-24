#include "engine/graph/RenderPassState.h"

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/WireframePassState.h"

#include <stdexcept>

namespace engine::graph {
  std::shared_ptr<const RenderPassState> RenderPassState::fromJson(RenderPassKind kind,
                                                                   RenderExecutorKind executor,
                                                                   const QJsonObject& object,
                                                                   const std::string& path) {
    if (object.isEmpty())
      return nullptr;

    if (kind == RenderPassKind::Beauty && executor == RenderExecutorKind::Rasterizer) {
      return std::make_shared<RasterBeautyPassState>(RasterBeautyPassState::fromJson(object, path));
    }

    if (kind == RenderPassKind::Shadow && executor == RenderExecutorKind::Rasterizer) {
      return std::make_shared<RasterShadowPassState>(RasterShadowPassState::fromJson(object, path));
    }

    if (kind == RenderPassKind::PostProcess && executor == RenderExecutorKind::PostProcess) {
      return PostProcessAAState::fromJson(object, path);
    }

    if ((kind == RenderPassKind::Beauty || kind == RenderPassKind::Overlay) &&
        executor == RenderExecutorKind::Wireframe) {
      return std::make_shared<WireframePassState>(WireframePassState::fromJson(object, path));
    }

    throw std::runtime_error("Invalid render graph JSON at " + path +
                             ": parameters are not supported for pass kind '" + toString(kind) +
                             "' and executor '" + toString(executor) + "'");
  }
}
