#include "engine/graph/RenderPassPayload.h"

#include "core/Buffer.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/util/BufferUtils.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterShadowMapArtifact.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/RenderGraphArtifactCache.h"
#include "engine/graph/RenderExecutionContext.h"
#include "engine/graph/RenderResourceStorage.h"
#include "engine/graph/WireframePassState.h"
#include "engine/raster/OpenGLRasterizer.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raster/RasterVisibilitySet.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/Camera.h"
#include "render/HomogeneousClipVolume.h"
#include "render/primitives/Scene.h"
#include "render/samplers/SampleStream.h"
#include "render/State.h"
#include "render/tonemap/Tonemap.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::graph {
  namespace {
    std::runtime_error passError(const RenderPassNode& pass, const std::string& message) {
      return std::runtime_error("pass '" + pass.id + "': " + message);
    }

    void requireColorResource(const RenderResourceStorage& storage,
                              const RenderResourceId& resource, const RenderPassNode& pass) {
      if (!storage.resource(resource).colorBacked()) {
        throw passError(pass, "resource '" + resource + "' is not color-backed");
      }
    }

    void requireDepthResource(const RenderResourceStorage& storage,
                              const RenderResourceId& resource, const RenderPassNode& pass) {
      if (!storage.resource(resource).depthBacked()) {
        throw passError(pass, "resource '" + resource + "' is not depth-backed");
      }
    }

    void requireStencilResource(const RenderResourceStorage& storage,
                                const RenderResourceId& resource, const RenderPassNode& pass) {
      if (!storage.resource(resource).stencilBacked()) {
        throw passError(pass, "resource '" + resource + "' is not stencil-backed");
      }
    }

    void requireObjectIdResource(const RenderResourceStorage& storage,
                                 const RenderResourceId& resource, const RenderPassNode& pass) {
      if (!storage.resource(resource).objectIdBacked()) {
        throw passError(pass, "resource '" + resource + "' is not object-id-backed");
      }
    }

    void requireMatchingSize(const Buffer<Colord>& source, const Buffer<Colord>& destination,
                             const std::string& action) {
      if (!core::util::bufferDimensionsEqual(source, destination)) {
        throw std::runtime_error(action + " requires matching color buffer dimensions");
      }
    }

    void requireMatchingSize(const Buffer<double>& source, const Buffer<Colord>& destination,
                             const std::string& action) {
      if (!core::util::bufferDimensionsEqual(source, destination)) {
        throw std::runtime_error(action + " requires matching depth/color buffer dimensions");
      }
    }

    bool hasFiniteDepthRange(const Buffer<double>& buffer, double* minDepth, double* maxDepth) {
      *minDepth = std::numeric_limits<double>::infinity();
      *maxDepth = -std::numeric_limits<double>::infinity();
      for (int y = 0; y != buffer.height(); ++y) {
        for (int x = 0; x != buffer.width(); ++x) {
          const double depth = buffer[y][x];
          if (!std::isfinite(depth)) {
            continue;
          }
          *minDepth = std::min(*minDepth, depth);
          *maxDepth = std::max(*maxDepth, depth);
        }
      }
      return std::isfinite(*minDepth) && std::isfinite(*maxDepth);
    }

    bool hasFiniteColorRange(const Buffer<Colord>& buffer, Colord* minColor, Colord* maxColor) {
      double minimum[3] = {std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::infinity()};
      double maximum[3] = {-std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()};

      for (int y = 0; y != buffer.height(); ++y) {
        for (int x = 0; x != buffer.width(); ++x) {
          for (int component = 0; component != 3; ++component) {
            const double value = buffer[y][x][component];
            if (!std::isfinite(value)) {
              continue;
            }
            minimum[component] = std::min(minimum[component], value);
            maximum[component] = std::max(maximum[component], value);
          }
        }
      }

      *minColor = Colord(minimum[0], minimum[1], minimum[2]);
      *maxColor = Colord(maximum[0], maximum[1], maximum[2]);
      return std::isfinite(minimum[0]) && std::isfinite(minimum[1]) && std::isfinite(minimum[2]) &&
             std::isfinite(maximum[0]) && std::isfinite(maximum[1]) && std::isfinite(maximum[2]);
    }

    double normalizedComponent(double value, double minimum, double maximum) {
      if (!std::isfinite(value)) {
        return 0.0;
      }
      const double range = maximum - minimum;
      if (range <= 1e-9) {
        return 0.5;
      }
      return std::clamp((value - minimum) / range, 0.0, 1.0);
    }

    Colord colorForObjectId(std::uint32_t id) {
      if (id == 0) {
        return Colord::black();
      }

      std::uint32_t hash = id * 2654435761u;
      hash ^= hash >> 16;
      return Colord(static_cast<double>((hash >> 16) & 0xffu) / 255.0,
                    static_cast<double>((hash >> 8) & 0xffu) / 255.0,
                    static_cast<double>(hash & 0xffu) / 255.0);
    }

    class SceneRasterIdentityIds {
    public:
      explicit SceneRasterIdentityIds(const std::shared_ptr<render::Scene>& scene) {
        if (!scene) {
          return;
        }

        std::uint32_t nextPrimitiveId = 1;
        std::uint32_t nextMaterialId = 1;
        scene->forEachLeaf(
          [&](const render::Primitive* primitive, std::shared_ptr<render::Material> material) {
            if (primitive && m_primitiveIds.emplace(primitive, nextPrimitiveId).second) {
              ++nextPrimitiveId;
            }
            if (material && m_materialIds.emplace(material.get(), nextMaterialId).second) {
              ++nextMaterialId;
            }
          });
      }

      std::uint32_t primitiveId(const render::Primitive* primitive) const {
        const auto it = m_primitiveIds.find(primitive);
        return it == m_primitiveIds.end() ? 0 : it->second;
      }

      std::uint32_t materialId(const render::Material* material) const {
        const auto it = m_materialIds.find(material);
        return it == m_materialIds.end() ? 0 : it->second;
      }

    private:
      std::map<const render::Primitive*, std::uint32_t> m_primitiveIds;
      std::map<const render::Material*, std::uint32_t> m_materialIds;
    };

    bool hasFeature(const RenderPassNode& pass, const RenderFeatureKind& feature) {
      return std::any_of(pass.features.begin(), pass.features.end(),
                         [&](const RenderFeatureKind& value) { return value == feature; });
    }

    class RasterVisibilityInput {
    public:
      explicit RasterVisibilityInput(const RenderExecutionContext& context)
          : m_context(context) {
      }

      void applyTo(::engine::raster::Rasterizer& rasterizer) const {
        if (const auto set = selectedVisibilitySet()) {
          rasterizer.setVisibilitySet(set);
        }
      }

      bool applyTo(::engine::raster::OpenGLRasterizer& rasterizer) const {
        if (const auto set = selectedVisibilitySet()) {
          rasterizer.setVisibilitySet(set);
          return true;
        }
        return false;
      }

    private:
      std::shared_ptr<const ::engine::raster::RasterVisibilitySet> selectedVisibilitySet() const {
        for (const auto& read : m_context.pass().reads) {
          const auto& resource = m_context.storage().resource(read.resource);
          if (resource.descriptor().type != RenderResourceType::VisibilitySet ||
              resource.substituteDefault()) {
            continue;
          }

          const auto visibilitySet = resource.visibilitySet();
          if (visibilitySet) {
            return visibilitySet;
          }
        }
        return nullptr;
      }

      const RenderExecutionContext& m_context;
    };

    void applyRasterShadowInputs(const RenderExecutionContext& context,
                                 const RasterBeautyPassState& beautyState,
                                 ::engine::raster::Rasterizer& rasterizer) {
      for (const auto& read : context.pass().reads) {
        const auto& resource = context.storage().resource(read.resource);
        if (resource.descriptor().type != RenderResourceType::ShadowMap ||
            resource.substituteDefault()) {
          continue;
        }

        const auto state = resource.state();
        const auto* shadowState = state ? state->asRasterShadowPassState() : nullptr;
        if (shadowState) {
          shadowState->applyTo(rasterizer);
        } else if (!beautyState.shadows().empty()) {
          beautyState.shadows().applyTo(rasterizer);
          rasterizer.setShadowMapsEnabled(true);
        } else {
          RasterShadowPassState::previewDefaults().applyTo(rasterizer);
        }

        if (const auto artifact = resource.artifact()) {
          artifact->applyRasterShadowMapsTo(rasterizer);
        }
      }
    }

    bool applyRasterShadowInputs(const RenderExecutionContext& context,
                                 const RasterBeautyPassState& beautyState,
                                 ::engine::raster::OpenGLRasterizer& rasterizer) {
      bool applied = false;
      for (const auto& read : context.pass().reads) {
        const auto& resource = context.storage().resource(read.resource);
        if (resource.descriptor().type != RenderResourceType::ShadowMap ||
            resource.substituteDefault()) {
          continue;
        }

        const auto state = resource.state();
        const auto* shadowState = state ? state->asRasterShadowPassState() : nullptr;
        if (shadowState) {
          shadowState->applyTo(rasterizer);
        } else if (!beautyState.shadows().empty()) {
          beautyState.shadows().applyTo(rasterizer);
          rasterizer.setShadowMapsEnabled(true);
        } else {
          RasterShadowPassState::previewDefaults().applyTo(rasterizer);
        }

        if (const auto artifact = resource.artifact()) {
          applied = artifact->applyRasterShadowMapsTo(rasterizer) || applied;
        }
      }
      return applied;
    }

    void prepareEngine(render::RenderEngine& engine, const GraphRenderEngine& graph, bool cancelled,
                       std::shared_ptr<render::Tonemap> tonemap) {
      engine.setTonemap(std::move(tonemap));
      if (graph.hasBackgroundColorOverride()) {
        engine.setBackgroundColor(graph.backgroundColor());
      }

      if (cancelled) {
        engine.cancel();
      } else {
        engine.uncancel();
      }
    }

    class BeautyPassPayload : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        auto engine = createEngine(context);
        prepareEngine(*engine, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(engine);
        engine->render(context.storage().color(write.resource));
      }

      bool executeDisplay(RenderExecutionContext& context, Buffer<unsigned int>& buffer,
                          std::shared_ptr<render::Tonemap> tonemap) override {
        auto engine = createEngine(context);
        prepareEngine(*engine, context.graph(), context.cancelled(), std::move(tonemap));
        context.setActiveEngine(engine);
        engine->render(buffer);
        return true;
      }

    private:
      virtual std::shared_ptr<render::RenderEngine>
      createEngine(const RenderExecutionContext& context) const = 0;
    };

    /**
      * Whole-frame beauty payload backed by the Whitted raytracer.
      */
    class RaytraceBeautyPass : public BeautyPassPayload {
    public:
      bool executeDisplayAndStore(RenderExecutionContext& context, Buffer<unsigned int>& buffer,
                                  std::shared_ptr<render::Tonemap> tonemap) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        auto raytracer =
          std::static_pointer_cast<::engine::raytracer::Raytracer>(createEngine(context));
        prepareEngine(*raytracer, context.graph(), context.cancelled(), std::move(tonemap));
        context.setActiveEngine(raytracer);
        raytracer->render(context.storage().color(write.resource), buffer, raytracer->tonemap());
        return true;
      }

    private:
      std::shared_ptr<render::RenderEngine>
      createEngine(const RenderExecutionContext& context) const override {
        const auto& graph = context.graph();
        auto camera = graph.camera() ? graph.camera()->clone() : nullptr;
        auto raytracer =
          std::make_shared<::engine::raytracer::Raytracer>(std::move(camera), graph.scene());
        RaytracerBeautyPassState::valueFromPass(context.pass()).applyTo(*raytracer);
        return raytracer;
      }
    };

    /**
      * Whole-frame beauty payload backed by the software rasterizer.
      */
    class RasterBeautyPass : public BeautyPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        auto engine = createEngine(context);
        prepareEngine(*engine, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(engine);
        engine->render(context.storage().color(write.resource));
        recordReadbackTrace(context, engine);
      }

      bool executeDisplay(RenderExecutionContext& context, Buffer<unsigned int>& buffer,
                          std::shared_ptr<render::Tonemap> tonemap) override {
        auto engine = createEngine(context);
        prepareEngine(*engine, context.graph(), context.cancelled(), std::move(tonemap));
        context.setActiveEngine(engine);
        engine->render(buffer);
        recordReadbackTrace(context, engine);
        return true;
      }

    private:
      std::shared_ptr<render::RenderEngine>
      createEngine(const RenderExecutionContext& context) const override {
        const auto& graph = context.graph();
        auto camera = graph.camera() ? graph.camera()->clone() : nullptr;
        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(context.pass());
        const auto backend = state.execution().backend();
        auto engine = backend.createEngine(std::move(camera), graph.scene());
        if (backend.usesSoftwareRasterizer()) {
          auto rasterizer = std::static_pointer_cast<::engine::raster::Rasterizer>(engine);
          state.applyTo(*rasterizer);
          RasterVisibilityInput(context).applyTo(*rasterizer);
          applyRasterShadowInputs(context, state, *rasterizer);
        } else if (backend.isOpenGL()) {
          auto rasterizer = std::static_pointer_cast<::engine::raster::OpenGLRasterizer>(engine);
          state.applyTo(*rasterizer);
          if (RasterVisibilityInput(context).applyTo(*rasterizer)) {
            context.recordTraceMessage(
              "OpenGL raster backend consumed graph visibility set during mesh preparation");
          }
          if (applyRasterShadowInputs(context, state, *rasterizer)) {
            context.recordTraceMessage(
              "OpenGL raster backend consumed graph shadow maps during mesh preparation");
          } else if (readsShadowMap(context)) {
            context.recordTraceMessage(
              "OpenGL raster backend found a shadow-map edge without a materialized artifact; "
              "beauty rendered without shadow-map lighting");
          }
        }
        return engine;
      }

      void recordReadbackTrace(RenderExecutionContext& context,
                               const std::shared_ptr<render::RenderEngine>& engine) const {
        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(context.pass());
        if (!state.execution().backend().isOpenGL()) {
          return;
        }

        const auto rasterizer =
          std::static_pointer_cast<::engine::raster::OpenGLRasterizer>(engine);
        for (const auto& message : rasterizer->traceMessages()) {
          context.recordTraceMessage(message);
        }
      }

      bool readsShadowMap(const RenderExecutionContext& context) const {
        return std::any_of(context.pass().reads.begin(), context.pass().reads.end(),
                           [&](const ResourceRead& read) {
                             return context.storage().descriptor(read.resource).type ==
                                    RenderResourceType::ShadowMap;
                           });
      }
    };

    class RasterPreviewShadowPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        const auto& descriptor = context.storage().descriptor(write.resource);
        RenderResource& resource = context.storage().resource(write.resource);
        if (descriptor.type != RenderResourceType::ShadowMap) {
          throw passError(pass, "preview shadow pass must write a shadow-map resource");
        }
        auto state =
          std::make_shared<RasterShadowPassState>(RasterShadowPassState::valueFromPass(pass));
        resource.setState(state);

        if (!context.storage().hasBuffer(write.resource)) {
          return;
        }

        const bool cacheable = descriptor.lifetime == RenderResourceLifetime::PersistentCache;
        const RenderGraphCacheKey cacheKey = RenderGraphCacheKey::forPassOutput(
          pass, descriptor, context.graph().cacheInputFingerprintForPass(pass));
        if (cacheable && restoreFromCache(context, write.resource, cacheKey)) {
          return;
        }

        auto camera = context.graph().camera() ? context.graph().camera()->clone() : nullptr;
        auto rasterizer = std::make_shared<::engine::raster::Rasterizer>(std::move(camera),
                                                                         context.graph().scene());
        state->applyTo(*rasterizer);
        if (context.cancelled()) {
          rasterizer->cancel();
        } else {
          rasterizer->uncancel();
        }
        context.setActiveEngine(rasterizer);
        auto shadowMaps = rasterizer->buildShadowMaps();
        auto artifact = std::make_shared<RasterShadowMapArtifact>(
          cacheKey, shadowMaps, descriptor.width, descriptor.height,
          "raster preview directional shadow-map collection");
        artifact->copyDepthTo(context.storage().depth(write.resource));
        resource.setArtifact(artifact);

        if (cacheable) {
          context.graph().artifactCache()->store(artifact);
          resource.setCacheMetadata(
            {RenderGraphCacheStatus::Stored,
             "cache miss; stored raster preview directional shadow-map artifact"});
        } else {
          resource.setCacheMetadata(
            {RenderGraphCacheStatus::Uncached,
             "raster preview shadow pass materialized a non-cacheable shadow-map artifact"});
        }
      }

    private:
      bool restoreFromCache(RenderExecutionContext& context, const RenderResourceId& resourceId,
                            const RenderGraphCacheKey& cacheKey) const {
        auto artifact = context.graph().artifactCache()->find(cacheKey);
        if (!artifact) {
          return false;
        }

        if (!artifact->copyRasterShadowMapPreviewTo(context.storage().depth(resourceId))) {
          context.storage()
            .resource(resourceId)
            .setCacheMetadata({RenderGraphCacheStatus::Invalidated,
                               "cached artifact type did not match the shadow-map resource"});
          return false;
        }

        context.storage().resource(resourceId).setArtifact(std::move(artifact));
        context.storage()
          .resource(resourceId)
          .setCacheMetadata({RenderGraphCacheStatus::Hit,
                             "restored raster preview directional shadow-map artifact from cache"});
        return true;
      }
    };

    class PrimaryIntersectionAOVPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        clearOutput(context, write.resource);

        auto camera = context.graph().camera() ? context.graph().camera()->clone() : nullptr;
        auto scene = context.graph().scene();
        if (!camera || !scene) {
          return;
        }

        auto plane = camera->viewPlane();
        if (!plane) {
          return;
        }
        const auto& descriptor = context.storage().descriptor(write.resource);
        const Recti targetRect(descriptor.width, descriptor.height);
        plane->setup(camera->matrix(), targetRect);

        Recti renderRect = targetRect;
        if (plane->aspectMode() == render::AspectMode::FitExact) {
          renderRect = plane->innerRect();
        }

        render::NullSampleStream stream;
        for (int y = renderRect.top(); y < renderRect.bottom(); ++y) {
          if (context.cancelled()) {
            return;
          }
          for (int x = renderRect.left(); x < renderRect.right(); ++x) {
            Rayd ray = camera->rayForPixel(static_cast<double>(x) + 0.5,
                                           static_cast<double>(y) + 0.5, stream);
            if (!ray.direction().isDefined()) {
              continue;
            }

            HitPointInterval hits;
            render::State state;
            if (scene->intersect(ray, hits, state)) {
              const HitPoint hit = hits.minWithPositiveDistance();
              if (!hit.isUndefined()) {
                writeHit(context, write.resource, x, y, hit);
              }
            }
          }
        }
      }

    private:
      virtual void clearOutput(RenderExecutionContext& context,
                               const RenderResourceId& resource) = 0;
      virtual void writeHit(RenderExecutionContext& context, const RenderResourceId& resource,
                            int x, int y, const HitPoint& hit) = 0;
    };

    class DepthAOVPass : public PrimaryIntersectionAOVPass {
    private:
      void clearOutput(RenderExecutionContext& context, const RenderResourceId& resource) override {
        requireDepthResource(context.storage(), resource, context.pass());
        context.storage().depth(resource).clear(std::numeric_limits<double>::infinity());
      }

      void writeHit(RenderExecutionContext& context, const RenderResourceId& resource, int x, int y,
                    const HitPoint& hit) override {
        context.storage().depth(resource)[y][x] = hit.distance();
      }
    };

    class StencilAOVPass : public PrimaryIntersectionAOVPass {
    private:
      void clearOutput(RenderExecutionContext& context, const RenderResourceId& resource) override {
        requireStencilResource(context.storage(), resource, context.pass());
        context.storage().stencil(resource).clear(0);
      }

      void writeHit(RenderExecutionContext& context, const RenderResourceId& resource, int x, int y,
                    const HitPoint&) override {
        context.storage().stencil(resource)[y][x] = 0xff;
      }
    };

    class NormalAOVPass : public PrimaryIntersectionAOVPass {
    private:
      void clearOutput(RenderExecutionContext& context, const RenderResourceId& resource) override {
        requireColorResource(context.storage(), resource, context.pass());
        context.storage().color(resource).clear(Colord::black());
      }

      void writeHit(RenderExecutionContext& context, const RenderResourceId& resource, int x, int y,
                    const HitPoint& hit) override {
        const Vector3d normal = hit.normal().normalized();
        if (normal.isUndefined()) {
          return;
        }
        context.storage().color(resource)[y][x] =
          Colord(normal.x() * 0.5 + 0.5, normal.y() * 0.5 + 0.5, normal.z() * 0.5 + 0.5);
      }
    };

    class ObjectIdAOVPass : public PrimaryIntersectionAOVPass {
    private:
      void clearOutput(RenderExecutionContext& context, const RenderResourceId& resource) override {
        requireObjectIdResource(context.storage(), resource, context.pass());
        context.storage().objectId(resource).clear(0);

        m_objectIds.clear();
        std::uint32_t nextId = 1;
        if (auto scene = context.graph().scene()) {
          scene->forEachLeaf(
            [&](const render::Primitive* primitive, std::shared_ptr<render::Material>) {
              if (primitive && m_objectIds.emplace(primitive, nextId).second) {
                ++nextId;
              }
            });
        }
      }

      void writeHit(RenderExecutionContext& context, const RenderResourceId& resource, int x, int y,
                    const HitPoint& hit) override {
        const auto it = m_objectIds.find(hit.primitive());
        context.storage().objectId(resource)[y][x] = it == m_objectIds.end() ? 0 : it->second;
      }

      std::map<const render::Primitive*, std::uint32_t> m_objectIds;
    };

    class MaterialIdAOVPass : public PrimaryIntersectionAOVPass {
    private:
      void clearOutput(RenderExecutionContext& context, const RenderResourceId& resource) override {
        requireObjectIdResource(context.storage(), resource, context.pass());
        context.storage().objectId(resource).clear(0);

        m_materialIdsByPrimitive.clear();
        std::map<const render::Material*, std::uint32_t> idsByMaterial;
        std::uint32_t nextId = 1;
        if (auto scene = context.graph().scene()) {
          scene->forEachLeaf(
            [&](const render::Primitive* primitive, std::shared_ptr<render::Material> material) {
              if (!primitive || !material) {
                return;
              }
              const auto [idIt, inserted] = idsByMaterial.emplace(material.get(), nextId);
              if (inserted) {
                ++nextId;
              }
              m_materialIdsByPrimitive[primitive] = idIt->second;
            });
        }
      }

      void writeHit(RenderExecutionContext& context, const RenderResourceId& resource, int x, int y,
                    const HitPoint& hit) override {
        const auto it = m_materialIdsByPrimitive.find(hit.primitive());
        context.storage().objectId(resource)[y][x] =
          it == m_materialIdsByPrimitive.end() ? 0 : it->second;
      }

      std::map<const render::Primitive*, std::uint32_t> m_materialIdsByPrimitive;
    };

    class WorldPositionAOVPass : public PrimaryIntersectionAOVPass {
    private:
      void clearOutput(RenderExecutionContext& context, const RenderResourceId& resource) override {
        requireColorResource(context.storage(), resource, context.pass());
        const double missing = std::numeric_limits<double>::quiet_NaN();
        context.storage().color(resource).clear(Colord(missing, missing, missing));
      }

      void writeHit(RenderExecutionContext& context, const RenderResourceId& resource, int x, int y,
                    const HitPoint& hit) override {
        const Vector4d& point = hit.point();
        if (point.isUndefined()) {
          return;
        }
        context.storage().color(resource)[y][x] = Colord(point.x(), point.y(), point.z());
      }
    };

    class RasterDiagnosticAOVPass : public RenderPassPayload {
    protected:
      void renderRasterDiagnostics(
        RenderExecutionContext& context,
        const ::engine::raster::Rasterizer::DiagnosticOutputBuffers& outputs) const {
        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(context.pass());
        const auto backend = state.execution().backend();
        if (backend.usesSoftwareRasterizer()) {
          renderSoftwareRasterDiagnostics(context, outputs);
          return;
        }
        if (backend.isOpenGL()) {
          context.recordTraceMessage("OpenGL raster backend selected; diagnostic AOV used "
                                     "software raster fallback");
          renderSoftwareRasterDiagnostics(context, outputs);
          return;
        }
        throw passError(context.pass(), "unsupported raster diagnostic AOV backend");
      }

      void renderSoftwareRasterDiagnostics(
        RenderExecutionContext& context,
        const ::engine::raster::Rasterizer::DiagnosticOutputBuffers& outputs) const {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        const auto& descriptor = context.storage().descriptor(write.resource);

        auto camera = context.graph().camera() ? context.graph().camera()->clone() : nullptr;
        auto rasterizer = std::make_shared<::engine::raster::Rasterizer>(std::move(camera),
                                                                         context.graph().scene());
        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(pass);
        state.applyTo(*rasterizer);
        RasterVisibilityInput(context).applyTo(*rasterizer);
        rasterizer->setDiagnosticOutputBuffers(outputs);

        Buffer<Colord> scratch(descriptor.width, descriptor.height);
        prepareEngine(*rasterizer, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(rasterizer);
        rasterizer->render(scratch);
      }
    };

    class RasterDepthAOVPass : public RasterDiagnosticAOVPass {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireDepthResource(context.storage(), write.resource, pass);

        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(pass);
        if (state.execution().backend().isOpenGL()) {
          renderOpenGLDepth(context, state);
          return;
        }

        ::engine::raster::Rasterizer::DiagnosticOutputBuffers outputs;
        outputs.depth = &context.storage().depth(write.resource);
        renderRasterDiagnostics(context, outputs);
      }

    private:
      void renderOpenGLDepth(RenderExecutionContext& context,
                             const RasterBeautyPassState& state) const {
        auto camera = context.graph().camera() ? context.graph().camera()->clone() : nullptr;
        auto rasterizer = std::make_shared<::engine::raster::OpenGLRasterizer>(
          std::move(camera), context.graph().scene());
        state.applyTo(*rasterizer);

        const auto& write = context.pass().singleWrite();
        prepareEngine(*rasterizer, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(rasterizer);
        rasterizer->renderDepth(context.storage().depth(write.resource));
        context.recordTraceMessage(rasterizer->readbackTraceMessage());
      }
    };

    class RasterStencilAOVPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireStencilResource(context.storage(), write.resource, pass);

        Buffer<std::uint8_t>& stencil = context.storage().stencil(write.resource);
        const RasterBeautyPassState state = stencilAOVState(pass);
        if (state.execution().backend().isOpenGL()) {
          renderOpenGLStencil(context, state, stencil);
          return;
        }

        auto camera = context.graph().camera() ? context.graph().camera()->clone() : nullptr;
        auto rasterizer = std::make_shared<::engine::raster::Rasterizer>(std::move(camera),
                                                                         context.graph().scene());
        state.applyTo(*rasterizer);
        rasterizer->setColorStoreOp(::engine::raster::Rasterizer::AttachmentStoreOp::Discard);

        ::engine::raster::Rasterizer::AttachmentBuffers attachments;
        attachments.stencil = &stencil;
        rasterizer->setAttachmentBuffers(attachments);

        Buffer<Colord> scratch(stencil.width(), stencil.height());
        prepareEngine(*rasterizer, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(rasterizer);
        rasterizer->render(scratch);
      }

    private:
      void renderOpenGLStencil(RenderExecutionContext& context, const RasterBeautyPassState& state,
                               Buffer<std::uint8_t>& stencil) const {
        auto camera = context.graph().camera() ? context.graph().camera()->clone() : nullptr;
        auto rasterizer = std::make_shared<::engine::raster::OpenGLRasterizer>(
          std::move(camera), context.graph().scene());
        state.applyTo(*rasterizer);

        prepareEngine(*rasterizer, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(rasterizer);
        rasterizer->renderStencil(stencil);
        context.recordTraceMessage(rasterizer->readbackTraceMessage());
      }

      RasterBeautyPassState stencilAOVState(const RenderPassNode& pass) const {
        RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(pass);
        state.sampling().setMSAASamples(1);
        state.sampling().setPostProcessAA(::engine::raster::Rasterizer::PostProcessAA::None);
        state.framebuffer().setColorWriteMask(0);
        if (!state.framebuffer().stencilTestEnabled()) {
          state.framebuffer().configureStencilWritePass(0xff);
        }
        return state;
      }
    };

    class RasterNormalAOVPass : public RasterDiagnosticAOVPass {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        Buffer<Colord>& color = context.storage().color(write.resource);
        Buffer<Vector3d> normals(color.width(), color.height());
        ::engine::raster::Rasterizer::DiagnosticOutputBuffers outputs;
        outputs.normal = &normals;
        renderRasterDiagnostics(context, outputs);

        for (int y = 0; y != color.height(); ++y) {
          for (int x = 0; x != color.width(); ++x) {
            const Vector3d normal = normals[y][x].normalized();
            if (normal.isUndefined()) {
              color[y][x] = Colord::black();
              continue;
            }
            color[y][x] =
              Colord(normal.x() * 0.5 + 0.5, normal.y() * 0.5 + 0.5, normal.z() * 0.5 + 0.5);
          }
        }
      }
    };

    class RasterObjectIdAOVPass : public RasterDiagnosticAOVPass {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireObjectIdResource(context.storage(), write.resource, pass);

        Buffer<std::uint32_t>& output = context.storage().objectId(write.resource);
        Buffer<const render::Primitive*> primitives(output.width(), output.height());
        ::engine::raster::Rasterizer::DiagnosticOutputBuffers outputs;
        outputs.primitive = &primitives;
        if (RasterBeautyPassState::valueFromPass(pass).execution().backend().isOpenGL()) {
          context.recordTraceMessage(
            "OpenGL raster backend selected; object ID AOV used software raster diagnostic "
            "fallback for exact ID output");
        }
        renderSoftwareRasterDiagnostics(context, outputs);

        const SceneRasterIdentityIds ids(context.graph().scene());
        for (int y = 0; y != output.height(); ++y) {
          for (int x = 0; x != output.width(); ++x) {
            output[y][x] = ids.primitiveId(primitives[y][x]);
          }
        }
      }
    };

    class RasterMaterialIdAOVPass : public RasterDiagnosticAOVPass {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireObjectIdResource(context.storage(), write.resource, pass);

        Buffer<std::uint32_t>& output = context.storage().objectId(write.resource);
        Buffer<const render::Material*> materials(output.width(), output.height());
        ::engine::raster::Rasterizer::DiagnosticOutputBuffers outputs;
        outputs.material = &materials;
        if (RasterBeautyPassState::valueFromPass(pass).execution().backend().isOpenGL()) {
          context.recordTraceMessage(
            "OpenGL raster backend selected; material ID AOV used software raster diagnostic "
            "fallback for exact ID output");
        }
        renderSoftwareRasterDiagnostics(context, outputs);

        const SceneRasterIdentityIds ids(context.graph().scene());
        for (int y = 0; y != output.height(); ++y) {
          for (int x = 0; x != output.width(); ++x) {
            output[y][x] = ids.materialId(materials[y][x]);
          }
        }
      }
    };

    class RasterWorldPositionAOVPass : public RasterDiagnosticAOVPass {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        Buffer<Colord>& color = context.storage().color(write.resource);
        Buffer<Vector3d> positions(color.width(), color.height());
        ::engine::raster::Rasterizer::DiagnosticOutputBuffers outputs;
        outputs.worldPosition = &positions;
        renderRasterDiagnostics(context, outputs);

        const double missing = std::numeric_limits<double>::quiet_NaN();
        for (int y = 0; y != color.height(); ++y) {
          for (int x = 0; x != color.width(); ++x) {
            const Vector3d& point = positions[y][x];
            color[y][x] = point.isDefined() ? Colord(point.x(), point.y(), point.z())
                                            : Colord(missing, missing, missing);
          }
        }
      }
    };

    class RasterCounterAOVPass : public RasterDiagnosticAOVPass {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        Buffer<Colord>& color = context.storage().color(write.resource);
        Buffer<std::uint32_t> counts(color.width(), color.height());
        ::engine::raster::Rasterizer::DiagnosticOutputBuffers outputs;
        attachCounter(outputs, counts);
        renderRasterDiagnostics(context, outputs);

        for (int y = 0; y != color.height(); ++y) {
          for (int x = 0; x != color.width(); ++x) {
            color[y][x] = colorForCounter(counts[y][x]);
          }
        }
      }

    private:
      static Colord colorForCounter(std::uint32_t value) {
        if (value == 0) {
          return Colord::black();
        }

        constexpr double redThreshold = 16.0;
        const double normalized =
          std::min(1.0, std::log1p(static_cast<double>(value)) / std::log1p(redThreshold));
        const double green = std::max(0.0, 1.0 - std::abs(normalized * 2.0 - 1.0));
        return Colord(normalized, green, 1.0 - normalized);
      }

      virtual void attachCounter(::engine::raster::Rasterizer::DiagnosticOutputBuffers& outputs,
                                 Buffer<std::uint32_t>& counts) const = 0;
    };

    class RasterCoverageCountAOVPass : public RasterCounterAOVPass {
    private:
      void attachCounter(::engine::raster::Rasterizer::DiagnosticOutputBuffers& outputs,
                         Buffer<std::uint32_t>& counts) const override {
        outputs.coverageCount = &counts;
      }
    };

    class RasterDepthTestCountAOVPass : public RasterCounterAOVPass {
    private:
      void attachCounter(::engine::raster::Rasterizer::DiagnosticOutputBuffers& outputs,
                         Buffer<std::uint32_t>& counts) const override {
        outputs.depthTestCount = &counts;
      }
    };

    class RasterDepthPassCountAOVPass : public RasterCounterAOVPass {
    private:
      void attachCounter(::engine::raster::Rasterizer::DiagnosticOutputBuffers& outputs,
                         Buffer<std::uint32_t>& counts) const override {
        outputs.depthPassCount = &counts;
      }
    };

    class RasterShadeCountAOVPass : public RasterCounterAOVPass {
    private:
      void attachCounter(::engine::raster::Rasterizer::DiagnosticOutputBuffers& outputs,
                         Buffer<std::uint32_t>& counts) const override {
        outputs.shadeCount = &counts;
      }
    };

    class RasterColorWriteCountAOVPass : public RasterCounterAOVPass {
    private:
      void attachCounter(::engine::raster::Rasterizer::DiagnosticOutputBuffers& outputs,
                         Buffer<std::uint32_t>& counts) const override {
        outputs.colorWriteCount = &counts;
      }
    };

    class DepthVisualizationPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireDepthResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<double>& depth = context.storage().depth(read.resource);
        Buffer<Colord>& color = context.storage().color(write.resource);
        requireMatchingSize(depth, color, "depth visualization");

        double minDepth = 0.0;
        double maxDepth = 0.0;
        if (!hasFiniteDepthRange(depth, &minDepth, &maxDepth)) {
          color.clear(Colord::black());
          return;
        }

        const double range = std::max(maxDepth - minDepth, 1e-9);
        for (int y = 0; y != depth.height(); ++y) {
          for (int x = 0; x != depth.width(); ++x) {
            const double value = depth[y][x];
            if (!std::isfinite(value)) {
              color[y][x] = Colord::black();
              continue;
            }
            const double normalized = 1.0 - std::clamp((value - minDepth) / range, 0.0, 1.0);
            color[y][x] = Colord(normalized, normalized, normalized);
          }
        }
      }
    };

    class StencilVisualizationPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireStencilResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<std::uint8_t>& stencil = context.storage().stencil(read.resource);
        Buffer<Colord>& color = context.storage().color(write.resource);
        if (!core::util::bufferDimensionsEqual(stencil, color)) {
          throw passError(pass, "stencil visualization requires matching buffer dimensions");
        }

        for (int y = 0; y != stencil.height(); ++y) {
          for (int x = 0; x != stencil.width(); ++x) {
            const double value = static_cast<double>(stencil[y][x]) / 255.0;
            color[y][x] = Colord(value, value, value);
          }
        }
      }
    };

    class ObjectIdVisualizationPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireObjectIdResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<std::uint32_t>& objectIds = context.storage().objectId(read.resource);
        Buffer<Colord>& color = context.storage().color(write.resource);
        if (!core::util::bufferDimensionsEqual(objectIds, color)) {
          throw passError(pass, "object-id visualization requires matching buffer dimensions");
        }

        for (int y = 0; y != objectIds.height(); ++y) {
          for (int x = 0; x != objectIds.width(); ++x) {
            color[y][x] = colorForObjectId(objectIds[y][x]);
          }
        }
      }
    };

    class WorldPositionVisualizationPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<Colord>& worldPositions = context.storage().color(read.resource);
        Buffer<Colord>& color = context.storage().color(write.resource);
        requireMatchingSize(worldPositions, color, "world-position visualization");

        Colord minimum;
        Colord maximum;
        if (!hasFiniteColorRange(worldPositions, &minimum, &maximum)) {
          color.clear(Colord::black());
          return;
        }

        for (int y = 0; y != worldPositions.height(); ++y) {
          for (int x = 0; x != worldPositions.width(); ++x) {
            color[y][x] =
              Colord(normalizedComponent(worldPositions[y][x].r(), minimum.r(), maximum.r()),
                     normalizedComponent(worldPositions[y][x].g(), minimum.g(), maximum.g()),
                     normalizedComponent(worldPositions[y][x].b(), minimum.b(), maximum.b()));
          }
        }
      }
    };

    class ColorCopyPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<Colord>& source = context.storage().color(read.resource);
        Buffer<Colord>& destination = context.storage().color(write.resource);
        requireMatchingSize(source, destination, "color copy");
        core::util::copyBuffer(destination, source);
      }
    };

    class ReadbackPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        const RenderResource& source = context.storage().resource(read.resource);
        if (!source.hasBuffer()) {
          throw passError(pass, "resource '" + read.resource +
                                  "' has no CPU buffer; GPU readback is not implemented yet");
        }

        context.storage().copy(read.resource, write.resource, "readback");
        context.recordTraceMessage("readback copied CPU-materialized resource '" + read.resource +
                                   "' to '" + write.resource + "'");
      }
    };

    class DepthStencilCompositePass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        if (pass.reads.size() < 3 || pass.reads.size() > 5) {
          throw passError(pass,
                          "depth/stencil composite requires base color, foreground color, and "
                          "at least one depth or stencil input");
        }
        requireColorResource(context.storage(), pass.reads[0].resource, pass);
        requireColorResource(context.storage(), pass.reads[1].resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        Inputs inputs = readInputs(context);
        const Buffer<Colord>& baseColor = context.storage().color(inputs.baseColor);
        const Buffer<Colord>& foregroundColor = context.storage().color(inputs.foregroundColor);
        Buffer<Colord>& output = context.storage().color(write.resource);
        requireMatchingSize(baseColor, foregroundColor, "depth/stencil composite");
        requireMatchingSize(baseColor, output, "depth/stencil composite");

        for (int y = 0; y != output.height(); ++y) {
          for (int x = 0; x != output.width(); ++x) {
            output[y][x] =
              foregroundVisibleAt(context, inputs, x, y) ? foregroundColor[y][x] : baseColor[y][x];
          }
        }
      }

    private:
      struct Inputs {
        RenderResourceId baseColor;
        RenderResourceId foregroundColor;
        std::optional<RenderResourceId> baseDepth;
        std::optional<RenderResourceId> foregroundDepth;
        std::optional<RenderResourceId> stencil;
      };

      Inputs readInputs(RenderExecutionContext& context) const {
        const auto& pass = context.pass();
        Inputs inputs;
        inputs.baseColor = pass.reads[0].resource;
        inputs.foregroundColor = pass.reads[1].resource;

        std::vector<RenderResourceId> depthResources;
        for (std::size_t i = 2; i != pass.reads.size(); ++i) {
          const RenderResourceId& resourceId = pass.reads[i].resource;
          const RenderResource& resource = context.storage().resource(resourceId);
          if (resource.depthBacked()) {
            requireDepthResource(context.storage(), resourceId, pass);
            depthResources.push_back(resourceId);
            continue;
          }
          if (resource.stencilBacked()) {
            requireStencilResource(context.storage(), resourceId, pass);
            if (inputs.stencil) {
              throw passError(pass, "depth/stencil composite accepts only one stencil input");
            }
            inputs.stencil = resourceId;
            continue;
          }

          throw passError(pass, "composite input '" + resourceId +
                                  "' is neither depth-backed nor stencil-backed");
        }

        if (!depthResources.empty() && depthResources.size() != 2) {
          throw passError(pass, "depth composite requires both base and foreground depth inputs");
        }
        if (depthResources.size() == 2) {
          inputs.baseDepth = depthResources[0];
          inputs.foregroundDepth = depthResources[1];
        }
        if (!inputs.baseDepth && !inputs.stencil) {
          throw passError(pass, "composite requires depth inputs or a stencil input");
        }
        validateInputShapes(context, inputs);
        return inputs;
      }

      void validateInputShapes(RenderExecutionContext& context, const Inputs& inputs) const {
        const Buffer<Colord>& baseColor = context.storage().color(inputs.baseColor);
        const auto requireShape = [&](const auto& buffer, const std::string& role) {
          if (!core::util::bufferDimensionsEqual(baseColor, buffer)) {
            throw passError(context.pass(),
                            "depth/stencil composite requires matching " + role + " dimensions");
          }
        };

        requireShape(context.storage().color(inputs.foregroundColor), "foreground color");
        if (inputs.baseDepth) {
          requireShape(context.storage().depth(*inputs.baseDepth), "base depth");
        }
        if (inputs.foregroundDepth) {
          requireShape(context.storage().depth(*inputs.foregroundDepth), "foreground depth");
        }
        if (inputs.stencil) {
          requireShape(context.storage().stencil(*inputs.stencil), "stencil");
        }
      }

      bool foregroundVisibleAt(RenderExecutionContext& context, const Inputs& inputs, int x,
                               int y) const {
        if (inputs.stencil && context.storage().stencil(*inputs.stencil)[y][x] == 0) {
          return false;
        }

        if (!inputs.baseDepth || !inputs.foregroundDepth) {
          return true;
        }

        const double foregroundDepth = context.storage().depth(*inputs.foregroundDepth)[y][x];
        if (!std::isfinite(foregroundDepth)) {
          return false;
        }

        const double baseDepth = context.storage().depth(*inputs.baseDepth)[y][x];
        return !std::isfinite(baseDepth) || foregroundDepth <= baseDepth;
      }
    };

    /**
      * Whole-frame beauty payload backed by the wireframe renderer.
      */
    class WireframeBeautyPass : public BeautyPassPayload {
    private:
      std::shared_ptr<render::RenderEngine>
      createEngine(const RenderExecutionContext& context) const override {
        const auto& graph = context.graph();
        auto camera = graph.camera() ? graph.camera()->clone() : nullptr;
        auto wireframe =
          std::make_shared<::engine::wireframe::Wireframe>(std::move(camera), graph.scene());
        WireframePassState::valueFromPass(context.pass()).applyTo(*wireframe);
        return wireframe;
      }
    };

    class WireframeOverlayPassBase : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<Colord>& source = context.storage().color(read.resource);
        Buffer<Colord>& destination = context.storage().color(write.resource);
        requireMatchingSize(source, destination, "color copy");
        core::util::copyBuffer(destination, source);

        Buffer<Colord> overlay(source.width(), source.height());
        auto camera = context.graph().camera() ? context.graph().camera()->clone() : nullptr;
        auto wireframe = std::make_shared<::engine::wireframe::Wireframe>(std::move(camera),
                                                                          context.graph().scene());
        WireframePassState::valueFromPass(pass).applyTo(*wireframe);
        configure(*wireframe);
        wireframe->setBackgroundColor(Colord::black());
        wireframe->setEdgeColor(Colord::white());
        if (context.cancelled()) {
          wireframe->cancel();
        } else {
          wireframe->uncancel();
        }

        context.setActiveEngine(wireframe);
        wireframe->render(overlay);

        const Colord clear = Colord::black();
        for (int y = 0; y != overlay.height(); ++y) {
          for (int x = 0; x != overlay.width(); ++x) {
            if (!(overlay[y][x] == clear)) {
              destination[y][x] = overlay[y][x];
            }
          }
        }
      }

    private:
      virtual void configure(::engine::wireframe::Wireframe& wireframe) const = 0;
    };

    /**
      * Overlay payload that draws tessellated wireframe edges over an existing
      * color image.
      */
    class WireframeOverlayPass : public WireframeOverlayPassBase {
    private:
      void configure(::engine::wireframe::Wireframe& wireframe) const override {
        wireframe.setGeometryMode(::engine::wireframe::Wireframe::GeometryMode::TessellatedEdges);
      }
    };

    /**
      * Overlay payload that draws semantic curve center lines without
      * tessellating them into physical ribbons or tubes.
      */
    class CurveOverlayPass : public WireframeOverlayPassBase {
    private:
      void configure(::engine::wireframe::Wireframe& wireframe) const override {
        wireframe.setGeometryMode(::engine::wireframe::Wireframe::GeometryMode::CurveOverlay);
      }
    };

    /**
      * Image-space color filter pass. The current graph storage model gives
      * postprocess passes distinct read/write resources, so filters copy their
      * input first and then apply their in-place implementation to the output.
      */
    class ColorFilterPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<Colord>& source = context.storage().color(read.resource);
        Buffer<Colord>& destination = context.storage().color(write.resource);
        requireMatchingSize(source, destination, "color copy");
        core::util::copyBuffer(destination, source);
        apply(destination);
      }

    private:
      virtual void apply(Buffer<Colord>& buffer) const = 0;
    };

    class PostProcessAAPass : public ColorFilterPass {
    public:
      explicit PostProcessAAPass(std::shared_ptr<const PostProcessAAState> state)
          : m_state(std::move(state)) {
      }

    private:
      void apply(Buffer<Colord>& buffer) const override {
        m_state->apply(buffer);
      }

    private:
      std::shared_ptr<const PostProcessAAState> m_state;
    };

    /**
      * Postprocess payload that applies the graph engine's tone mapper.
      */
    class TonemapPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<Colord>& source = context.storage().color(read.resource);
        Buffer<Colord>& destination = context.storage().color(write.resource);
        requireMatchingSize(source, destination, "tonemap pass");

        auto tonemap = context.graph().tonemap();
        for (int y = 0; y != source.height(); ++y) {
          for (int x = 0; x != source.width(); ++x) {
            destination[y][x] = tonemap->apply(source[y][x]);
          }
        }
      }
    };

    class RasterVisibilityCullingPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        const auto& descriptor = context.storage().descriptor(write.resource);
        if (descriptor.type != RenderResourceType::VisibilitySet) {
          throw passError(pass, "visibility culling pass must write a visibility-set resource");
        }

        const RasterVisibilityPassState state =
          RasterVisibilityPassState::valueFromPass(context.pass());
        const auto visibilitySet = buildVisibilitySet(context, state);
        context.storage().setVisibilitySet(write.resource, visibilitySet);
        context.recordTraceMessage(traceMessage(*visibilitySet, state));
      }

    private:
      std::shared_ptr<::engine::raster::RasterVisibilitySet>
      buildVisibilitySet(const RenderExecutionContext& context,
                         const RasterVisibilityPassState& state) const {
        auto visibilitySet = std::make_shared<::engine::raster::RasterVisibilitySet>();
        const auto scene = context.graph().scene();
        if (!scene) {
          return visibilitySet;
        }

        const int lod = state.geometry().lod();
        const std::shared_ptr<render::Camera> camera = context.graph().camera();
        const render::HomogeneousClipVolume clipVolume = rasterClipVolume();
        std::unordered_map<const render::Primitive*, MeshStats> meshStats;
        std::vector<VisibleLeafDepth> visibleLeafDepths;
        bool orderable = state.frontToBackOrderingEnabled() && camera != nullptr;
        scene->forEachTransformedLeaf(
          nullptr, Matrix4d(), Matrix3d(), [&](const render::Primitive::TransformedLeaf& leaf) {
            const std::size_t leafIndex = visibilitySet->leafCount();
            const MeshStats stats = meshStatsFor(leaf.primitive, lod, meshStats);
            if (boundsOutsideFrustum(leaf, camera.get(), clipVolume)) {
              visibilitySet->addRejectedLeaf(
                ::engine::raster::RasterVisibilitySet::RejectionReason::Frustum,
                stats.triangleCount, stats.faceCount);
              return;
            }

            visibilitySet->addVisibleLeaf(stats.triangleCount, stats.faceCount);
            if (orderable) {
              if (const auto depth = frontToBackDepth(leaf, camera.get())) {
                visibleLeafDepths.push_back(VisibleLeafDepth{leafIndex, *depth});
              } else {
                orderable = false;
              }
            }
          });
        if (orderable && visibleLeafDepths.size() > 1) {
          std::stable_sort(visibleLeafDepths.begin(), visibleLeafDepths.end(),
                           [](const VisibleLeafDepth& lhs, const VisibleLeafDepth& rhs) {
                             return lhs.depth < rhs.depth;
                           });
          std::vector<std::size_t> visibleLeafOrder;
          visibleLeafOrder.reserve(visibleLeafDepths.size());
          for (const VisibleLeafDepth& leaf : visibleLeafDepths) {
            visibleLeafOrder.push_back(leaf.leafIndex);
          }
          visibilitySet->setVisibleLeafOrder(std::move(visibleLeafOrder));
        }
        return visibilitySet;
      }

      render::HomogeneousClipVolume rasterClipVolume() const {
        const engine::raster::Rasterizer defaultRasterizer(nullptr);
        return render::HomogeneousClipVolume(defaultRasterizer.nearClipDepth(),
                                             defaultRasterizer.farClipDepth());
      }

      bool boundsOutsideFrustum(const render::Primitive::TransformedLeaf& leaf,
                                const render::Camera* camera,
                                const render::HomogeneousClipVolume& clipVolume) const {
        if (!camera) {
          return false;
        }

        const BoundingBoxd bounds = leaf.boundingBox();
        if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite()) {
          return false;
        }

        std::uint8_t sharedOutCode = render::HomogeneousClipVolume::allBits();
        for (const Vector3d& corner : bounds.vertices()) {
          const Vector4d clip = camera->projectPointToClipSpace(corner);
          if (clip.isUndefined()) {
            return false;
          }
          sharedOutCode &= clipVolume.outCode(clip);
          if (sharedOutCode == 0) {
            return false;
          }
        }

        return sharedOutCode != 0;
      }

      std::optional<double> frontToBackDepth(const render::Primitive::TransformedLeaf& leaf,
                                             const render::Camera* camera) const {
        if (!camera) {
          return std::nullopt;
        }

        const BoundingBoxd bounds = leaf.boundingBox();
        if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite()) {
          return std::nullopt;
        }

        const Vector4d clip = camera->projectPointToClipSpace(bounds.center());
        if (clip.isUndefined() || !std::isfinite(clip.z())) {
          return std::nullopt;
        }
        return clip.z();
      }

      struct MeshStats {
        std::size_t triangleCount{0};
        std::size_t faceCount{0};
      };

      struct VisibleLeafDepth {
        std::size_t leafIndex{0};
        double depth{0.0};
      };

      MeshStats
      meshStatsFor(const render::Primitive* primitive, int lod,
                   std::unordered_map<const render::Primitive*, MeshStats>& meshStats) const {
        if (!primitive) {
          return {};
        }

        const auto cached = meshStats.find(primitive);
        if (cached != meshStats.end()) {
          return cached->second;
        }

        MeshStats stats;
        const std::shared_ptr<Mesh> mesh = primitive->tessellate(lod);
        if (mesh) {
          stats.faceCount = mesh->faces().size();
          for (const auto& face : mesh->faces()) {
            if (face.size() >= 3) {
              stats.triangleCount += face.size() - 2;
            }
          }
        }
        meshStats.emplace(primitive, stats);
        return stats;
      }

      std::string traceMessage(const ::engine::raster::RasterVisibilitySet& visibilitySet,
                               const RasterVisibilityPassState& state) const {
        const char* ordering = "unsupported";
        if (visibilitySet.hasVisibleLeafOrder()) {
          ordering = "enabled";
        } else if (!state.frontToBackOrderingEnabled()) {
          ordering = "disabled";
        } else if (visibilitySet.visibleLeafCount() <= 1) {
          ordering = "not_needed";
        }
        std::ostringstream out;
        out << "visibility culling produced a CPU visibility set"
            << "; lod=" << state.geometry().lod() << "; inputLeaves=" << visibilitySet.leafCount()
            << "; inputTriangles=" << visibilitySet.inputTriangleCount()
            << "; visibleLeaves=" << visibilitySet.visibleLeafCount()
            << "; visibleTriangles=" << visibilitySet.visibleTriangleCount()
            << "; rejectedLeaves=" << visibilitySet.rejectedLeafCount()
            << "; rejectedTriangles=" << visibilitySet.rejectedTriangleCount()
            << "; frustumRejectedLeaves="
            << visibilitySet.rejectedLeafCount(
                 ::engine::raster::RasterVisibilitySet::RejectionReason::Frustum)
            << "; frustumRejectedTriangles="
            << visibilitySet.rejectedTriangleCount(
                 ::engine::raster::RasterVisibilitySet::RejectionReason::Frustum)
            << "; frontToBackOrdering=" << ordering
            << "; frontToBackOrderedLeaves=" << visibilitySet.visibleLeafOrder().size()
            << "; CPU raster passes can skip rejected leaves";
        return out.str();
      }
    };

    class BuiltinPassPayloadFactory {
    public:
      virtual ~BuiltinPassPayloadFactory() = default;

      virtual bool matches(const RenderPassNode& pass) const = 0;
      virtual std::unique_ptr<RenderPassPayload> create(const RenderPassNode& pass) const = 0;
    };

    template<class Payload>
    class ExactPassPayloadFactory : public BuiltinPassPayloadFactory {
    public:
      ExactPassPayloadFactory(RenderPassKind kind, RenderExecutorKind executor)
          : m_kind(kind),
            m_executor(executor) {
      }

      bool matches(const RenderPassNode& pass) const override {
        return pass.kind == m_kind && pass.executor == m_executor;
      }

      std::unique_ptr<RenderPassPayload> create(const RenderPassNode&) const override {
        return std::make_unique<Payload>();
      }

    private:
      RenderPassKind m_kind;
      RenderExecutorKind m_executor;
    };

    template<class Payload>
    class FeaturePassPayloadFactory : public BuiltinPassPayloadFactory {
    public:
      FeaturePassPayloadFactory(RenderPassKind kind, RenderExecutorKind executor,
                                std::vector<RenderFeatureKind> requiredFeatures)
          : m_kind(kind),
            m_executor(executor),
            m_requiredFeatures(std::move(requiredFeatures)) {
      }

      bool matches(const RenderPassNode& pass) const override {
        return pass.kind == m_kind && pass.executor == m_executor &&
               std::all_of(m_requiredFeatures.begin(), m_requiredFeatures.end(),
                           [&](const auto& feature) { return hasFeature(pass, feature); });
      }

      std::unique_ptr<RenderPassPayload> create(const RenderPassNode&) const override {
        return std::make_unique<Payload>();
      }

    private:
      RenderPassKind m_kind;
      RenderExecutorKind m_executor;
      std::vector<RenderFeatureKind> m_requiredFeatures;
    };

    class PostProcessAAPayloadFactory : public BuiltinPassPayloadFactory {
    public:
      bool matches(const RenderPassNode& pass) const override {
        return pass.kind == RenderPassKind::PostProcess &&
               pass.executor == RenderExecutorKind::PostProcess &&
               PostProcessAAState::fromPass(pass) != nullptr;
      }

      std::unique_ptr<RenderPassPayload> create(const RenderPassNode& pass) const override {
        return std::make_unique<PostProcessAAPass>(PostProcessAAState::fromPass(pass));
      }
    };

    class WireframeOverlayPayloadFactory : public BuiltinPassPayloadFactory {
    public:
      bool matches(const RenderPassNode& pass) const override {
        return pass.kind == RenderPassKind::Overlay &&
               pass.executor == RenderExecutorKind::Wireframe && !hasFeature(pass, "curve_overlay");
      }

      std::unique_ptr<RenderPassPayload> create(const RenderPassNode&) const override {
        return std::make_unique<WireframeOverlayPass>();
      }
    };

    class DepthStencilCompositePayloadFactory : public BuiltinPassPayloadFactory {
    public:
      bool matches(const RenderPassNode& pass) const override {
        return pass.kind == RenderPassKind::Composite &&
               pass.executor == RenderExecutorKind::Composite &&
               (hasFeature(pass, "depth_composite") || hasFeature(pass, "stencil_composite"));
      }

      std::unique_ptr<RenderPassPayload> create(const RenderPassNode&) const override {
        return std::make_unique<DepthStencilCompositePass>();
      }
    };

    const std::vector<const BuiltinPassPayloadFactory*>& builtinPayloadFactories() {
      static const ExactPassPayloadFactory<RaytraceBeautyPass> raytraceBeauty(
        RenderPassKind::Beauty, RenderExecutorKind::Raytracer);
      static const ExactPassPayloadFactory<RasterBeautyPass> rasterBeauty(
        RenderPassKind::Beauty, RenderExecutorKind::Rasterizer);
      static const ExactPassPayloadFactory<WireframeBeautyPass> wireframeBeauty(
        RenderPassKind::Beauty, RenderExecutorKind::Wireframe);
      static const ExactPassPayloadFactory<TonemapPass> tonemap(RenderPassKind::Tonemap,
                                                                RenderExecutorKind::PostProcess);
      static const ExactPassPayloadFactory<ReadbackPass> readback(RenderPassKind::Readback,
                                                                  RenderExecutorKind::PostProcess);
      static const FeaturePassPayloadFactory<RasterVisibilityCullingPass> rasterVisibility(
        RenderPassKind::Visibility, RenderExecutorKind::Rasterizer, {"visibility", "culling"});
      static const FeaturePassPayloadFactory<RasterPreviewShadowPass> rasterPreviewShadows(
        RenderPassKind::Shadow, RenderExecutorKind::Rasterizer, {"preview_shadows"});
      static const FeaturePassPayloadFactory<DepthVisualizationPass> depthVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"depth", "visualization"});
      static const FeaturePassPayloadFactory<DepthAOVPass> depthAOV(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"depth"});
      static const FeaturePassPayloadFactory<RasterDepthAOVPass> depthAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"depth"});
      static const FeaturePassPayloadFactory<DepthAOVPass> depthAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"depth"});
      static const FeaturePassPayloadFactory<StencilVisualizationPass> stencilVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"stencil", "visualization"});
      static const FeaturePassPayloadFactory<StencilAOVPass> stencilAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"stencil"});
      static const FeaturePassPayloadFactory<RasterStencilAOVPass> stencilAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"stencil"});
      static const FeaturePassPayloadFactory<StencilAOVPass> stencilAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"stencil"});
      static const FeaturePassPayloadFactory<NormalAOVPass> normalAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"normal"});
      static const FeaturePassPayloadFactory<RasterNormalAOVPass> normalAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"normal"});
      static const FeaturePassPayloadFactory<NormalAOVPass> normalAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"normal"});
      static const FeaturePassPayloadFactory<ColorCopyPass> normalVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"normal", "visualization"});
      static const FeaturePassPayloadFactory<ObjectIdVisualizationPass> objectIdVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"object_id", "visualization"});
      static const FeaturePassPayloadFactory<ObjectIdAOVPass> objectIdAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"object_id"});
      static const FeaturePassPayloadFactory<RasterObjectIdAOVPass> objectIdAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"object_id"});
      static const FeaturePassPayloadFactory<ObjectIdAOVPass> objectIdAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"object_id"});
      static const FeaturePassPayloadFactory<ObjectIdVisualizationPass> materialIdVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"material_id", "visualization"});
      static const FeaturePassPayloadFactory<MaterialIdAOVPass> materialIdAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"material_id"});
      static const FeaturePassPayloadFactory<RasterMaterialIdAOVPass> materialIdAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"material_id"});
      static const FeaturePassPayloadFactory<MaterialIdAOVPass> materialIdAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"material_id"});
      static const FeaturePassPayloadFactory<WorldPositionVisualizationPass>
        worldPositionVisualization(RenderPassKind::AOV, RenderExecutorKind::PostProcess,
                                   {"world_position", "visualization"});
      static const FeaturePassPayloadFactory<WorldPositionAOVPass> worldPositionAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"world_position"});
      static const FeaturePassPayloadFactory<RasterWorldPositionAOVPass> worldPositionAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"world_position"});
      static const FeaturePassPayloadFactory<WorldPositionAOVPass> worldPositionAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"world_position"});
      static const FeaturePassPayloadFactory<RasterCoverageCountAOVPass> rasterCoverageCountAOV(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"raster_coverage_count"});
      static const FeaturePassPayloadFactory<RasterDepthTestCountAOVPass> rasterDepthTestCountAOV(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"raster_depth_test_count"});
      static const FeaturePassPayloadFactory<RasterDepthPassCountAOVPass> rasterDepthPassCountAOV(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"raster_depth_pass_count"});
      static const FeaturePassPayloadFactory<RasterShadeCountAOVPass> rasterShadeCountAOV(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"raster_shade_count"});
      static const FeaturePassPayloadFactory<RasterColorWriteCountAOVPass> rasterColorWriteCountAOV(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"raster_color_write_count"});
      static const FeaturePassPayloadFactory<ColorCopyPass> rasterCoverageCountVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess,
        {"raster_coverage_count", "visualization"});
      static const FeaturePassPayloadFactory<ColorCopyPass> rasterDepthTestCountVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess,
        {"raster_depth_test_count", "visualization"});
      static const FeaturePassPayloadFactory<ColorCopyPass> rasterDepthPassCountVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess,
        {"raster_depth_pass_count", "visualization"});
      static const FeaturePassPayloadFactory<ColorCopyPass> rasterShadeCountVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess,
        {"raster_shade_count", "visualization"});
      static const FeaturePassPayloadFactory<ColorCopyPass> rasterColorWriteCountVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess,
        {"raster_color_write_count", "visualization"});
      static const PostProcessAAPayloadFactory postProcessAA;
      static const WireframeOverlayPayloadFactory wireframeOverlay;
      static const DepthStencilCompositePayloadFactory depthStencilComposite;
      static const FeaturePassPayloadFactory<CurveOverlayPass> curveOverlay(
        RenderPassKind::Overlay, RenderExecutorKind::Wireframe, {"curve_overlay"});
      static const std::vector<const BuiltinPassPayloadFactory*> result = {
        &raytraceBeauty,
        &rasterBeauty,
        &wireframeBeauty,
        &tonemap,
        &readback,
        &rasterVisibility,
        &rasterPreviewShadows,
        &depthVisualization,
        &depthAOV,
        &depthAOVRasterizer,
        &depthAOVWireframe,
        &stencilVisualization,
        &stencilAOVRaytracer,
        &stencilAOVRasterizer,
        &stencilAOVWireframe,
        &normalVisualization,
        &normalAOVRaytracer,
        &normalAOVRasterizer,
        &normalAOVWireframe,
        &objectIdVisualization,
        &objectIdAOVRaytracer,
        &objectIdAOVRasterizer,
        &objectIdAOVWireframe,
        &materialIdVisualization,
        &materialIdAOVRaytracer,
        &materialIdAOVRasterizer,
        &materialIdAOVWireframe,
        &worldPositionVisualization,
        &worldPositionAOVRaytracer,
        &worldPositionAOVRasterizer,
        &worldPositionAOVWireframe,
        &rasterCoverageCountVisualization,
        &rasterCoverageCountAOV,
        &rasterDepthTestCountVisualization,
        &rasterDepthTestCountAOV,
        &rasterDepthPassCountVisualization,
        &rasterDepthPassCountAOV,
        &rasterShadeCountVisualization,
        &rasterShadeCountAOV,
        &rasterColorWriteCountVisualization,
        &rasterColorWriteCountAOV,
        &postProcessAA,
        &wireframeOverlay,
        &depthStencilComposite,
        &curveOverlay,
      };
      return result;
    }
  }

  std::unique_ptr<RenderPassPayload> RenderPassPayload::createBuiltin(const RenderPassNode& pass) {
    for (const auto* factory : builtinPayloadFactories()) {
      if (factory->matches(pass)) {
        return factory->create(pass);
      }
    }

    return nullptr;
  }
}
