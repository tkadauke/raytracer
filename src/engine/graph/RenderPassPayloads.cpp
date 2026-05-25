#include "engine/graph/RenderPassPayload.h"

#include "core/Buffer.h"
#include "core/math/HitPointInterval.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderGraphArtifactCache.h"
#include "engine/graph/RenderExecutionContext.h"
#include "engine/graph/RenderResourceStorage.h"
#include "engine/graph/WireframePassState.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/samplers/SampleStream.h"
#include "render/State.h"
#include "render/tonemap/Tonemap.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

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

    void requireObjectIdResource(const RenderResourceStorage& storage,
                                 const RenderResourceId& resource, const RenderPassNode& pass) {
      if (!storage.resource(resource).objectIdBacked()) {
        throw passError(pass, "resource '" + resource + "' is not object-id-backed");
      }
    }

    void requireMatchingSize(const Buffer<Colord>& source, const Buffer<Colord>& destination,
                             const std::string& action) {
      if (source.width() != destination.width() || source.height() != destination.height()) {
        throw std::runtime_error(action + " requires matching color buffer dimensions");
      }
    }

    void copyColorBuffer(const Buffer<Colord>& source, Buffer<Colord>& destination) {
      requireMatchingSize(source, destination, "color copy");
      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          destination[y][x] = source[y][x];
        }
      }
    }

    void requireMatchingSize(const Buffer<double>& source, const Buffer<Colord>& destination,
                             const std::string& action) {
      if (source.width() != destination.width() || source.height() != destination.height()) {
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

    bool hasFeature(const RenderPassNode& pass, const RenderFeatureKind& feature) {
      return std::any_of(pass.features.begin(), pass.features.end(),
                         [&](const RenderFeatureKind& value) { return value == feature; });
    }

    void applyRasterShadowInputs(const RenderExecutionContext& context,
                                 const RasterBeautyPassState& beautyState,
                                 ::engine::raster::Rasterizer& rasterizer) {
      for (const auto& read : context.pass().reads) {
        const auto& resource = context.storage().resource(read.resource);
        if (resource.descriptor().type != RenderResourceType::ShadowMap ||
            resource.substituteDefault()) {
          continue;
        }

        if (auto state = std::dynamic_pointer_cast<const RasterShadowPassState>(resource.state())) {
          state->applyTo(rasterizer);
        } else if (!beautyState.shadows().empty()) {
          beautyState.shadows().applyTo(rasterizer);
          rasterizer.setShadowMapsEnabled(true);
        } else {
          RasterShadowPassState::previewDefaults().applyTo(rasterizer);
        }
      }
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
        return std::make_shared<::engine::raytracer::Raytracer>(std::move(camera), graph.scene());
      }
    };

    /**
      * Whole-frame beauty payload backed by the software rasterizer.
      */
    class RasterBeautyPass : public BeautyPassPayload {
    private:
      std::shared_ptr<render::RenderEngine>
      createEngine(const RenderExecutionContext& context) const override {
        const auto& graph = context.graph();
        auto camera = graph.camera() ? graph.camera()->clone() : nullptr;
        auto rasterizer =
          std::make_shared<::engine::raster::Rasterizer>(std::move(camera), graph.scene());
        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(context.pass());
        state.applyTo(*rasterizer);
        applyRasterShadowInputs(context, state, *rasterizer);
        return rasterizer;
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
        Buffer<double>& depth = context.storage().depth(write.resource);
        if (rasterizer->renderFirstDirectionalShadowMap(depth)) {
          if (cacheable) {
            context.graph().artifactCache()->store(std::make_shared<RenderGraphDepthArtifact>(
              cacheKey, depth, "raster preview directional shadow depth map"));
            resource.setCacheMetadata(
              {RenderGraphCacheStatus::Stored,
               "cache miss; stored raster preview directional shadow depth artifact"});
          }
        } else if (cacheable) {
          resource.setCacheMetadata(
            {RenderGraphCacheStatus::Uncached,
             "raster preview shadow pass did not materialize a cacheable depth artifact"});
        }
      }

    private:
      bool restoreFromCache(RenderExecutionContext& context, const RenderResourceId& resourceId,
                            const RenderGraphCacheKey& cacheKey) const {
        auto artifact = context.graph().artifactCache()->find(cacheKey);
        if (!artifact) {
          return false;
        }

        auto depthArtifact = std::dynamic_pointer_cast<const RenderGraphDepthArtifact>(artifact);
        if (!depthArtifact) {
          context.storage()
            .resource(resourceId)
            .setCacheMetadata({RenderGraphCacheStatus::Invalidated,
                               "cached artifact type did not match the shadow-map depth resource"});
          return false;
        }

        depthArtifact->copyTo(context.storage().depth(resourceId));
        context.storage()
          .resource(resourceId)
          .setCacheMetadata(
            {RenderGraphCacheStatus::Hit,
             "restored raster preview directional shadow depth artifact from cache"});
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
        if (objectIds.width() != color.width() || objectIds.height() != color.height()) {
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

        copyColorBuffer(context.storage().color(read.resource),
                        context.storage().color(write.resource));
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

    /**
      * Overlay payload that draws wireframe edges over an existing color image.
      */
    class WireframeOverlayPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& read = pass.singleRead();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), read.resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<Colord>& source = context.storage().color(read.resource);
        Buffer<Colord>& destination = context.storage().color(write.resource);
        copyColorBuffer(source, destination);

        Buffer<Colord> overlay(source.width(), source.height());
        auto camera = context.graph().camera() ? context.graph().camera()->clone() : nullptr;
        auto wireframe = std::make_shared<::engine::wireframe::Wireframe>(std::move(camera),
                                                                          context.graph().scene());
        WireframePassState::valueFromPass(pass).applyTo(*wireframe);
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
        copyColorBuffer(source, destination);
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
  }

  std::unique_ptr<RenderPassPayload> RenderPassPayload::createBuiltin(const RenderPassNode& pass) {
    if (pass.kind == RenderPassKind::Beauty) {
      switch (pass.executor) {
      case RenderExecutorKind::Raytracer:
        return std::make_unique<RaytraceBeautyPass>();
      case RenderExecutorKind::Rasterizer:
        return std::make_unique<RasterBeautyPass>();
      case RenderExecutorKind::Wireframe:
        return std::make_unique<WireframeBeautyPass>();
      case RenderExecutorKind::Composite:
      case RenderExecutorKind::PostProcess:
        break;
      }
    }

    if (pass.kind == RenderPassKind::Tonemap && pass.executor == RenderExecutorKind::PostProcess) {
      return std::make_unique<TonemapPass>();
    }

    if (pass.kind == RenderPassKind::Shadow && pass.executor == RenderExecutorKind::Rasterizer &&
        hasFeature(pass, "preview_shadows")) {
      return std::make_unique<RasterPreviewShadowPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "depth") &&
        hasFeature(pass, "visualization")) {
      return std::make_unique<DepthVisualizationPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "depth")) {
      return std::make_unique<DepthAOVPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "normal") &&
        hasFeature(pass, "visualization")) {
      return std::make_unique<ColorCopyPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "normal")) {
      return std::make_unique<NormalAOVPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "object_id") &&
        hasFeature(pass, "visualization")) {
      return std::make_unique<ObjectIdVisualizationPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "object_id")) {
      return std::make_unique<ObjectIdAOVPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "material_id") &&
        hasFeature(pass, "visualization")) {
      return std::make_unique<ObjectIdVisualizationPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "material_id")) {
      return std::make_unique<MaterialIdAOVPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "world_position") &&
        hasFeature(pass, "visualization")) {
      return std::make_unique<WorldPositionVisualizationPass>();
    }

    if (pass.kind == RenderPassKind::AOV && hasFeature(pass, "world_position")) {
      return std::make_unique<WorldPositionAOVPass>();
    }

    if (pass.kind == RenderPassKind::PostProcess &&
        pass.executor == RenderExecutorKind::PostProcess) {
      if (auto state = PostProcessAAState::fromPass(pass))
        return std::make_unique<PostProcessAAPass>(std::move(state));
    }

    if (pass.kind == RenderPassKind::Overlay && pass.executor == RenderExecutorKind::Wireframe) {
      return std::make_unique<WireframeOverlayPass>();
    }

    return nullptr;
  }
}
