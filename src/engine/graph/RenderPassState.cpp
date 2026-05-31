#include "engine/graph/RenderPassState.h"

#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/WireframePassState.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::graph {
  namespace {
    class RenderPassStateJsonFactory {
    public:
      RenderPassStateJsonFactory(std::vector<RenderPassKind> kinds, RenderExecutorKind executor)
          : m_kinds(std::move(kinds)),
            m_executor(executor) {
      }

      virtual ~RenderPassStateJsonFactory() = default;

      bool matches(RenderPassKind kind, RenderExecutorKind executor) const {
        return executor == m_executor &&
               std::find(m_kinds.begin(), m_kinds.end(), kind) != m_kinds.end();
      }

      virtual std::shared_ptr<const RenderPassState> create(const QJsonObject& object,
                                                            const std::string& path) const = 0;

    private:
      std::vector<RenderPassKind> m_kinds;
      RenderExecutorKind m_executor;
    };

    template<class State>
    class TypedRenderPassStateJsonFactory : public RenderPassStateJsonFactory {
    public:
      using RenderPassStateJsonFactory::RenderPassStateJsonFactory;

      std::shared_ptr<const RenderPassState> create(const QJsonObject& object,
                                                    const std::string& path) const override {
        return std::make_shared<State>(State::fromJson(object, path));
      }
    };

    class PostProcessAAStateJsonFactory : public RenderPassStateJsonFactory {
    public:
      PostProcessAAStateJsonFactory()
          : RenderPassStateJsonFactory({RenderPassKind::PostProcess},
                                       RenderExecutorKind::PostProcess) {
      }

      std::shared_ptr<const RenderPassState> create(const QJsonObject& object,
                                                    const std::string& path) const override {
        return PostProcessAAState::fromJson(object, path);
      }
    };

    const std::vector<const RenderPassStateJsonFactory*>& passStateJsonFactories() {
      static const TypedRenderPassStateJsonFactory<RasterBeautyPassState> rasterBeauty(
        {RenderPassKind::Beauty, RenderPassKind::AOV}, RenderExecutorKind::Rasterizer);
      static const TypedRenderPassStateJsonFactory<RasterShadowPassState> rasterShadow(
        {RenderPassKind::Shadow}, RenderExecutorKind::Rasterizer);
      static const TypedRenderPassStateJsonFactory<RasterVisibilityPassState> rasterVisibility(
        {RenderPassKind::Visibility}, RenderExecutorKind::Rasterizer);
      static const TypedRenderPassStateJsonFactory<RaytracerBeautyPassState> raytracerBeauty(
        {RenderPassKind::Beauty}, RenderExecutorKind::Raytracer);
      static const TypedRenderPassStateJsonFactory<RaytracerBeautyPassState> wavefrontBeauty(
        {RenderPassKind::Beauty}, RenderExecutorKind::Wavefront);
      static const PostProcessAAStateJsonFactory postProcessAA;
      static const TypedRenderPassStateJsonFactory<WireframePassState> wireframe(
        {RenderPassKind::Beauty, RenderPassKind::Overlay}, RenderExecutorKind::Wireframe);

      static const std::vector<const RenderPassStateJsonFactory*> result = {
        &rasterBeauty,    &rasterShadow,  &rasterVisibility, &raytracerBeauty,
        &wavefrontBeauty, &postProcessAA, &wireframe};
      return result;
    }
  }

  std::shared_ptr<const RenderPassState> RenderPassState::fromJson(RenderPassKind kind,
                                                                   RenderExecutorKind executor,
                                                                   const QJsonObject& object,
                                                                   const std::string& path) {
    if (object.isEmpty())
      return nullptr;

    for (const auto* factory : passStateJsonFactories()) {
      if (factory->matches(kind, executor)) {
        return factory->create(object, path);
      }
    }

    throw std::runtime_error("Invalid render graph JSON at " + path +
                             ": parameters are not supported for pass kind '" + toString(kind) +
                             "' and executor '" + toString(executor) + "'");
  }

  const RasterBeautyPassState* RenderPassState::asRasterBeautyPassState() const {
    return nullptr;
  }

  const RasterShadowPassState* RenderPassState::asRasterShadowPassState() const {
    return nullptr;
  }

  const RasterVisibilityPassState* RenderPassState::asRasterVisibilityPassState() const {
    return nullptr;
  }

  const RaytracerBeautyPassState* RenderPassState::asRaytracerBeautyPassState() const {
    return nullptr;
  }

  const WireframePassState* RenderPassState::asWireframePassState() const {
    return nullptr;
  }

  const PostProcessAAState* RenderPassState::asPostProcessAAState() const {
    return nullptr;
  }
}
