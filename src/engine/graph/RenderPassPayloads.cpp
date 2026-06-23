#include "engine/graph/RenderPassPayload.h"

#include "core/Buffer.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/util/BufferUtils.h"
#include "engine/TracingExecutionCapabilityJson.h"
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
#include "engine/raster/RasterVisibilitySceneCache.h"
#include "engine/raster/RasterVisibilitySet.h"
#include "engine/raster/detail/RasterTriangleEmitter.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wavefront/WavefrontRaytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/Camera.h"
#include "render/GpuDiffusePathLoopBackend.h"
#include "render/GpuDiffusePathStepReference.h"
#include "render/GpuTracingScene.h"
#include "render/HomogeneousClipVolume.h"
#include "render/IntersectionService.h"
#include "render/lights/Light.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/primitives/Scene.h"
#include "render/samplers/SampleStream.h"
#include "render/State.h"
#include "render/textures/Texture.h"
#include "render/tonemap/Tonemap.h"
#include "render/TracingAccumulationLayout.h"
#include "render/viewplanes/ViewPlane.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
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

    void requireMatchingSize(const Buffer<Colord>& source, const Buffer<unsigned int>& destination,
                             const std::string& action) {
      if (!core::util::bufferDimensionsEqual(source, destination)) {
        throw std::runtime_error(action + " requires matching color/display buffer dimensions");
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

      *minColor = Colord(minimum);
      *maxColor = Colord(maximum);
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

    class RenderResourceTexture : public render::Texturec {
    public:
      explicit RenderResourceTexture(const Buffer<Colord>& color)
          : m_color(color) {
      }

      Colord evaluate(const Rayd&, const HitPoint& hitPoint) const override {
        if (m_color.width() <= 0 || m_color.height() <= 0) {
          return Colord::black();
        }

        const Vector2d& uv = hitPoint.uv();
        const double u = std::clamp(uv.x(), 0.0, 1.0);
        const double v = std::clamp(uv.y(), 0.0, 1.0);
        const int x =
          std::clamp(static_cast<int>(std::floor(u * m_color.width())), 0, m_color.width() - 1);
        const int y =
          std::clamp(static_cast<int>(std::floor(v * m_color.height())), 0, m_color.height() - 1);
        return m_color[y][x];
      }

    private:
      const Buffer<Colord>& m_color;
    };

    std::optional<std::string> subviewNameForColorResource(const RenderResourceDescriptor& desc) {
      if (desc.type != RenderResourceType::Color || !desc.hasFeature("subview_color_output")) {
        return std::nullopt;
      }
      const std::string prefix = "subview_name:";
      for (const auto& feature : desc.features) {
        if (feature.rfind(prefix, 0) == 0) {
          return feature.substr(prefix.size());
        }
      }
      return std::nullopt;
    }

    std::map<std::string, std::shared_ptr<render::Texturec>>
    renderTextureInputsForPass(const RenderExecutionContext& context) {
      std::map<std::string, std::shared_ptr<render::Texturec>> result;
      for (const auto& read : context.pass().reads) {
        const auto& resource = context.storage().resource(read.resource);
        const auto name = subviewNameForColorResource(resource.descriptor());
        if (!name || resource.substituteDefault()) {
          continue;
        }
        if (!resource.colorBacked()) {
          throw passError(context.pass(),
                          "render-to-texture input '" + read.resource + "' is not color-backed");
        }
        result[*name] = std::make_shared<RenderResourceTexture>(resource.color());
      }
      return result;
    }

    std::shared_ptr<render::Material>
    materialWithRenderTexture(std::shared_ptr<render::Material> material,
                              std::shared_ptr<render::Texturec> texture,
                              const std::string& receiverName) {
      if (!texture) {
        return material;
      }

      auto matte = std::dynamic_pointer_cast<render::MatteMaterial>(material);
      if (!matte) {
        auto result = std::make_shared<render::MatteMaterial>(texture);
        if (material) {
          result->setSidedness(material->sidedness());
        }
        result->setRenderTextureSubview(receiverName);
        return result;
      }

      auto phong = std::dynamic_pointer_cast<render::PhongMaterial>(material);
      if (phong) {
        auto result = std::make_shared<render::PhongMaterial>(texture, phong->specularColor(),
                                                              phong->exponent());
        result->setAmbientCoefficient(phong->ambientCoefficient());
        result->setDiffuseCoefficient(phong->diffuseCoefficient());
        result->setSpecularCoefficient(phong->specularCoefficient());
        result->setNormalTexture(phong->normalTexture());
        result->setSidedness(phong->sidedness());
        result->setRenderTextureSubview(receiverName);
        return result;
      }

      auto result = std::make_shared<render::MatteMaterial>(texture);
      result->setAmbientCoefficient(matte->ambientCoefficient());
      result->setDiffuseCoefficient(matte->diffuseCoefficient());
      result->setNormalTexture(matte->normalTexture());
      result->setSidedness(matte->sidedness());
      result->setRenderTextureSubview(receiverName);
      return result;
    }

    class ScopedRenderTextureMaterialBindings {
    public:
      explicit ScopedRenderTextureMaterialBindings(RenderExecutionContext& context)
          : m_textures(renderTextureInputsForPass(context)) {
        if (m_textures.empty()) {
          return;
        }

        auto scene = context.graph().scene();
        if (!scene) {
          return;
        }

        static_cast<const render::Primitive&>(*scene).forEachTransformedLeaf(
          [&](const render::Primitive::TransformedLeaf& leaf) {
            if (!leaf.primitive) {
              return;
            }

            const std::string receiver = receiverNameForLeaf(leaf);
            if (receiver.empty()) {
              return;
            }

            const auto texture = m_textures.find(receiver);
            if (texture == m_textures.end()) {
              return;
            }

            auto* primitive = const_cast<render::Primitive*>(leaf.primitive);
            m_originalMaterials.push_back({primitive, primitive->material()});
            primitive->setMaterial(
              materialWithRenderTexture(leaf.material, texture->second, receiver));
            ++m_boundCount;
          });

        if (m_boundCount > 0) {
          context.recordTraceMessage("bound " + std::to_string(m_boundCount) +
                                     " render-to-texture receiver material(s)");
        }
      }

      ScopedRenderTextureMaterialBindings(const ScopedRenderTextureMaterialBindings&) = delete;
      ScopedRenderTextureMaterialBindings&
      operator=(const ScopedRenderTextureMaterialBindings&) = delete;

      ~ScopedRenderTextureMaterialBindings() {
        for (auto it = m_originalMaterials.rbegin(); it != m_originalMaterials.rend(); ++it) {
          it->primitive->setMaterial(std::move(it->material));
        }
      }

      bool hasInputs() const {
        return !m_textures.empty();
      }

      bool boundAnyReceiver() const {
        return m_boundCount > 0;
      }

    private:
      struct OriginalMaterial {
        render::Primitive* primitive;
        std::shared_ptr<render::Material> material;
      };

      static std::string receiverNameForLeaf(const render::Primitive::TransformedLeaf& leaf) {
        if (!leaf.primitive->renderTextureSubview().empty()) {
          return leaf.primitive->renderTextureSubview();
        }
        if (leaf.material && !leaf.material->renderTextureSubview().empty()) {
          return leaf.material->renderTextureSubview();
        }
        return {};
      }

      std::map<std::string, std::shared_ptr<render::Texturec>> m_textures;
      std::vector<OriginalMaterial> m_originalMaterials;
      std::size_t m_boundCount{0};
    };

    ScopedRenderTextureMaterialBindings
    bindRenderTextureMaterials(RenderExecutionContext& context) {
      return ScopedRenderTextureMaterialBindings(context);
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
      engine.setProgressiveDisplayEnabled(graph.progressiveDisplayEnabled());
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

        auto renderTextureBindings = bindRenderTextureMaterials(context);
        auto engine = createEngine(context);
        prepareEngine(*engine, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(engine);
        engine->render(context.storage().color(write.resource));
      }

      bool executeDisplay(RenderExecutionContext& context, Buffer<unsigned int>& buffer,
                          std::shared_ptr<render::Tonemap> tonemap) override {
        auto renderTextureBindings = bindRenderTextureMaterials(context);
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

    class OpenGLRasterTraceMessageRecorder {
    protected:
      void recordOpenGLRasterTraceMessages(
        RenderExecutionContext& context,
        const std::shared_ptr<::engine::raster::OpenGLRasterizer>& rasterizer) const {
        for (const auto& message : rasterizer->traceMessages()) {
          context.recordTraceMessage(message);
        }
      }
    };

    bool executionPathUsesGpu(const QString& path) {
      return path == QStringLiteral("gpu") || path.startsWith(QStringLiteral("metal")) ||
             path.startsWith(QStringLiteral("vulkan")) ||
             path.startsWith(QStringLiteral("full_gpu"));
    }

    QString actualTracingExecutionFromWavefrontMetrics(const QJsonObject& metrics) {
      const QJsonObject batching = metrics.value("batching").toObject();
      const bool closestUsesGpu = executionPathUsesGpu(
        batching.value("intersectionBackendClosestHitExecutionPath").toString());
      const bool anyUsesGpu =
        executionPathUsesGpu(batching.value("intersectionBackendAnyHitExecutionPath").toString());
      const bool combinedUsesGpu =
        executionPathUsesGpu(batching.value("intersectionBackendExecutionPath").toString());
      return closestUsesGpu || anyUsesGpu || combinedUsesGpu ? QStringLiteral("hybrid")
                                                             : QStringLiteral("cpu");
    }

    QString actualTracingFallbackReasonFromWavefrontMetrics(const QJsonObject& metrics) {
      const QJsonObject fallback =
        metrics.value("batching").toObject().value("tracingBackendFallback").toObject();
      return fallback.value("active").toBool() ? fallback.value("reason").toString() : QString();
    }

    QJsonObject tracingExecutionMetadata(const RenderPassNode& pass, QString actualExecution,
                                         QString actualFallbackReason = {}) {
      QJsonObject metadata;
      const RaytracerBeautyPassState state = RaytracerBeautyPassState::valueFromPass(pass);
      const auto requested = state.tracingExecution().value_or(TracingExecutionPreference::Auto);
      metadata["requestedMode"] = tracingExecutionPreferenceName(requested);
      if (state.predictedTracingExecution()) {
        metadata["predictedMode"] =
          tracingExecutionPreferenceName(*state.predictedTracingExecution());
      }
      metadata["actualMode"] = std::move(actualExecution);
      metadata["fallbackReason"] = QString::fromStdString(state.tracingExecutionFallbackReason());
      metadata["actualFallbackReason"] = std::move(actualFallbackReason);
      return metadata;
    }

    QJsonObject withTracingExecutionMetadata(QJsonObject metadata, const RenderPassNode& pass,
                                             QString actualExecution,
                                             QString actualFallbackReason = {}) {
      metadata["tracingExecution"] =
        tracingExecutionMetadata(pass, std::move(actualExecution), std::move(actualFallbackReason));
      return metadata;
    }

    QJsonArray unsignedArrayToJson(const std::vector<std::uint64_t>& values) {
      QJsonArray result;
      for (const std::uint64_t value : values) {
        result.push_back(static_cast<double>(value));
      }
      return result;
    }

    QJsonObject
    accumulationDiagnosticsToJson(const render::TracingAccumulationDiagnostics& diagnostics) {
      QJsonObject layout;
      layout["width"] = diagnostics.layout.width;
      layout["height"] = diagnostics.layout.height;
      layout["pixelCount"] = static_cast<double>(diagnostics.layout.pixelCount());
      layout["colorSumFormat"] = render::toString(diagnostics.layout.colorSumFormat);
      layout["sampleCountFormat"] = render::toString(diagnostics.layout.sampleCountFormat);
      layout["momentFormat"] = render::toString(diagnostics.layout.momentFormat);
      layout["resolveFormat"] = render::toString(diagnostics.layout.resolveFormat);
      layout["colorSumBytes"] = static_cast<double>(diagnostics.layout.colorSumBytes());
      layout["sampleCountBytes"] = static_cast<double>(diagnostics.layout.sampleCountBytes());
      layout["momentBytes"] = static_cast<double>(diagnostics.layout.momentBytes());
      layout["resolveBytes"] = static_cast<double>(diagnostics.layout.resolveBytes());
      layout["accumulationBytes"] = static_cast<double>(diagnostics.layout.accumulationBytes());
      layout["totalBytes"] = static_cast<double>(diagnostics.layout.totalBytes());

      QJsonObject result;
      result["backend"] = QString::fromStdString(diagnostics.backend);
      result["residency"] = QString::fromStdString(diagnostics.residency);
      result["layout"] = layout;
      result["residentBytes"] = static_cast<double>(diagnostics.residentBytes);
      result["clearOperations"] = static_cast<double>(diagnostics.clearOperations);
      result["addOperations"] = static_cast<double>(diagnostics.addOperations);
      result["addedSamples"] = static_cast<double>(diagnostics.addedSamples);
      result["resolveOperations"] = static_cast<double>(diagnostics.resolveOperations);
      result["readbackOperations"] = static_cast<double>(diagnostics.readbackOperations);
      result["readbackBytes"] = static_cast<double>(diagnostics.readbackBytes);
      return result;
    }

    QJsonObject
    gpuTracingSceneDiagnosticsToJson(const render::GpuTracingSceneDiagnostics& diagnostics) {
      QJsonObject result;
      result["compiled"] = diagnostics.compiled;
      result["materials"] = static_cast<double>(diagnostics.materials);
      result["textures"] = static_cast<double>(diagnostics.textures);
      result["lights"] = static_cast<double>(diagnostics.lights);
      result["environment"] = static_cast<double>(diagnostics.environment);
      result["debugIds"] = static_cast<double>(diagnostics.debugIds);
      result["unsupportedPrimitives"] = static_cast<double>(diagnostics.unsupportedPrimitives);
      result["unsupportedMaterials"] = static_cast<double>(diagnostics.unsupportedMaterials);
      result["unsupportedTextures"] = static_cast<double>(diagnostics.unsupportedTextures);
      result["unsupportedLights"] = static_cast<double>(diagnostics.unsupportedLights);
      result["uploadBytes"] = static_cast<double>(diagnostics.uploadBytes);
      return result;
    }

    QJsonObject integerObject(const std::map<std::string, std::uint64_t>& values) {
      QJsonObject result;
      for (const auto& [key, count] : values) {
        result[QString::fromStdString(key)] = static_cast<double>(count);
      }
      return result;
    }

    void addIntersectionServiceSceneDiagnostics(
      QJsonObject& service, const render::WavefrontIntersectionSceneDiagnostics& diagnostics) {
      const std::uint64_t supportedPrimitives =
        diagnostics.primitives >= diagnostics.unsupportedPrimitives
          ? diagnostics.primitives - diagnostics.unsupportedPrimitives
          : 0u;

      service["compiledScene"] = diagnostics.compiled;
      service["scenePrimitives"] = static_cast<double>(diagnostics.primitives);
      service["sceneSupportedPrimitives"] = static_cast<double>(supportedPrimitives);
      service["sceneUnsupportedPrimitives"] =
        static_cast<double>(diagnostics.unsupportedPrimitives);
      service["sceneUnsupportedReasons"] = integerObject(diagnostics.unsupportedReasons);
      service["sceneUploadBytes"] = static_cast<double>(diagnostics.uploadBytes);
    }

    constexpr const char* compiledDiffusePathLoopDirectLightContributionFallbackReason() {
      return "compiled CPU-reference path loop evaluates direct-light contribution on the host";
    }

    constexpr const char* compiledDiffusePathLoopResidentDirectLightUnavailableReason() {
      return "compiled CPU-reference path loop resolves direct-light visibility on the host";
    }

    QJsonObject
    compiledDiffusePathLoopMetadata(const render::GpuTracingSceneCompilation& compilation,
                                    const render::GpuDiffusePrimaryPathStateGeneration& generation,
                                    const render::GpuDiffusePathLoopResult& loop,
                                    const render::TracingAccumulationDiagnostics& accumulation,
                                    TracingExecutionPreference requestedTracingExecution) {
      QJsonObject input;
      input["primarySamples"] = static_cast<double>(generation.generatedPrimarySamples);
      input["skippedPrimarySamples"] = static_cast<double>(generation.skippedPrimarySamples);
      input["sampleStreamMode"] = QStringLiteral("gpu_sample_stream");
      input["requestedX"] = generation.requestedRect.x();
      input["requestedY"] = generation.requestedRect.y();
      input["requestedWidth"] = generation.requestedRect.width();
      input["requestedHeight"] = generation.requestedRect.height();
      input["actualX"] = generation.actualRect.x();
      input["actualY"] = generation.actualRect.y();
      input["actualWidth"] = generation.actualRect.width();
      input["actualHeight"] = generation.actualRect.height();

      QJsonObject batching;
      const bool frontierCompactionUsesGpu =
        loop.fullGpuPathLoopSupported() ||
        executionPathUsesGpu(QString::fromStdString(loop.frontierCompactionExecutionPath));
      batching["integrator"] = QStringLiteral("pathtracer");
      batching["executionMode"] = QStringLiteral("compiled_diffuse_path_loop");
      batching["tracingBackendMode"] = QString::fromStdString(loop.executionPath);
      batching["tracingBackend"] =
        loop.fullGpuPathLoopSupported() ? QStringLiteral("gpu") : QStringLiteral("cpu");
      batching["tracingBackendPlatform"] = QString::fromStdString(loop.platformLabel());
      batching["tracingBackendRequest"] = tracingExecutionPreferenceName(requestedTracingExecution);
      const render::TracingExecutionCapabilityRecords tracingCapabilities =
        loop.tracingCapabilities(accumulation);
      batching["tracingBackendCapabilities"] = tracingCapabilitiesToJson(tracingCapabilities);
      batching["tracingBackendFallback"] = tracingBackendFallbackToJson(tracingCapabilities);
      batching["closestHitExecutionPath"] =
        QString::fromStdString(loop.metrics.closestHitExecutionPath);
      batching["emissionExecutionPath"] =
        QString::fromStdString(loop.metrics.emissionExecutionPath);
      batching["directLightVisibilityExecutionPath"] =
        QString::fromStdString(loop.metrics.directLightVisibilityExecutionPath);
      batching["directLightContributionExecutionPath"] =
        QString::fromStdString(loop.metrics.directLightContributionExecutionPath);
      batching["directLightContributionFallbackReason"] =
        loop.fullGpuPathLoopSupported()
          ? QString()
          : QString::fromLatin1(compiledDiffusePathLoopDirectLightContributionFallbackReason());
      batching["intersectionBackendExecutionPath"] =
        QString::fromStdString(loop.metrics.closestHitExecutionPath);
      batching["intersectionBackendClosestHitExecutionPath"] =
        QString::fromStdString(loop.metrics.closestHitExecutionPath);
      batching["intersectionBackendAnyHitExecutionPath"] =
        QString::fromStdString(loop.metrics.directLightVisibilityExecutionPath);
      batching["intersectionBackendSupportsResidentFrontiers"] = loop.fullGpuPathLoopSupported();
      batching["intersectionBackendSupportsGpuFrontierCompaction"] = frontierCompactionUsesGpu;
      batching["intersectionBackendGpuFrontierCompactionUnavailableReason"] =
        frontierCompactionUsesGpu
          ? QString()
          : QStringLiteral("compiled CPU-reference path loop compacts path state on the host");
      batching["intersectionBackendSupportsPreparedRayBatchCompaction"] =
        loop.fullGpuPathLoopSupported();
      batching["intersectionBackendSupportsResidentDirectLightBatches"] =
        loop.fullGpuPathLoopSupported();
      batching["intersectionBackendResidentDirectLightBatchesUnavailableReason"] =
        loop.fullGpuPathLoopSupported()
          ? QString()
          : QString::fromLatin1(compiledDiffusePathLoopResidentDirectLightUnavailableReason());
      batching["initialPathCount"] = static_cast<double>(loop.initialPathCount);
      batching["depthCount"] = static_cast<double>(loop.depthCount);
      batching["maxDepthTerminatedPaths"] = static_cast<double>(loop.maxDepthTerminatedPaths);
      batching["activePathsPerDepth"] = unsignedArrayToJson(loop.activePathsPerDepth);
      batching["resolvedPathStates"] = static_cast<double>(loop.removedPathCount());
      batching["stepRecords"] = static_cast<double>(loop.stepRecords.size());
      batching["activePaths"] = static_cast<double>(loop.inputPathCount());
      batching["closestHitRays"] = static_cast<double>(loop.metrics.closestHitRays);
      batching["misses"] = static_cast<double>(loop.metrics.misses);
      batching["hits"] = static_cast<double>(loop.metrics.hits);
      batching["unsupportedHits"] = static_cast<double>(loop.metrics.unsupportedHits);
      batching["emissiveHits"] = static_cast<double>(loop.metrics.emissiveHits);
      batching["emissionContributionEvaluations"] =
        static_cast<double>(loop.metrics.emissionContributionEvaluations);
      batching["directLightSamples"] = static_cast<double>(loop.metrics.directLightSamples);
      batching["directLightVisibilityRays"] =
        static_cast<double>(loop.metrics.directLightVisibilityRays);
      batching["directLightContributionEvaluations"] =
        static_cast<double>(loop.metrics.directLightContributionEvaluations);
      batching["directLightContributingSamples"] =
        static_cast<double>(loop.metrics.directLightContributingSamples);
      batching["directLightOccludedSamples"] =
        static_cast<double>(loop.metrics.directLightOccludedSamples);
      batching["spawnedContinuations"] = static_cast<double>(loop.retainedPathCount());
      batching["terminatedPaths"] = static_cast<double>(loop.metrics.terminatedPaths);
      batching["frontierCompactionExecutionPath"] =
        QString::fromStdString(loop.frontierCompactionExecutionPath);
      batching["frontierCompactionPathStateResidency"] =
        QString::fromStdString(loop.frontierCompactionPathStateResidency);
      batching["frontierCompactionPasses"] = static_cast<double>(loop.compactionPassCount());
      batching["frontierCompactionInputSamples"] = static_cast<double>(loop.inputPathCount());
      batching["frontierCompactionRetainedSamples"] = static_cast<double>(loop.retainedPathCount());
      batching["frontierCompactionRemovedSamples"] = static_cast<double>(loop.removedPathCount());
      batching["frontierCompactionRemovedSampleFraction"] = loop.removedPathFraction();
      batching["frontierCompactionMovedSamples"] = static_cast<double>(loop.movedPathCount());
      batching["frontierCompactionMovedRetainedSampleFraction"] = loop.movedRetainedPathFraction();
      batching["frontierCompactionRetainedIndexBytes"] =
        static_cast<double>(loop.retainedPathIndexBytes());
      batching["frontierCompactionInputHostPathStateBytes"] =
        static_cast<double>(loop.inputPathStateBytes());
      batching["frontierCompactionRetainedHostPathStateBytes"] =
        static_cast<double>(loop.retainedPathStateBytes());
      batching["frontierCompactionRemovedHostPathStateBytes"] =
        static_cast<double>(loop.removedPathStateBytes());
      batching["frontierCompactionUploadWorkerSeconds"] =
        loop.frontierCompactionUploadWorkerSeconds;
      batching["frontierCompactionKernelWorkerSeconds"] =
        loop.frontierCompactionKernelWorkerSeconds;
      batching["frontierCompactionReadbackWorkerSeconds"] =
        loop.frontierCompactionReadbackWorkerSeconds;
      batching["residentPathLoopExecutionPath"] = QString::fromStdString(loop.executionPath);
      batching["residentPathLoopResidency"] = QString::fromStdString(loop.pathStateResidency);
      batching["residentPathLoopPlatformName"] = QString::fromStdString(loop.platformLabel());
      batching["residentPathLoopDepths"] = static_cast<double>(loop.depthCount);
      batching["residentPathLoopInputPaths"] = static_cast<double>(loop.inputPathCount());
      batching["residentPathLoopRetainedPaths"] = static_cast<double>(loop.retainedPathCount());
      batching["residentPathLoopRemovedPaths"] = static_cast<double>(loop.removedPathCount());
      batching["residentPathLoopMovedPaths"] = static_cast<double>(loop.movedPathCount());
      batching["residentPathLoopRetainedIndexBytes"] =
        static_cast<double>(loop.retainedPathIndexBytes());
      batching["residentPathLoopResidentPathStateBytes"] =
        static_cast<double>(loop.residentPathStateBytes());
      batching["residentPathLoopInputResidentPathStateBytes"] =
        static_cast<double>(loop.inputPathStateBytes());
      batching["residentPathLoopRetainedResidentPathStateBytes"] =
        static_cast<double>(loop.retainedPathStateBytes());
      batching["residentPathLoopRemovedResidentPathStateBytes"] =
        static_cast<double>(loop.removedPathStateBytes());
      batching["residentPathLoopCompactionPasses"] =
        static_cast<double>(loop.compactionPassCount());
      batching["residentPathLoopRoundTrips"] = static_cast<double>(loop.roundTrips);
      batching["residentPathLoopSavedHostReadbacks"] = static_cast<double>(loop.savedHostReadbacks);
      batching["residentPathLoopSavedHostReadbackBytes"] =
        static_cast<double>(loop.savedHostReadbackBytes);
      batching["residentPathLoopSubmittedIntersectionRays"] =
        static_cast<double>(loop.submittedIntersectionRayCount());
      batching["residentPathLoopFullPlatformGpuKernel"] = loop.fullGpuPathLoopSupported();
      batching["tracingSceneCompiled"] = compilation.diagnostics.compiled;
      batching["tracingSceneMaterials"] = static_cast<double>(compilation.diagnostics.materials);
      batching["tracingSceneTextures"] = static_cast<double>(compilation.diagnostics.textures);
      batching["tracingSceneLights"] = static_cast<double>(compilation.diagnostics.lights);
      batching["tracingSceneEnvironment"] =
        static_cast<double>(compilation.diagnostics.environment);
      batching["tracingSceneDebugIds"] = static_cast<double>(compilation.diagnostics.debugIds);
      batching["tracingSceneUnsupportedMaterials"] =
        static_cast<double>(compilation.diagnostics.unsupportedMaterials);
      batching["tracingSceneUnsupportedTextures"] =
        static_cast<double>(compilation.diagnostics.unsupportedTextures);
      batching["tracingSceneUnsupportedLights"] =
        static_cast<double>(compilation.diagnostics.unsupportedLights);
      batching["tracingSceneUnsupportedMaterialReasons"] =
        integerObject(compilation.diagnostics.unsupportedMaterialReasons);
      batching["tracingSceneUnsupportedTextureReasons"] =
        integerObject(compilation.diagnostics.unsupportedTextureReasons);
      batching["tracingSceneUnsupportedLightReasons"] =
        integerObject(compilation.diagnostics.unsupportedLightReasons);
      batching["tracingSceneUploadBytes"] =
        static_cast<double>(compilation.diagnostics.uploadBytes);

      QJsonObject compiledLoop;
      compiledLoop["backend"] = QString::fromStdString(loop.executionPath);
      compiledLoop["residency"] = QString::fromStdString(loop.pathStateResidency);
      compiledLoop["platformName"] = QString::fromStdString(loop.platformLabel());
      compiledLoop["fullPlatformGpuKernel"] = loop.fullGpuPathLoopSupported();
      compiledLoop["submittedIntersectionRays"] =
        static_cast<double>(loop.submittedIntersectionRayCount());
      compiledLoop["note"] =
        loop.fullGpuPathLoopSupported()
          ? QStringLiteral("executes the GPU tracing record contract through a platform path-loop")
          : QStringLiteral(
              "executes the GPU tracing record contract through the CPU reference path-loop");

      QJsonObject metadata;
      metadata["input"] = input;
      metadata["batching"] = batching;
      metadata["compiledTracingScene"] = gpuTracingSceneDiagnosticsToJson(compilation.diagnostics);
      metadata["compiledDiffusePathLoop"] = compiledLoop;
      metadata["accumulation"] = accumulationDiagnosticsToJson(accumulation);
      return metadata;
    }

    bool predictedGpuTracing(const RaytracerBeautyPassState& state) {
      return state.predictedTracingExecution() &&
             *state.predictedTracingExecution() == TracingExecutionPreference::GPU;
    }

    std::optional<std::string>
    compiledDiffusePathLoopFallbackReason(const RaytracerBeautyPassState& state,
                                          const GraphRenderEngine& graph) {
      if (!predictedGpuTracing(state)) {
        return "compiled diffuse path loop requires predicted GPU tracing execution";
      }
      if (state.integrator().value_or("whitted") != "pathtracer") {
        return "compiled diffuse path loop currently supports only the pathtracer integrator";
      }
      if (graph.hasBackgroundColorOverride()) {
        return "compiled diffuse path loop cannot apply graph background-color overrides yet";
      }
      if (state.sampleStreamMode() && *state.sampleStreamMode() != "gpu_sample_stream") {
        return "compiled diffuse path loop requires the GPU sample stream";
      }
      if (state.convergenceEnabled().value_or(false)) {
        return "compiled diffuse path loop does not support wavefront convergence yet";
      }
      if (state.adaptiveSamplingEnabled().value_or(false)) {
        return "compiled diffuse path loop does not support adaptive sampling yet";
      }
      if (state.denoiser() && *state.denoiser() != "none") {
        return "compiled diffuse path loop does not support denoising yet";
      }
      if (state.denoiseRadius() || state.denoiseColorSigma()) {
        return "compiled diffuse path loop does not support denoising yet";
      }
      return std::nullopt;
    }

    void packColorBuffer(const Buffer<Colord>& source, Buffer<unsigned int>& destination,
                         const std::shared_ptr<render::Tonemap>& tonemap) {
      requireMatchingSize(source, destination, "compiled diffuse path-loop color pack");
      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          destination[y][x] = (tonemap ? tonemap->apply(source[y][x]) : source[y][x]).rgb();
        }
      }
    }

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

        auto renderTextureBindings = bindRenderTextureMaterials(context);
        auto raytracer =
          std::static_pointer_cast<::engine::raytracer::Raytracer>(createEngine(context));
        prepareEngine(*raytracer, context.graph(), context.cancelled(), std::move(tonemap));
        context.setActiveEngine(raytracer);
        raytracer->render(context.storage().color(write.resource), buffer, raytracer->tonemap());
        context.setTraceMetadata(
          withTracingExecutionMetadata(QJsonObject(), pass, QStringLiteral("cpu")));
        return true;
      }

    private:
      std::shared_ptr<render::RenderEngine>
      createEngine(const RenderExecutionContext& context) const override {
        const auto& graph = context.graph();
        auto camera = context.camera() ? context.camera()->clone() : nullptr;
        auto raytracer =
          std::make_shared<::engine::raytracer::Raytracer>(std::move(camera), graph.scene());
        RaytracerBeautyPassState::valueFromPass(context.pass()).applyTo(*raytracer);
        return raytracer;
      }
    };

    class WavefrontPassSupport {
    protected:
      std::shared_ptr<::engine::wavefront::WavefrontRaytracer>
      createWavefront(const RenderExecutionContext& context) const {
        const auto& graph = context.graph();
        auto camera = context.camera() ? context.camera()->clone() : nullptr;
        auto wavefront = std::make_shared<::engine::wavefront::WavefrontRaytracer>(
          std::move(camera), graph.scene());
        wavefront->setMetricsEnabled(graph.executionTraceEnabled());
        RaytracerBeautyPassState::valueFromPass(context.pass()).applyTo(*wavefront);
        return wavefront;
      }

      void recordWavefrontMetrics(RenderExecutionContext& context,
                                  const ::engine::wavefront::WavefrontRaytracer& wavefront,
                                  const std::string& actualFallbackReason = {}) const {
        QJsonObject metrics = wavefront.lastMetrics().toJson();
        const QString fallback = actualFallbackReason.empty()
                                   ? actualTracingFallbackReasonFromWavefrontMetrics(metrics)
                                   : QString::fromStdString(actualFallbackReason);
        context.setTraceMetadata(withTracingExecutionMetadata(
          metrics, context.pass(), actualTracingExecutionFromWavefrontMetrics(metrics), fallback));
      }
    };

    /**
      * Whole-frame beauty payload backed by the wavefront ray executor.
      */
    class WavefrontBeautyPass : public BeautyPassPayload, private WavefrontPassSupport {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        auto renderTextureBindings = bindRenderTextureMaterials(context);
        auto wavefront = createWavefront(context);
        prepareEngine(*wavefront, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(wavefront);
        std::string compiledFallbackReason;
        if (tryExecuteCompiledDiffusePathLoop(context, *wavefront,
                                              &context.storage().color(write.resource), nullptr,
                                              compiledFallbackReason)) {
          return;
        }
        recordCompiledDiffusePathLoopFallback(context, compiledFallbackReason);
        wavefront->render(context.storage().color(write.resource));
        recordWavefrontMetrics(context, *wavefront, compiledFallbackReason);
      }

      bool executeDisplay(RenderExecutionContext& context, Buffer<unsigned int>& buffer,
                          std::shared_ptr<render::Tonemap> tonemap) override {
        auto renderTextureBindings = bindRenderTextureMaterials(context);
        auto wavefront = createWavefront(context);
        prepareEngine(*wavefront, context.graph(), context.cancelled(), std::move(tonemap));
        context.setActiveEngine(wavefront);
        std::string compiledFallbackReason;
        if (tryExecuteCompiledDiffusePathLoop(context, *wavefront, nullptr, &buffer,
                                              compiledFallbackReason)) {
          return true;
        }
        recordCompiledDiffusePathLoopFallback(context, compiledFallbackReason);
        wavefront->render(buffer);
        recordWavefrontMetrics(context, *wavefront, compiledFallbackReason);
        return true;
      }

      bool executeDisplayAndStore(RenderExecutionContext& context, Buffer<unsigned int>& buffer,
                                  std::shared_ptr<render::Tonemap> tonemap) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        auto renderTextureBindings = bindRenderTextureMaterials(context);
        auto wavefront = createWavefront(context);
        prepareEngine(*wavefront, context.graph(), context.cancelled(), std::move(tonemap));
        context.setActiveEngine(wavefront);
        std::string compiledFallbackReason;
        if (tryExecuteCompiledDiffusePathLoop(context, *wavefront,
                                              &context.storage().color(write.resource), &buffer,
                                              compiledFallbackReason)) {
          return true;
        }
        recordCompiledDiffusePathLoopFallback(context, compiledFallbackReason);
        wavefront->render(context.storage().color(write.resource), buffer, wavefront->tonemap());
        recordWavefrontMetrics(context, *wavefront, compiledFallbackReason);
        return true;
      }

    private:
      bool tryExecuteCompiledDiffusePathLoop(RenderExecutionContext& context,
                                             ::engine::wavefront::WavefrontRaytracer& wavefront,
                                             Buffer<Colord>* hdrTarget,
                                             Buffer<unsigned int>* displayTarget,
                                             std::string& fallbackReason) const {
        const RaytracerBeautyPassState state =
          RaytracerBeautyPassState::valueFromPass(context.pass());
        if (!predictedGpuTracing(state)) {
          return false;
        }

        if (const auto reason = compiledDiffusePathLoopFallbackReason(state, context.graph())) {
          fallbackReason = *reason;
          return false;
        }
        if (context.cancelled()) {
          fallbackReason = "compiled diffuse path loop skipped because rendering was cancelled";
          return false;
        }

        auto scene = context.graph().scene();
        auto camera = wavefront.camera();
        if (!scene) {
          fallbackReason = "compiled diffuse path loop requires a render scene";
          return false;
        }
        if (!camera) {
          fallbackReason = "compiled diffuse path loop requires a camera";
          return false;
        }
        if (!camera->viewPlane()) {
          fallbackReason = "compiled diffuse path loop requires a camera view plane";
          return false;
        }

        const int width = hdrTarget ? hdrTarget->width() : displayTarget->width();
        const int height = hdrTarget ? hdrTarget->height() : displayTarget->height();
        if (displayTarget &&
            (displayTarget->width() != width || displayTarget->height() != height)) {
          fallbackReason =
            "compiled diffuse path loop requires matching HDR and display target dimensions";
          return false;
        }

        const render::GpuTracingSceneCompilation compilation =
          render::compileGpuTracingScene(*scene);
        const render::GpuDiffusePathLoopSupport support =
          render::gpuDiffusePathLoopSupport(compilation, *scene);
        if (!support.supported) {
          fallbackReason = support.reason;
          return false;
        }

        const Recti targetRect(width, height);
        camera->viewPlane()->setup(camera->matrix(), targetRect);
        const std::optional<std::uint64_t> samplingSeed = wavefront.samplingSeed();
        const std::uint64_t seedValue = samplingSeed.value_or(0);
        const std::uint32_t sampleSeed = static_cast<std::uint32_t>(seedValue ^ (seedValue >> 32u));
        const render::GpuDiffusePrimaryPathStateGeneration generation =
          render::GpuDiffusePrimaryPathStateGenerator().generate(*camera, targetRect, samplingSeed,
                                                                 sampleSeed);

        render::GpuDiffusePathLoopSettings settings;
        settings.maxDepth = static_cast<std::uint32_t>(state.maximumRecursionDepth().value_or(8));
        settings.russianRouletteDepth =
          static_cast<std::uint32_t>(state.russianRouletteDepth().value_or(3));
        settings.directLightSamples =
          static_cast<std::uint32_t>(std::max(1, state.directLightSamples().value_or(1)));
        const std::shared_ptr<const render::GpuDiffusePathLoopBackend> pathLoopBackend =
          context.graph().gpuDiffusePathLoopBackend();
        if (!pathLoopBackend) {
          fallbackReason = "compiled diffuse path loop requires an execution backend";
          return false;
        }
        const render::GpuDiffusePathLoopResult loop =
          pathLoopBackend->run(compilation.sections, generation.pathStates, settings);
        const render::TracingAccumulationLayout layout =
          render::TracingAccumulationLayout::image(width, height);

        render::TracingAccumulationDiagnostics accumulation;
        if (hdrTarget) {
          accumulation = render::resolveGpuDiffusePathLoopImage(loop, layout, *hdrTarget);
          if (displayTarget) {
            packColorBuffer(*hdrTarget, *displayTarget, wavefront.tonemap());
          }
        } else {
          accumulation = render::resolveGpuDiffusePathLoopImage(loop, layout, *displayTarget,
                                                                wavefront.tonemap().get());
        }

        const bool frontierCompactionUsesGpu =
          executionPathUsesGpu(QString::fromStdString(loop.frontierCompactionExecutionPath));
        const QString actualTracingExecution =
          loop.fullGpuPathLoopSupported()
            ? QStringLiteral("gpu")
            : (frontierCompactionUsesGpu ? QStringLiteral("hybrid") : QStringLiteral("cpu"));
        const QString actualTracingFallback =
          loop.fullGpuPathLoopSupported()
            ? QString()
            : QStringLiteral("GPU tracing request executed by compiled CPU-reference diffuse "
                             "path loop; platform full-GPU path-loop kernel is not available yet");
        context.recordTraceMessage(
          "compiled diffuse path loop rendered " +
          std::to_string(generation.generatedPrimarySamples) +
          " primary path state(s) through the " +
          (loop.fullGpuPathLoopSupported() ? "platform GPU" : "CPU reference") + " backend");
        QJsonObject metadata = compiledDiffusePathLoopMetadata(
          compilation, generation, loop, accumulation,
          state.tracingExecution().value_or(TracingExecutionPreference::Auto));
        context.setTraceMetadata(withTracingExecutionMetadata(
          metadata, context.pass(), actualTracingExecution, actualTracingFallback));
        return true;
      }

      void recordCompiledDiffusePathLoopFallback(RenderExecutionContext& context,
                                                 const std::string& fallbackReason) const {
        if (fallbackReason.empty()) {
          return;
        }
        context.recordTraceMessage("compiled diffuse path loop fallback: " + fallbackReason);
      }

      std::shared_ptr<render::RenderEngine>
      createEngine(const RenderExecutionContext& context) const override {
        return createWavefront(context);
      }
    };

    class SampleStddevAOVPass : public RenderPassPayload, private WavefrontPassSupport {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        Buffer<Colord>& output = context.storage().color(write.resource);
        Buffer<Colord> beauty(output.width(), output.height());

        auto wavefront = createWavefront(context);
        wavefront->setSampleRadianceStddevCaptureEnabled(true);
        prepareEngine(*wavefront, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(wavefront);
        wavefront->render(beauty);
        recordWavefrontMetrics(context, *wavefront);

        const auto sampleStddev = wavefront->lastSampleRadianceStddev();
        if (!sampleStddev) {
          throw passError(pass, "wavefront did not produce sample standard-deviation output");
        }
        writePreview(*sampleStddev, output, pass);
      }

    private:
      void writePreview(const Buffer<double>& source, Buffer<Colord>& destination,
                        const RenderPassNode& pass) const {
        if (source.width() != destination.width() || source.height() != destination.height()) {
          throw passError(pass,
                          "sample standard-deviation visualization requires matching dimensions");
        }

        double maximum = 0.0;
        for (int y = 0; y != source.height(); ++y) {
          for (int x = 0; x != source.width(); ++x) {
            const double value = source[y][x];
            if (std::isfinite(value)) {
              maximum = std::max(maximum, value);
            }
          }
        }

        for (int y = 0; y != source.height(); ++y) {
          for (int x = 0; x != source.width(); ++x) {
            const double value = source[y][x];
            const double normalized =
              maximum > 0.0 && std::isfinite(value) ? std::clamp(value / maximum, 0.0, 1.0) : 0.0;
            destination[y][x] = Colord(normalized, normalized, normalized);
          }
        }
      }
    };

    class SampleStddevColorAOVPass : public RenderPassPayload, private WavefrontPassSupport {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        Buffer<Colord>& output = context.storage().color(write.resource);
        Buffer<Colord> beauty(output.width(), output.height());

        auto wavefront = createWavefront(context);
        wavefront->setSampleRadianceStddevCaptureEnabled(true);
        prepareEngine(*wavefront, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(wavefront);
        wavefront->render(beauty);
        recordWavefrontMetrics(context, *wavefront);

        const auto sampleStddevColor = wavefront->lastSampleRadianceStddevColor();
        if (!sampleStddevColor) {
          throw passError(pass, "wavefront did not produce color sample standard-deviation output");
        }
        writePreview(*sampleStddevColor, output, pass);
      }

    private:
      void writePreview(const Buffer<Colord>& source, Buffer<Colord>& destination,
                        const RenderPassNode& pass) const {
        if (source.width() != destination.width() || source.height() != destination.height()) {
          throw passError(
            pass, "color sample standard-deviation visualization requires matching dimensions");
        }

        double maximum = 0.0;
        for (int y = 0; y != source.height(); ++y) {
          for (int x = 0; x != source.width(); ++x) {
            const Colord value = source[y][x];
            if (std::isfinite(value.r()))
              maximum = std::max(maximum, value.r());
            if (std::isfinite(value.g()))
              maximum = std::max(maximum, value.g());
            if (std::isfinite(value.b()))
              maximum = std::max(maximum, value.b());
          }
        }

        for (int y = 0; y != source.height(); ++y) {
          for (int x = 0; x != source.width(); ++x) {
            const Colord value = source[y][x];
            const double scale = maximum > 0.0 ? 1.0 / maximum : 0.0;
            destination[y][x] = Colord(normalized(value.r(), scale), normalized(value.g(), scale),
                                       normalized(value.b(), scale));
          }
        }
      }

      double normalized(double value, double scale) const {
        return std::isfinite(value) ? std::clamp(value * scale, 0.0, 1.0) : 0.0;
      }
    };

    /**
      * Whole-frame beauty payload backed by the software rasterizer.
      */
    class RasterBeautyPass : public BeautyPassPayload, private OpenGLRasterTraceMessageRecorder {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        auto renderTextureBindings = bindRenderTextureMaterials(context);
        recordRenderTextureBackendDiagnostic(context, renderTextureBindings);
        auto engine = createEngine(context);
        prepareEngine(*engine, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(engine);
        engine->render(context.storage().color(write.resource));
        recordTraceMessages(context, engine);
        recordRasterMetrics(context, engine);
      }

      bool executeDisplay(RenderExecutionContext& context, Buffer<unsigned int>& buffer,
                          std::shared_ptr<render::Tonemap> tonemap) override {
        auto renderTextureBindings = bindRenderTextureMaterials(context);
        recordRenderTextureBackendDiagnostic(context, renderTextureBindings);
        auto engine = createEngine(context);
        prepareEngine(*engine, context.graph(), context.cancelled(), std::move(tonemap));
        context.setActiveEngine(engine);
        engine->render(buffer);
        recordTraceMessages(context, engine);
        recordRasterMetrics(context, engine);
        return true;
      }

    private:
      std::shared_ptr<render::RenderEngine>
      createEngine(const RenderExecutionContext& context) const override {
        const auto& graph = context.graph();
        auto camera = context.camera() ? context.camera()->clone() : nullptr;
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

      void recordTraceMessages(RenderExecutionContext& context,
                               const std::shared_ptr<render::RenderEngine>& engine) const {
        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(context.pass());
        if (!state.execution().backend().isOpenGL()) {
          return;
        }

        const auto rasterizer =
          std::static_pointer_cast<::engine::raster::OpenGLRasterizer>(engine);
        recordOpenGLRasterTraceMessages(context, rasterizer);
      }

      void recordRasterMetrics(RenderExecutionContext& context,
                               const std::shared_ptr<render::RenderEngine>& engine) const {
        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(context.pass());
        if (!state.execution().backend().usesSoftwareRasterizer()) {
          return;
        }

        const auto rasterizer = std::static_pointer_cast<::engine::raster::Rasterizer>(engine);
        context.setTraceMetadata(
          ::engine::raster::rasterRenderMetricsToJson(rasterizer->lastMetrics()));
      }

      void recordRenderTextureBackendDiagnostic(
        RenderExecutionContext& context,
        const ScopedRenderTextureMaterialBindings& bindings) const {
        if (!bindings.hasInputs()) {
          return;
        }
        const RasterBeautyPassState state = RasterBeautyPassState::valueFromPass(context.pass());
        if (state.execution().backend().isOpenGL()) {
          context.recordTraceMessage(
            "OpenGL raster backend cannot sample graph render-to-texture outputs directly; "
            "receiver materials use the CPU-backed texture fallback when available");
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

        auto camera = context.camera() ? context.camera()->clone() : nullptr;
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

        auto camera = context.camera() ? context.camera()->clone() : nullptr;
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

    class HybridVisibilityAOVPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        Buffer<Colord>& output = context.storage().color(write.resource);
        output.clear(Colord::black());

        auto camera = context.camera() ? context.camera()->clone() : nullptr;
        auto scene = context.graph().scene();
        if (!camera || !scene) {
          context.recordTraceMessage(
            "hybrid visibility AOV skipped because the pass has no camera or scene");
          return;
        }

        auto plane = camera->viewPlane();
        if (!plane) {
          context.recordTraceMessage(
            "hybrid visibility AOV skipped because the camera has no view plane");
          return;
        }

        const Recti targetRect(output.width(), output.height());
        plane->setup(camera->matrix(), targetRect);

        Recti renderRect = targetRect;
        if (plane->aspectMode() == render::AspectMode::FitExact) {
          renderRect = plane->innerRect();
        }

        std::vector<render::State> states;
        std::vector<render::WavefrontClosestHitQuery> queries;
        queries.reserve(static_cast<std::size_t>(std::max(0, renderRect.width())) *
                        static_cast<std::size_t>(std::max(0, renderRect.height())));
        states.resize(queries.capacity());

        std::vector<std::pair<int, int>> pixels;
        pixels.reserve(queries.capacity());

        render::NullSampleStream stream;
        for (int y = renderRect.top(); y < renderRect.bottom(); ++y) {
          for (int x = renderRect.left(); x < renderRect.right(); ++x) {
            Rayd ray = camera->rayForPixel(static_cast<double>(x) + 0.5,
                                           static_cast<double>(y) + 0.5, stream);
            if (!ray.direction().isDefined()) {
              continue;
            }

            const std::size_t index = queries.size();
            queries.push_back({ray, &states[index]});
            pixels.push_back({x, y});
          }
        }

        if (queries.empty() || context.cancelled()) {
          return;
        }

        render::WavefrontIntersectionBackendSelectionContext selectionContext;
        selectionContext.setExpectedQueryFamilies(queries.size(), 0);
        const auto backendChoice =
          RaytracerBeautyPassState::valueFromPass(pass).intersectionBackend().value_or(
            render::WavefrontIntersectionBackendChoice::automatic());
        render::IntersectionService service(*scene, backendChoice, selectionContext);
        const std::size_t queryCount = queries.size();
        const auto hits = service.closestHits(std::move(queries));
        if (hits.size() != queryCount) {
          throw passError(pass, "intersection service returned an unexpected closest-hit count");
        }

        double minDistance = std::numeric_limits<double>::infinity();
        double maxDistance = -std::numeric_limits<double>::infinity();
        for (const auto& hit : hits) {
          if (!hit.hit()) {
            continue;
          }
          const double distance = hit.hitPoint.distance();
          if (!std::isfinite(distance)) {
            continue;
          }
          minDistance = std::min(minDistance, distance);
          maxDistance = std::max(maxDistance, distance);
        }

        for (std::size_t i = 0; i != hits.size(); ++i) {
          if (!hits[i].hit()) {
            continue;
          }
          const auto [x, y] = pixels[i];
          const double visibility =
            normalizedHitDistance(hits[i].hitPoint.distance(), minDistance, maxDistance);
          output[y][x] = Colord(visibility, visibility, visibility);
        }

        recordTrace(context, service.diagnostics());
      }

    private:
      static double normalizedHitDistance(double distance, double minDistance, double maxDistance) {
        if (!std::isfinite(distance)) {
          return 0.0;
        }
        if (!std::isfinite(minDistance) || !std::isfinite(maxDistance)) {
          return 1.0;
        }
        const double range = maxDistance - minDistance;
        if (range <= 1e-9) {
          return 1.0;
        }
        return 1.0 - std::clamp((distance - minDistance) / range, 0.0, 1.0);
      }

      static void addTiming(QJsonObject& object,
                            const render::WavefrontIntersectionQueryTiming& timing) {
        object["uploadSeconds"] = timing.uploadSeconds;
        object["kernelSeconds"] = timing.kernelSeconds;
        object["readbackSeconds"] = timing.readbackSeconds;
        object["executionPath"] = QString::fromStdString(timing.executionPath);
        object["fallbackReason"] = QString::fromStdString(timing.fallbackReason);
      }

      static void recordTrace(RenderExecutionContext& context,
                              const render::IntersectionServiceDiagnostics& diagnostics) {
        context.recordTraceMessage(
          "hybrid visibility AOV submitted " + std::to_string(diagnostics.closestHitQueryCount) +
          " closest-hit primary ray(s) through the intersection service on " +
          diagnostics.closestHitExecutionPath);
        if (!diagnostics.fallbackReason.empty()) {
          context.recordTraceMessage("hybrid visibility AOV fallback: " +
                                     diagnostics.fallbackReason);
        }

        QJsonObject service;
        service["queryFamily"] = QStringLiteral("closest_hit");
        service["queryTag"] = QStringLiteral("debug_aov");
        service["queryCount"] = static_cast<double>(diagnostics.closestHitQueryCount);
        service["hitCount"] = static_cast<double>(diagnostics.closestHitHitCount);
        service["requestedBackend"] = QString::fromStdString(diagnostics.requestedBackend);
        service["selectedBackend"] = QString::fromStdString(diagnostics.selectedBackend);
        service["availability"] = QString::fromStdString(diagnostics.availability);
        service["platformName"] = QString::fromStdString(diagnostics.platformName);
        service["executionPath"] = QString::fromStdString(diagnostics.executionPath);
        service["closestHitExecutionPath"] =
          QString::fromStdString(diagnostics.closestHitExecutionPath);
        service["fallbackReason"] = QString::fromStdString(diagnostics.fallbackReason);
        service["closestHitRayUploadBytesEstimate"] =
          static_cast<double>(diagnostics.closestHitRayUploadBytesEstimate);
        service["closestHitReadbackBytesEstimate"] =
          static_cast<double>(diagnostics.closestHitReadbackBytesEstimate);
        service["anyHitRayUploadBytesEstimate"] =
          static_cast<double>(diagnostics.anyHitRayUploadBytesEstimate);
        service["anyHitReadbackBytesEstimate"] =
          static_cast<double>(diagnostics.anyHitReadbackBytesEstimate);
        service["queryTransferBytesEstimate"] =
          static_cast<double>(diagnostics.queryTransferBytesEstimate);
        service["closestHitFrontierResidency"] =
          QString::fromStdString(diagnostics.closestHitFrontierResidency);
        service["anyHitFrontierResidency"] =
          QString::fromStdString(diagnostics.anyHitFrontierResidency);
        service["closestHitFrontierPackedRayBytes"] =
          static_cast<double>(diagnostics.closestHitFrontierPackedRayBytes);
        service["closestHitFrontierHostPackedRayBytes"] =
          static_cast<double>(diagnostics.closestHitFrontierHostPackedRayBytes);
        service["closestHitFrontierHostQueryBytes"] =
          static_cast<double>(diagnostics.closestHitFrontierHostQueryBytes);
        service["closestHitFrontierStateHandleBytes"] =
          static_cast<double>(diagnostics.closestHitFrontierStateHandleBytes);
        service["anyHitFrontierPackedRayBytes"] =
          static_cast<double>(diagnostics.anyHitFrontierPackedRayBytes);
        service["anyHitFrontierHostPackedRayBytes"] =
          static_cast<double>(diagnostics.anyHitFrontierHostPackedRayBytes);
        service["anyHitFrontierHostQueryBytes"] =
          static_cast<double>(diagnostics.anyHitFrontierHostQueryBytes);
        service["anyHitFrontierStateHandleBytes"] =
          static_cast<double>(diagnostics.anyHitFrontierStateHandleBytes);
        addIntersectionServiceSceneDiagnostics(service, diagnostics.scene);

        QJsonObject timing;
        addTiming(timing, diagnostics.lastClosestHitTiming);
        service["closestHitTiming"] = timing;

        QJsonObject metadata;
        metadata["intersectionService"] = service;
        context.setTraceMetadata(metadata);
      }
    };

    class HybridRayTracedShadowPass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), write.resource, pass);

        Buffer<Colord>& mask = context.storage().color(write.resource);
        mask.clear(Colord::white());

        auto camera = context.camera() ? context.camera()->clone() : nullptr;
        auto scene = context.graph().scene();
        if (!camera || !scene) {
          context.recordTraceMessage(
            "hybrid ray-traced shadows skipped because the pass has no camera or scene");
          return;
        }
        if (scene->lights().empty()) {
          context.recordTraceMessage(
            "hybrid ray-traced shadows skipped because the scene has no lights");
          return;
        }

        auto plane = camera->viewPlane();
        if (!plane) {
          context.recordTraceMessage(
            "hybrid ray-traced shadows skipped because the camera has no view plane");
          return;
        }

        const Recti targetRect(mask.width(), mask.height());
        plane->setup(camera->matrix(), targetRect);
        Recti renderRect = targetRect;
        if (plane->aspectMode() == render::AspectMode::FitExact) {
          renderRect = plane->innerRect();
        }

        std::vector<render::State> primaryStates;
        std::vector<render::WavefrontClosestHitQuery> primaryQueries;
        std::vector<std::pair<int, int>> primaryPixels;
        const std::size_t pixelCapacity =
          static_cast<std::size_t>(std::max(0, renderRect.width())) *
          static_cast<std::size_t>(std::max(0, renderRect.height()));
        primaryStates.resize(pixelCapacity);
        primaryQueries.reserve(pixelCapacity);
        primaryPixels.reserve(pixelCapacity);

        render::NullSampleStream stream;
        for (int y = renderRect.top(); y < renderRect.bottom(); ++y) {
          for (int x = renderRect.left(); x < renderRect.right(); ++x) {
            Rayd ray = camera->rayForPixel(static_cast<double>(x) + 0.5,
                                           static_cast<double>(y) + 0.5, stream);
            if (!ray.direction().isDefined()) {
              continue;
            }
            const std::size_t index = primaryQueries.size();
            primaryQueries.push_back({ray, &primaryStates[index]});
            primaryPixels.push_back({x, y});
          }
        }

        if (primaryQueries.empty() || context.cancelled()) {
          return;
        }

        render::WavefrontIntersectionBackendSelectionContext selectionContext;
        selectionContext.setExpectedQueryFamilies(primaryQueries.size(),
                                                  primaryQueries.size() * scene->lights().size());
        const auto backendChoice =
          RaytracerBeautyPassState::valueFromPass(pass).intersectionBackend().value_or(
            render::WavefrontIntersectionBackendChoice::automatic());
        render::IntersectionService service(*scene, backendChoice, selectionContext);
        const std::size_t primaryQueryCount = primaryQueries.size();
        const auto hits = service.closestHits(std::move(primaryQueries));
        if (hits.size() != primaryQueryCount) {
          throw passError(pass, "intersection service returned an unexpected closest-hit count");
        }

        std::vector<render::State> shadowStates;
        std::vector<render::WavefrontAnyHitQuery> shadowQueries;
        std::vector<std::pair<int, int>> shadowPixels;
        shadowStates.resize(hits.size() * scene->lights().size());
        shadowQueries.reserve(shadowStates.size());
        shadowPixels.reserve(shadowStates.size());

        for (std::size_t i = 0; i != hits.size(); ++i) {
          if (!hits[i].hit()) {
            continue;
          }
          const Vector4d& point4 = hits[i].hitPoint.point();
          const Vector3d point(point4.x(), point4.y(), point4.z());
          const Vector3d normal = hits[i].hitPoint.normal().normalizedOrZero(1e-12);
          const Vector3d origin = point + normal * 1e-4;
          for (const auto& light : scene->lights()) {
            const render::LightSample sample = light->sample(point);
            if (!sample.direction.isDefined() ||
                (sample.radiance.r() <= 0.0 && sample.radiance.g() <= 0.0 &&
                 sample.radiance.b() <= 0.0)) {
              continue;
            }
            const double maxDistance = std::isfinite(sample.distance)
                                         ? std::max(0.0, sample.distance - 1e-4)
                                         : std::numeric_limits<double>::infinity();
            const std::size_t shadowIndex = shadowQueries.size();
            shadowQueries.push_back(
              {Rayd(origin, sample.direction), maxDistance, &shadowStates[shadowIndex]});
            shadowPixels.push_back(primaryPixels[i]);
          }
        }

        if (!shadowQueries.empty() && !context.cancelled()) {
          const std::size_t shadowQueryCount = shadowQueries.size();
          const render::WavefrontOcclusionFlags occluded =
            service.anyHits(std::move(shadowQueries));
          if (occluded.size() != shadowQueryCount) {
            throw passError(pass, "intersection service returned an unexpected any-hit count");
          }

          Buffer<double> litFractions(mask.width(), mask.height());
          litFractions.clear(1.0);
          Buffer<double> sampleCounts(mask.width(), mask.height());
          sampleCounts.clear(0.0);
          for (std::size_t i = 0; i != occluded.size(); ++i) {
            const auto [x, y] = shadowPixels[i];
            sampleCounts[y][x] += 1.0;
            if (occluded[i]) {
              litFractions[y][x] -= 1.0;
            }
          }

          for (int y = 0; y != mask.height(); ++y) {
            for (int x = 0; x != mask.width(); ++x) {
              if (sampleCounts[y][x] <= 0.0) {
                continue;
              }
              const double visibility =
                std::clamp(litFractions[y][x] / sampleCounts[y][x], 0.0, 1.0);
              mask[y][x] = Colord(visibility, visibility, visibility);
            }
          }
        }

        recordTrace(context, service.diagnostics());
      }

    private:
      static void addTiming(QJsonObject& object,
                            const render::WavefrontIntersectionQueryTiming& timing) {
        object["uploadSeconds"] = timing.uploadSeconds;
        object["kernelSeconds"] = timing.kernelSeconds;
        object["readbackSeconds"] = timing.readbackSeconds;
        object["executionPath"] = QString::fromStdString(timing.executionPath);
        object["fallbackReason"] = QString::fromStdString(timing.fallbackReason);
      }

      static void recordTrace(RenderExecutionContext& context,
                              const render::IntersectionServiceDiagnostics& diagnostics) {
        context.recordTraceMessage("hybrid ray-traced shadows submitted " +
                                   std::to_string(diagnostics.closestHitQueryCount) +
                                   " closest-hit primary ray(s) and " +
                                   std::to_string(diagnostics.anyHitQueryCount) +
                                   " any-hit shadow ray(s) through the intersection service on " +
                                   diagnostics.anyHitExecutionPath);
        if (!diagnostics.fallbackReason.empty()) {
          context.recordTraceMessage("hybrid ray-traced shadows fallback: " +
                                     diagnostics.fallbackReason);
        }

        QJsonObject service;
        service["queryFamily"] = QStringLiteral("closest_hit+any_hit");
        service["queryTag"] = QStringLiteral("hybrid_shadows");
        service["primaryQueryCount"] = static_cast<double>(diagnostics.closestHitQueryCount);
        service["primaryHitCount"] = static_cast<double>(diagnostics.closestHitHitCount);
        service["shadowQueryCount"] = static_cast<double>(diagnostics.anyHitQueryCount);
        service["occludedCount"] = static_cast<double>(diagnostics.anyHitOccludedCount);
        service["requestedBackend"] = QString::fromStdString(diagnostics.requestedBackend);
        service["selectedBackend"] = QString::fromStdString(diagnostics.selectedBackend);
        service["availability"] = QString::fromStdString(diagnostics.availability);
        service["platformName"] = QString::fromStdString(diagnostics.platformName);
        service["executionPath"] = QString::fromStdString(diagnostics.executionPath);
        service["closestHitExecutionPath"] =
          QString::fromStdString(diagnostics.closestHitExecutionPath);
        service["anyHitExecutionPath"] = QString::fromStdString(diagnostics.anyHitExecutionPath);
        service["fallbackReason"] = QString::fromStdString(diagnostics.fallbackReason);
        service["closestHitRayUploadBytesEstimate"] =
          static_cast<double>(diagnostics.closestHitRayUploadBytesEstimate);
        service["closestHitReadbackBytesEstimate"] =
          static_cast<double>(diagnostics.closestHitReadbackBytesEstimate);
        service["anyHitRayUploadBytesEstimate"] =
          static_cast<double>(diagnostics.anyHitRayUploadBytesEstimate);
        service["anyHitReadbackBytesEstimate"] =
          static_cast<double>(diagnostics.anyHitReadbackBytesEstimate);
        service["queryTransferBytesEstimate"] =
          static_cast<double>(diagnostics.queryTransferBytesEstimate);
        service["closestHitFrontierResidency"] =
          QString::fromStdString(diagnostics.closestHitFrontierResidency);
        service["anyHitFrontierResidency"] =
          QString::fromStdString(diagnostics.anyHitFrontierResidency);
        service["closestHitFrontierPackedRayBytes"] =
          static_cast<double>(diagnostics.closestHitFrontierPackedRayBytes);
        service["closestHitFrontierHostPackedRayBytes"] =
          static_cast<double>(diagnostics.closestHitFrontierHostPackedRayBytes);
        service["closestHitFrontierHostQueryBytes"] =
          static_cast<double>(diagnostics.closestHitFrontierHostQueryBytes);
        service["closestHitFrontierStateHandleBytes"] =
          static_cast<double>(diagnostics.closestHitFrontierStateHandleBytes);
        service["anyHitFrontierPackedRayBytes"] =
          static_cast<double>(diagnostics.anyHitFrontierPackedRayBytes);
        service["anyHitFrontierHostPackedRayBytes"] =
          static_cast<double>(diagnostics.anyHitFrontierHostPackedRayBytes);
        service["anyHitFrontierHostQueryBytes"] =
          static_cast<double>(diagnostics.anyHitFrontierHostQueryBytes);
        service["anyHitFrontierStateHandleBytes"] =
          static_cast<double>(diagnostics.anyHitFrontierStateHandleBytes);
        addIntersectionServiceSceneDiagnostics(service, diagnostics.scene);

        QJsonObject closestTiming;
        addTiming(closestTiming, diagnostics.lastClosestHitTiming);
        service["closestHitTiming"] = closestTiming;
        QJsonObject anyTiming;
        addTiming(anyTiming, diagnostics.lastAnyHitTiming);
        service["anyHitTiming"] = anyTiming;

        QJsonObject metadata;
        metadata["intersectionService"] = service;
        context.setTraceMetadata(metadata);
      }
    };

    class HybridShadowCompositePass : public RenderPassPayload {
    public:
      void execute(RenderExecutionContext& context) override {
        const auto& pass = context.pass();
        if (pass.reads.size() != 2) {
          throw passError(pass, "hybrid shadow composite requires color and shadow-mask inputs");
        }
        const auto& write = pass.singleWrite();
        requireColorResource(context.storage(), pass.reads[0].resource, pass);
        requireColorResource(context.storage(), pass.reads[1].resource, pass);
        requireColorResource(context.storage(), write.resource, pass);

        const Buffer<Colord>& color = context.storage().color(pass.reads[0].resource);
        const Buffer<Colord>& mask = context.storage().color(pass.reads[1].resource);
        Buffer<Colord>& output = context.storage().color(write.resource);
        requireMatchingSize(color, mask, "hybrid shadow composite");
        requireMatchingSize(color, output, "hybrid shadow composite");

        for (int y = 0; y != output.height(); ++y) {
          for (int x = 0; x != output.width(); ++x) {
            const double visibility = std::clamp(mask[y][x].r(), 0.0, 1.0);
            output[y][x] = color[y][x] * visibility;
          }
        }
      }
    };

    class RasterDiagnosticAOVPass : public RenderPassPayload,
                                    protected OpenGLRasterTraceMessageRecorder {
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

        auto camera = context.camera() ? context.camera()->clone() : nullptr;
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
        auto camera = context.camera() ? context.camera()->clone() : nullptr;
        auto rasterizer = std::make_shared<::engine::raster::OpenGLRasterizer>(
          std::move(camera), context.graph().scene());
        state.applyTo(*rasterizer);

        const auto& write = context.pass().singleWrite();
        prepareEngine(*rasterizer, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(rasterizer);
        rasterizer->renderDepth(context.storage().depth(write.resource));
        recordOpenGLRasterTraceMessages(context, rasterizer);
      }
    };

    class RasterStencilAOVPass : public RenderPassPayload,
                                 private OpenGLRasterTraceMessageRecorder {
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

        auto camera = context.camera() ? context.camera()->clone() : nullptr;
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
        auto camera = context.camera() ? context.camera()->clone() : nullptr;
        auto rasterizer = std::make_shared<::engine::raster::OpenGLRasterizer>(
          std::move(camera), context.graph().scene());
        state.applyTo(*rasterizer);

        prepareEngine(*rasterizer, context.graph(), context.cancelled(), context.graph().tonemap());
        context.setActiveEngine(rasterizer);
        rasterizer->renderStencil(stencil);
        recordOpenGLRasterTraceMessages(context, rasterizer);
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
        auto camera = context.camera() ? context.camera()->clone() : nullptr;
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
        auto camera = context.camera() ? context.camera()->clone() : nullptr;
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

    class RasterVisibilitySetArtifact : public RenderGraphCachedArtifact {
    public:
      RasterVisibilitySetArtifact(
        RenderGraphCacheKey key,
        std::shared_ptr<const ::engine::raster::RasterVisibilitySet> visibilitySet,
        std::string description = {})
          : RenderGraphCachedArtifact(std::move(key), std::move(description)),
            m_visibilitySet(std::move(visibilitySet)) {
      }

      std::shared_ptr<const ::engine::raster::RasterVisibilitySet> visibilitySet() const {
        return m_visibilitySet;
      }

    private:
      std::shared_ptr<const ::engine::raster::RasterVisibilitySet> m_visibilitySet;
    };

    class RasterVisibilityCullingPass : public RenderPassPayload {
    private:
      using MeshStats = ::engine::raster::RasterVisibilitySceneCache::MeshStats;
      using MeshStatsLookup = ::engine::raster::RasterVisibilitySceneCache::MeshStatsLookup;
      using MaterialCullabilityLookup =
        ::engine::raster::RasterVisibilitySceneCache::MaterialCullabilityLookup;

      struct VisibilityBuildResult {
        std::shared_ptr<::engine::raster::RasterVisibilitySet> visibilitySet;
        std::size_t meshCacheHits{0};
        std::size_t meshCacheMisses{0};
        std::size_t boundsCacheHits{0};
        std::size_t boundsCacheMisses{0};
        std::size_t materialCullabilityCacheHits{0};
        std::size_t materialCullabilityCacheMisses{0};
      };

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
        const RenderGraphCacheKey cacheKey = RenderGraphCacheKey::forPassOutput(
          pass, descriptor, context.graph().cacheInputFingerprintForPass(pass));
        if (restoreFromCache(context, write.resource, cacheKey, state)) {
          return;
        }

        const VisibilityBuildResult result = buildVisibilitySet(context, state);
        context.storage().setVisibilitySet(write.resource, result.visibilitySet);
        auto artifact = std::make_shared<RasterVisibilitySetArtifact>(
          cacheKey, result.visibilitySet, "raster visibility set");
        context.storage().resource(write.resource).setArtifact(artifact);
        context.graph().artifactCache()->store(artifact);
        context.storage()
          .resource(write.resource)
          .setCacheMetadata(
            {RenderGraphCacheStatus::Stored, "cache miss; stored raster visibility-set artifact"});
        context.recordTraceMessage(traceMessage(
          *result.visibilitySet, state, RenderGraphCacheStatus::Stored, result.meshCacheHits,
          result.meshCacheMisses, result.boundsCacheHits, result.boundsCacheMisses,
          result.materialCullabilityCacheHits, result.materialCullabilityCacheMisses));
      }

    private:
      struct VisibleLeafDepth {
        std::size_t leafIndex{0};
        double depth{0.0};
      };

      struct ProjectedTileCoverage {
        std::vector<std::size_t> tiles;
        double nearestDepth{std::numeric_limits<double>::infinity()};
      };

      static constexpr int TileSize = 32;

      bool restoreFromCache(RenderExecutionContext& context, const RenderResourceId& resourceId,
                            const RenderGraphCacheKey& cacheKey,
                            const RasterVisibilityPassState& state) const {
        auto cached = context.graph().artifactCache()->find(cacheKey);
        if (!cached) {
          return false;
        }

        auto artifact = std::dynamic_pointer_cast<const RasterVisibilitySetArtifact>(cached);
        if (!artifact || !artifact->visibilitySet()) {
          context.storage()
            .resource(resourceId)
            .setCacheMetadata({RenderGraphCacheStatus::Invalidated,
                               "cached artifact type did not match the visibility-set resource"});
          return false;
        }

        context.storage().setVisibilitySet(resourceId, artifact->visibilitySet());
        context.storage().resource(resourceId).setArtifact(std::move(artifact));
        context.storage()
          .resource(resourceId)
          .setCacheMetadata(
            {RenderGraphCacheStatus::Hit, "restored raster visibility-set artifact from cache"});
        context.recordTraceMessage(traceMessage(*context.storage().visibilitySet(resourceId), state,
                                                RenderGraphCacheStatus::Hit, 0, 0, 0, 0, 0, 0));
        return true;
      }

      VisibilityBuildResult buildVisibilitySet(const RenderExecutionContext& context,
                                               const RasterVisibilityPassState& state) const {
        auto visibilitySet = std::make_shared<::engine::raster::RasterVisibilitySet>();
        const auto& write = context.pass().singleWrite();
        const auto& descriptor = context.storage().descriptor(write.resource);
        visibilitySet->setTileGrid(descriptor.width, descriptor.height, TileSize, TileSize);
        const auto scene = context.graph().scene();
        if (!scene) {
          return {visibilitySet, 0, 0, 0, 0};
        }

        const int lod = state.geometry().lod();
        const std::shared_ptr<render::Camera> camera = context.camera();
        const render::HomogeneousClipVolume clipVolume = rasterClipVolume();
        std::size_t meshCacheHits = 0;
        std::size_t meshCacheMisses = 0;
        std::size_t boundsCacheHits = 0;
        std::size_t boundsCacheMisses = 0;
        std::size_t materialCullabilityCacheHits = 0;
        std::size_t materialCullabilityCacheMisses = 0;
        std::vector<VisibleLeafDepth> visibleLeafDepths;
        const bool tileDepthSummariesAllowed = state.frontToBackOrderingEnabled();
        bool orderable = state.frontToBackOrderingEnabled() && camera != nullptr;
        scene->forEachTransformedLeaf(
          nullptr, Matrix4d(), Matrix3d(), [&](const render::Primitive::TransformedLeaf& leaf) {
            const std::size_t leafIndex = visibilitySet->leafCount();
            const MeshStatsLookup meshStats = meshStatsFor(context, leaf.primitive, lod);
            if (meshStats.hit) {
              ++meshCacheHits;
            } else {
              ++meshCacheMisses;
            }
            const MeshStats& stats = meshStats.stats;
            const auto bounds = transformedBoundsFor(context, leaf);
            if (bounds.hit) {
              ++boundsCacheHits;
            } else {
              ++boundsCacheMisses;
            }
            if (boundsOutsideFrustum(bounds.bounds, camera.get(), clipVolume)) {
              visibilitySet->addRejectedLeaf(
                ::engine::raster::RasterVisibilitySet::RejectionReason::Frustum,
                stats.triangleCount, stats.faceCount);
              return;
            }
            const ::engine::raster::Rasterizer::CullMode cullMode =
              visibilityCullModeFor(context, state.geometry(), leaf.material,
                                    materialCullabilityCacheHits, materialCullabilityCacheMisses);
            if (leafFullyBackfacing(leaf, camera.get(), clipVolume, cullMode, stats)) {
              visibilitySet->addRejectedLeaf(
                ::engine::raster::RasterVisibilitySet::RejectionReason::Backface,
                stats.triangleCount, stats.faceCount);
              return;
            }

            visibilitySet->addVisibleLeaf(stats.triangleCount, stats.faceCount);
            if (auto coverage = projectedBoundsTiles(bounds.bounds, camera.get(), clipVolume,
                                                     visibilitySet->tileGrid())) {
              if (tileDepthSummariesAllowed) {
                visibilitySet->setVisibleLeafTiles(leafIndex, std::move(coverage->tiles),
                                                   coverage->nearestDepth);
              } else {
                visibilitySet->setVisibleLeafTiles(leafIndex, std::move(coverage->tiles));
              }
            }
            if (orderable) {
              if (const auto depth = frontToBackDepth(bounds.bounds, camera.get())) {
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
        return {visibilitySet,
                meshCacheHits,
                meshCacheMisses,
                boundsCacheHits,
                boundsCacheMisses,
                materialCullabilityCacheHits,
                materialCullabilityCacheMisses};
      }

      render::HomogeneousClipVolume rasterClipVolume() const {
        const engine::raster::Rasterizer defaultRasterizer(nullptr);
        return render::HomogeneousClipVolume(defaultRasterizer.nearClipDepth(),
                                             defaultRasterizer.farClipDepth());
      }

      bool boundsOutsideFrustum(const BoundingBoxd& bounds, const render::Camera* camera,
                                const render::HomogeneousClipVolume& clipVolume) const {
        if (!camera) {
          return false;
        }

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

      bool leafFullyBackfacing(const render::Primitive::TransformedLeaf& leaf,
                               const render::Camera* camera,
                               const render::HomogeneousClipVolume& clipVolume,
                               ::engine::raster::Rasterizer::CullMode cullMode,
                               const MeshStats& stats) const {
        if (!camera || cullMode == ::engine::raster::Rasterizer::CullMode::Both || !stats.mesh ||
            stats.triangleCount == 0) {
          return false;
        }

        const auto& sourceVertices = stats.mesh->vertices();
        std::vector<::engine::raster::detail::ProjectedVertex> projected(sourceVertices.size());
        std::vector<Mesh::Vertex> vertices;
        vertices.reserve(sourceVertices.size());
        for (std::size_t vi = 0; vi != sourceVertices.size(); ++vi) {
          const auto& vertex = sourceVertices[vi];
          vertices.emplace_back(leaf.transformPoint(vertex.point),
                                leaf.transformNormal(vertex.normal).normalizedOrZero(1e-12),
                                vertex.uv);
          const Vector4d clip = camera->projectPointToClipSpace(vertices.back().point);
          if (clip.isUndefined() || clip.w() == 0.0) {
            return false;
          }

          const double invW = 1.0 / clip.w();
          const Vector3d screen(clip.x() * invW, clip.y() * invW, clip.z());
          if (!screen.isDefined()) {
            return false;
          }
          projected[vi] = {clip, screen, clipVolume.outCode(clip)};
        }

        const ::engine::raster::detail::TriangleCullPolicy cullPolicy{cullMode, true};
        bool testedTriangle = false;
        for (const auto& face : stats.mesh->faces()) {
          if (face.size() < 3) {
            continue;
          }

          for (std::size_t i = 1; i + 1 < face.size(); ++i) {
            const auto& p0 = projected[face[0]];
            const auto& p1 = projected[face[i]];
            const auto& p2 = projected[face[i + 1]];
            if ((p0.outCode | p1.outCode | p2.outCode) != 0) {
              return false;
            }

            const auto& v0 = vertices[face[0]];
            const auto& v1 = vertices[face[i]];
            const auto& v2 = vertices[face[i + 1]];
            const ::engine::raster::detail::ClipVert c0{v0.point, v0.normal, v0.uv, p0.clip,
                                                        p0.screen};
            const ::engine::raster::detail::ClipVert c1{v1.point, v1.normal, v1.uv, p1.clip,
                                                        p1.screen};
            const ::engine::raster::detail::ClipVert c2{v2.point, v2.normal, v2.uv, p2.clip,
                                                        p2.screen};
            if (!cullPolicy.shouldCull(cullMode, c0, c1, c2)) {
              return false;
            }
            testedTriangle = true;
          }
        }
        return testedTriangle;
      }

      ::engine::raster::Rasterizer::CullMode
      visibilityCullModeFor(const RenderExecutionContext& context,
                            const RasterGeometryState& geometry,
                            const std::shared_ptr<render::Material>& material,
                            std::size_t& materialCullabilityCacheHits,
                            std::size_t& materialCullabilityCacheMisses) const {
        if (geometry.hasCullModeOverride()) {
          return geometry.cullMode();
        }

        const MaterialCullabilityLookup lookup = materialCullabilityFor(context, material);
        if (lookup.hit) {
          ++materialCullabilityCacheHits;
        } else {
          ++materialCullabilityCacheMisses;
        }
        return lookup.cullability.defaultCullMode;
      }

      std::optional<ProjectedTileCoverage>
      projectedBoundsTiles(const BoundingBoxd& bounds, const render::Camera* camera,
                           const render::HomogeneousClipVolume& clipVolume,
                           const ::engine::raster::RasterVisibilitySet::TileGrid& grid) const {
        if (!camera || !grid.enabled()) {
          return std::nullopt;
        }

        if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite()) {
          return std::nullopt;
        }

        double minX = std::numeric_limits<double>::infinity();
        double minY = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();
        double nearestDepth = std::numeric_limits<double>::infinity();
        for (const Vector3d& corner : bounds.vertices()) {
          const Vector4d clip = camera->projectPointToClipSpace(corner);
          if (clip.isUndefined() || clip.w() == 0.0 || !std::isfinite(clip.z()) ||
              clipVolume.outCode(clip) != 0) {
            return std::nullopt;
          }

          const double invW = 1.0 / clip.w();
          const double pixelX = (clip.x() * invW * 0.5 + 0.5) * grid.width;
          const double pixelY = (0.5 - clip.y() * invW * 0.5) * grid.height;
          if (!std::isfinite(pixelX) || !std::isfinite(pixelY)) {
            return std::nullopt;
          }
          minX = std::min(minX, pixelX);
          minY = std::min(minY, pixelY);
          maxX = std::max(maxX, pixelX);
          maxY = std::max(maxY, pixelY);
          nearestDepth = std::min(nearestDepth, clip.z());
        }

        const int left = std::clamp(static_cast<int>(std::floor(minX)), 0, grid.width - 1);
        const int top = std::clamp(static_cast<int>(std::floor(minY)), 0, grid.height - 1);
        const int right = std::clamp(static_cast<int>(std::ceil(maxX)) - 1, 0, grid.width - 1);
        const int bottom = std::clamp(static_cast<int>(std::ceil(maxY)) - 1, 0, grid.height - 1);
        if (left > right || top > bottom) {
          return std::nullopt;
        }

        const int tileLeft = left / grid.tileWidth;
        const int tileRight = right / grid.tileWidth;
        const int tileTop = top / grid.tileHeight;
        const int tileBottom = bottom / grid.tileHeight;
        std::vector<std::size_t> tiles;
        tiles.reserve(
          static_cast<std::size_t>((tileRight - tileLeft + 1) * (tileBottom - tileTop + 1)));
        for (int ty = tileTop; ty <= tileBottom; ++ty) {
          for (int tx = tileLeft; tx <= tileRight; ++tx) {
            tiles.push_back(static_cast<std::size_t>(ty * grid.columns + tx));
          }
        }
        return ProjectedTileCoverage{std::move(tiles), nearestDepth};
      }

      std::optional<double> frontToBackDepth(const BoundingBoxd& bounds,
                                             const render::Camera* camera) const {
        if (!camera) {
          return std::nullopt;
        }

        if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite()) {
          return std::nullopt;
        }

        const Vector4d clip = camera->projectPointToClipSpace(bounds.center());
        if (clip.isUndefined() || !std::isfinite(clip.z())) {
          return std::nullopt;
        }
        return clip.z();
      }

      MeshStatsLookup meshStatsFor(const RenderExecutionContext& context,
                                   const render::Primitive* primitive, int lod) const {
        if (!primitive) {
          return {};
        }
        return context.graph().rasterVisibilitySceneCache()->meshStatsFor(*primitive, lod);
      }

      ::engine::raster::RasterVisibilitySceneCache::TransformedBoundsLookup
      transformedBoundsFor(const RenderExecutionContext& context,
                           const render::Primitive::TransformedLeaf& leaf) const {
        if (!leaf.primitive) {
          return {};
        }
        return context.graph().rasterVisibilitySceneCache()->transformedBoundsFor(*leaf.primitive,
                                                                                  leaf.pointMatrix);
      }

      MaterialCullabilityLookup
      materialCullabilityFor(const RenderExecutionContext& context,
                             const std::shared_ptr<render::Material>& material) const {
        return context.graph().rasterVisibilitySceneCache()->materialCullabilityFor(material);
      }

      std::string traceMessage(const ::engine::raster::RasterVisibilitySet& visibilitySet,
                               const RasterVisibilityPassState& state,
                               RenderGraphCacheStatus cacheStatus, std::size_t meshCacheHits,
                               std::size_t meshCacheMisses, std::size_t boundsCacheHits,
                               std::size_t boundsCacheMisses,
                               std::size_t materialCullabilityCacheHits,
                               std::size_t materialCullabilityCacheMisses) const {
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
            << "; cache=" << toString(cacheStatus) << "; lod=" << state.geometry().lod()
            << "; meshCacheHits=" << meshCacheHits << "; meshCacheMisses=" << meshCacheMisses
            << "; boundsCacheHits=" << boundsCacheHits
            << "; boundsCacheMisses=" << boundsCacheMisses
            << "; materialCullabilityCacheHits=" << materialCullabilityCacheHits
            << "; materialCullabilityCacheMisses=" << materialCullabilityCacheMisses
            << "; inputLeaves=" << visibilitySet.leafCount()
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
            << "; backfaceRejectedLeaves="
            << visibilitySet.rejectedLeafCount(
                 ::engine::raster::RasterVisibilitySet::RejectionReason::Backface)
            << "; backfaceRejectedTriangles="
            << visibilitySet.rejectedTriangleCount(
                 ::engine::raster::RasterVisibilitySet::RejectionReason::Backface)
            << "; tileGrid=" << visibilitySet.tileGrid().columns << "x"
            << visibilitySet.tileGrid().rows << "; tileSize=" << visibilitySet.tileGrid().tileWidth
            << "x" << visibilitySet.tileGrid().tileHeight
            << "; coveredTiles=" << visibilitySet.coveredTileCount()
            << "; visibleTileReferences=" << visibilitySet.visibleLeafTileReferenceCount()
            << "; uncertainTileLeaves=" << visibilitySet.tileUncertainVisibleLeafCount()
            << "; depthSummarizedTiles=" << visibilitySet.tileDepthSummarizedTileCount()
            << "; tileDepthReferences=" << visibilitySet.tileDepthReferenceCount()
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
                           [&](const auto& feature) { return pass.hasFeature(feature); });
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
               pass.executor == RenderExecutorKind::Wireframe && !pass.hasFeature("curve_overlay");
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
               (pass.hasFeature("depth_composite") || pass.hasFeature("stencil_composite"));
      }

      std::unique_ptr<RenderPassPayload> create(const RenderPassNode&) const override {
        return std::make_unique<DepthStencilCompositePass>();
      }
    };

    const std::vector<const BuiltinPassPayloadFactory*>& builtinPayloadFactories() {
      static const ExactPassPayloadFactory<RaytraceBeautyPass> raytraceBeauty(
        RenderPassKind::Beauty, RenderExecutorKind::Raytracer);
      static const ExactPassPayloadFactory<WavefrontBeautyPass> wavefrontBeauty(
        RenderPassKind::Beauty, RenderExecutorKind::Wavefront);
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
      static const FeaturePassPayloadFactory<HybridRayTracedShadowPass> hybridRayTracedShadows(
        RenderPassKind::Shadow, RenderExecutorKind::Raytracer, {"ray_traced_shadows"});
      static const FeaturePassPayloadFactory<HybridShadowCompositePass> hybridShadowComposite(
        RenderPassKind::Composite, RenderExecutorKind::Composite, {"ray_traced_shadows"});
      static const FeaturePassPayloadFactory<DepthVisualizationPass> depthVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"depth", "visualization"});
      static const FeaturePassPayloadFactory<DepthAOVPass> depthAOV(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"depth"});
      static const FeaturePassPayloadFactory<DepthAOVPass> depthAOVWavefront(
        RenderPassKind::AOV, RenderExecutorKind::Wavefront, {"depth"});
      static const FeaturePassPayloadFactory<RasterDepthAOVPass> depthAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"depth"});
      static const FeaturePassPayloadFactory<DepthAOVPass> depthAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"depth"});
      static const FeaturePassPayloadFactory<StencilVisualizationPass> stencilVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"stencil", "visualization"});
      static const FeaturePassPayloadFactory<StencilAOVPass> stencilAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"stencil"});
      static const FeaturePassPayloadFactory<StencilAOVPass> stencilAOVWavefront(
        RenderPassKind::AOV, RenderExecutorKind::Wavefront, {"stencil"});
      static const FeaturePassPayloadFactory<RasterStencilAOVPass> stencilAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"stencil"});
      static const FeaturePassPayloadFactory<StencilAOVPass> stencilAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"stencil"});
      static const FeaturePassPayloadFactory<NormalAOVPass> normalAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"normal"});
      static const FeaturePassPayloadFactory<NormalAOVPass> normalAOVWavefront(
        RenderPassKind::AOV, RenderExecutorKind::Wavefront, {"normal"});
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
      static const FeaturePassPayloadFactory<ObjectIdAOVPass> objectIdAOVWavefront(
        RenderPassKind::AOV, RenderExecutorKind::Wavefront, {"object_id"});
      static const FeaturePassPayloadFactory<RasterObjectIdAOVPass> objectIdAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"object_id"});
      static const FeaturePassPayloadFactory<ObjectIdAOVPass> objectIdAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"object_id"});
      static const FeaturePassPayloadFactory<ObjectIdVisualizationPass> materialIdVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"material_id", "visualization"});
      static const FeaturePassPayloadFactory<MaterialIdAOVPass> materialIdAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"material_id"});
      static const FeaturePassPayloadFactory<MaterialIdAOVPass> materialIdAOVWavefront(
        RenderPassKind::AOV, RenderExecutorKind::Wavefront, {"material_id"});
      static const FeaturePassPayloadFactory<RasterMaterialIdAOVPass> materialIdAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"material_id"});
      static const FeaturePassPayloadFactory<MaterialIdAOVPass> materialIdAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"material_id"});
      static const FeaturePassPayloadFactory<WorldPositionVisualizationPass>
        worldPositionVisualization(RenderPassKind::AOV, RenderExecutorKind::PostProcess,
                                   {"world_position", "visualization"});
      static const FeaturePassPayloadFactory<WorldPositionAOVPass> worldPositionAOVRaytracer(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"world_position"});
      static const FeaturePassPayloadFactory<WorldPositionAOVPass> worldPositionAOVWavefront(
        RenderPassKind::AOV, RenderExecutorKind::Wavefront, {"world_position"});
      static const FeaturePassPayloadFactory<RasterWorldPositionAOVPass> worldPositionAOVRasterizer(
        RenderPassKind::AOV, RenderExecutorKind::Rasterizer, {"world_position"});
      static const FeaturePassPayloadFactory<WorldPositionAOVPass> worldPositionAOVWireframe(
        RenderPassKind::AOV, RenderExecutorKind::Wireframe, {"world_position"});
      static const FeaturePassPayloadFactory<ColorCopyPass> sampleStddevVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess, {"sample_stddev", "visualization"});
      static const FeaturePassPayloadFactory<SampleStddevAOVPass> sampleStddevAOVWavefront(
        RenderPassKind::AOV, RenderExecutorKind::Wavefront, {"sample_stddev"});
      static const FeaturePassPayloadFactory<ColorCopyPass> sampleStddevColorVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess,
        {"sample_stddev_color", "visualization"});
      static const FeaturePassPayloadFactory<SampleStddevColorAOVPass>
        sampleStddevColorAOVWavefront(RenderPassKind::AOV, RenderExecutorKind::Wavefront,
                                      {"sample_stddev_color"});
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
      static const FeaturePassPayloadFactory<ColorCopyPass> hybridVisibilityVisualization(
        RenderPassKind::AOV, RenderExecutorKind::PostProcess,
        {"hybrid_visibility", "visualization"});
      static const FeaturePassPayloadFactory<HybridVisibilityAOVPass> hybridVisibilityAOV(
        RenderPassKind::AOV, RenderExecutorKind::Raytracer, {"hybrid_visibility"});
      static const FeaturePassPayloadFactory<HybridVisibilityAOVPass> hybridVisibilityAOVWavefront(
        RenderPassKind::AOV, RenderExecutorKind::Wavefront, {"hybrid_visibility"});
      static const PostProcessAAPayloadFactory postProcessAA;
      static const WireframeOverlayPayloadFactory wireframeOverlay;
      static const DepthStencilCompositePayloadFactory depthStencilComposite;
      static const FeaturePassPayloadFactory<CurveOverlayPass> curveOverlay(
        RenderPassKind::Overlay, RenderExecutorKind::Wireframe, {"curve_overlay"});
      static const std::vector<const BuiltinPassPayloadFactory*> result = {
        &raytraceBeauty,
        &wavefrontBeauty,
        &rasterBeauty,
        &wireframeBeauty,
        &tonemap,
        &readback,
        &rasterVisibility,
        &rasterPreviewShadows,
        &hybridRayTracedShadows,
        &hybridShadowComposite,
        &depthVisualization,
        &depthAOV,
        &depthAOVWavefront,
        &depthAOVRasterizer,
        &depthAOVWireframe,
        &stencilVisualization,
        &stencilAOVRaytracer,
        &stencilAOVWavefront,
        &stencilAOVRasterizer,
        &stencilAOVWireframe,
        &normalVisualization,
        &normalAOVRaytracer,
        &normalAOVWavefront,
        &normalAOVRasterizer,
        &normalAOVWireframe,
        &objectIdVisualization,
        &objectIdAOVRaytracer,
        &objectIdAOVWavefront,
        &objectIdAOVRasterizer,
        &objectIdAOVWireframe,
        &materialIdVisualization,
        &materialIdAOVRaytracer,
        &materialIdAOVWavefront,
        &materialIdAOVRasterizer,
        &materialIdAOVWireframe,
        &worldPositionVisualization,
        &worldPositionAOVRaytracer,
        &worldPositionAOVWavefront,
        &worldPositionAOVRasterizer,
        &worldPositionAOVWireframe,
        &sampleStddevVisualization,
        &sampleStddevAOVWavefront,
        &sampleStddevColorVisualization,
        &sampleStddevColorAOVWavefront,
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
        &hybridVisibilityVisualization,
        &hybridVisibilityAOV,
        &hybridVisibilityAOVWavefront,
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
