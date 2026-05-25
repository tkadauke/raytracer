#include "engine/graph/RenderPassPayload.h"

#include "core/Buffer.h"
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
#include "render/tonemap/Tonemap.h"

#include <algorithm>
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
