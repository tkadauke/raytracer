#include "engine/graph/RenderPassPayload.h"

#include "core/Buffer.h"
#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderExecutionContext.h"
#include "engine/graph/RenderResourceStorage.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/Camera.h"
#include "render/tonemap/Tonemap.h"

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

    void applyPreviewShadowPolicy(::engine::raster::Rasterizer& rasterizer) {
      rasterizer.setShadowMapsEnabled(true);
      rasterizer.setShadowMapSize(256);
      rasterizer.setShadowCascadeCount(4);
      rasterizer.setShadowBias(0.1);
      rasterizer.setShadowFilterRadius(1);
      rasterizer.setShadowFilterMode(engine::raster::Rasterizer::ShadowFilterMode::PCF);
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

        auto engine = createEngine(context.graph());
        prepareEngine(*engine, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(engine);
        engine->render(context.storage().color(write.resource));
      }

      bool executeDisplay(RenderExecutionContext& context, Buffer<unsigned int>& buffer,
                          std::shared_ptr<render::Tonemap> tonemap) override {
        auto engine = createEngine(context.graph());
        prepareEngine(*engine, context.graph(), context.cancelled(), std::move(tonemap));
        context.setActiveEngine(engine);
        engine->render(buffer);
        return true;
      }

    private:
      virtual std::shared_ptr<render::RenderEngine>
      createEngine(const GraphRenderEngine& graph) const = 0;
    };

    /**
      * Whole-frame beauty payload backed by the Whitted raytracer.
      */
    class RaytraceBeautyPass : public BeautyPassPayload {
    private:
      std::shared_ptr<render::RenderEngine>
      createEngine(const GraphRenderEngine& graph) const override {
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
      createEngine(const GraphRenderEngine& graph) const override {
        auto camera = graph.camera() ? graph.camera()->clone() : nullptr;
        auto rasterizer =
          std::make_shared<::engine::raster::Rasterizer>(std::move(camera), graph.scene());
        if (graph.intent().enablePreviewShadows) {
          applyPreviewShadowPolicy(*rasterizer);
        }
        return rasterizer;
      }
    };

    /**
      * Whole-frame beauty payload backed by the wireframe renderer.
      */
    class WireframeBeautyPass : public BeautyPassPayload {
    private:
      std::shared_ptr<render::RenderEngine>
      createEngine(const GraphRenderEngine& graph) const override {
        auto camera = graph.camera() ? graph.camera()->clone() : nullptr;
        return std::make_shared<::engine::wireframe::Wireframe>(std::move(camera), graph.scene());
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

    if (pass.kind == RenderPassKind::Overlay && pass.executor == RenderExecutorKind::Wireframe) {
      return std::make_unique<WireframeOverlayPass>();
    }

    return nullptr;
  }
}
