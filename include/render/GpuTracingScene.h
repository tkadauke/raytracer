#pragma once

#include "core/Color.h"
#include "render/GpuIntersectionScene.h"
#include "render/samplers/SampleStream.h"
#include "render/textures/Texture.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace render {
  class CompiledIntersectionScene;
  class Light;
  class Material;
  class Scene;

  inline constexpr std::uint32_t gpuTracingSceneLayoutVersion = 4u;

  enum class GpuTracingSceneSectionKind : std::uint32_t {
    Geometry = 1,
    Materials = 2,
    Textures = 3,
    Lights = 4,
    Environment = 5,
    DebugIds = 6
  };

  enum class GpuTracingMaterialKind : std::uint32_t {
    Unsupported = 0,
    Matte = 1,
    Emissive = 2,
    Phong = 3,
    Reflective = 4,
    Transparent = 5
  };

  enum class GpuTracingTextureKind : std::uint32_t {
    Unsupported = 0,
    ConstantColor = 1,
    CheckerBoard = 2,
    Image = 3,
    Tinted = 4
  };

  enum class GpuTracingTextureMappingKind : std::uint32_t { None = 0, Planar = 1, UV = 2 };
  inline constexpr std::uint32_t gpuTracingTextureMappingMask = 0xffu;
  inline constexpr std::uint32_t gpuTracingTextureWrapClampFlag = 1u << 8u;

  enum class GpuTracingLightKind : std::uint32_t {
    Unsupported = 0,
    Point = 1,
    Directional = 2,
    RectangularArea = 3
  };

  inline constexpr std::uint32_t gpuDiffusePathStateActiveFlag = 1u << 0u;
  inline constexpr std::uint32_t gpuDiffusePathStateTerminatedFlag = 1u << 1u;
  inline constexpr std::uint32_t gpuDiffusePathStateSampledFromBsdfFlag = 1u << 2u;
  inline constexpr std::uint32_t gpuDiffusePathStateBsdfSampleDeltaFlag = 1u << 3u;
  inline constexpr std::uint32_t gpuDiffusePathStateUnsupportedFlag = 1u << 4u;

  struct GpuTracingSceneSectionLayout {
    GpuTracingSceneSectionKind kind{GpuTracingSceneSectionKind::Geometry};
    std::uint32_t layoutVersion{gpuTracingSceneLayoutVersion};
    std::uint32_t recordCount{0};
    std::uint32_t recordSize{0};
    std::uint32_t recordAlignment{0};
    std::uint32_t byteOffset{0};
    std::uint32_t byteCount{0};
  };

  struct alignas(16) GpuTracingMaterialRecord {
    std::uint32_t kind{static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported)};
    std::uint32_t albedoTexture{0};
    std::uint32_t emissionTexture{0};
    std::uint32_t flags{0};
    std::array<float, 4> parameters{};
    std::array<float, 4> specularParameters{};
    std::array<float, 4> continuationParameters{};
    std::array<float, 4> transmissionParameters{};
  };

  struct alignas(16) GpuTracingTextureRecord {
    std::uint32_t kind{static_cast<std::uint32_t>(GpuTracingTextureKind::Unsupported)};
    std::uint32_t payloadOffset{0};
    std::uint32_t payloadCount{0};
    std::uint32_t flags{0};
    std::array<float, 4> parameters{};
  };

  struct alignas(16) GpuTracingLightRecord {
    std::uint32_t kind{static_cast<std::uint32_t>(GpuTracingLightKind::Unsupported)};
    std::uint32_t emissionTexture{0};
    std::uint32_t flags{0};
    std::uint32_t object{0};
    std::array<float, 4> positionOrDirection{};
    std::array<float, 4> u{};
    std::array<float, 4> v{};
    std::array<float, 4> parameters{};
  };

  struct alignas(16) GpuTracingEnvironmentRecord {
    std::uint32_t texture{0};
    std::uint32_t flags{0};
    std::array<std::uint32_t, 2> reserved{};
    std::array<float, 4> color{};
  };

  struct alignas(16) GpuTracingDebugIdRecord {
    std::uint32_t object{0};
    std::uint32_t material{0};
    std::uint32_t sourceId{0};
    std::uint32_t flags{0};
  };

  struct alignas(16) GpuTracingShadingRecord {
    std::uint32_t material{0};
    std::uint32_t object{0};
    std::uint32_t primitiveRecord{0};
    std::uint32_t pathIndex{0};
    std::array<float, 4> point{};
    std::array<float, 4> normal{};
    std::array<float, 4> uv{};
    std::array<float, 4> throughput{};
    std::array<float, 4> accumulatedRadiance{};
  };

  /**
    * GPU-readable state for one diffuse path-tracing step.
    *
    * The record is intentionally flat and 32-bit-addressed so CPU reference
    * code, Metal, Vulkan, and future shader tests can share the same field
    * order. Active paths carry `gpuDiffusePathStateActiveFlag` and a ray to
    * intersect. Terminated paths carry `gpuDiffusePathStateTerminatedFlag`;
    * their ray and sampling fields remain defined but must not be dispatched as
    * active work.
    *
    * `pixelIndex` and `primarySampleIndex` identify the accumulation target.
    * `sampleSeed`, `sampleDimensionBase`, and `sampleDimensionStride` describe
    * the deterministic GPU sample-coordinate cursor for the current bounce.
    * `previousBsdfPdf`, `previousLightPdf`, and `previousEventFlags` preserve
    * the previous sampled event for emission/direct-light MIS weighting.
    */
  struct alignas(16) GpuDiffusePathStateRecord {
    GpuIntersectionRay ray;
    std::array<float, 4> throughput{};
    std::array<float, 4> accumulatedRadiance{};
    std::uint32_t pixelIndex{0};
    std::uint32_t primarySampleIndex{0};
    std::uint32_t depth{0};
    std::uint32_t sampleSeed{0};
    std::uint32_t sampleDimensionBase{0};
    std::uint32_t sampleDimensionStride{0};
    std::uint32_t flags{gpuDiffusePathStateTerminatedFlag};
    std::uint32_t reserved0{0};
    float previousBsdfPdf{0.0f};
    float previousLightPdf{0.0f};
    std::uint32_t previousMaterial{0};
    std::uint32_t previousEventFlags{0};
    std::array<std::uint32_t, 4> reserved{};
  };

  enum class GpuDiffusePathStepEvent : std::uint32_t {
    Inactive = 0,
    Miss = 1,
    Hit = 2,
    Unsupported = 3
  };

  struct alignas(16) GpuDiffusePathStepRecord {
    std::uint32_t event{static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive)};
    std::uint32_t pathIndex{0};
    std::uint32_t pixelIndex{0};
    std::uint32_t primarySampleIndex{0};
    std::uint32_t depth{0};
    std::uint32_t material{0};
    std::uint32_t object{0};
    std::uint32_t flags{0};
    std::array<float, 4> emittedRadiance{};
    std::array<float, 4> directLightRadiance{};
    std::array<float, 4> missRadiance{};
    std::array<float, 4> continuationThroughput{};
  };

  [[nodiscard]] inline bool
  gpuDiffusePathStateIsActive(const GpuDiffusePathStateRecord& pathState) {
    return (pathState.flags & gpuDiffusePathStateActiveFlag) != 0u &&
           (pathState.flags & gpuDiffusePathStateTerminatedFlag) == 0u;
  }

  [[nodiscard]] inline bool
  gpuDiffusePathStateIsTerminated(const GpuDiffusePathStateRecord& pathState) {
    return (pathState.flags & gpuDiffusePathStateTerminatedFlag) != 0u;
  }

  [[nodiscard]] inline GpuDiffusePathStateRecord makeActiveGpuDiffusePathState() {
    GpuDiffusePathStateRecord pathState;
    pathState.flags = gpuDiffusePathStateActiveFlag;
    pathState.throughput = {1.0f, 1.0f, 1.0f, 0.0f};
    pathState.sampleDimensionBase = static_cast<std::uint32_t>(SampleDimension::BSDF);
    pathState.sampleDimensionStride = static_cast<std::uint32_t>(kPathSampleDimensionStride);
    return pathState;
  }

  [[nodiscard]] inline GpuDiffusePathStateRecord makeTerminatedGpuDiffusePathState() {
    GpuDiffusePathStateRecord pathState;
    pathState.flags = gpuDiffusePathStateTerminatedFlag;
    return pathState;
  }

  struct GpuTracingSceneSections {
    GpuIntersectionSceneBuffers geometry;
    std::vector<GpuTracingMaterialRecord> materials;
    std::vector<GpuTracingTextureRecord> textures;
    std::vector<GpuTracingLightRecord> lights;
    // Compiled scenes write record 0 as scene ambient, record 1 as the visible
    // background for primary misses, and the last record as the environment
    // radiance for bounced misses.
    std::vector<GpuTracingEnvironmentRecord> environment;
    std::vector<GpuTracingDebugIdRecord> debugIds;

    [[nodiscard]] std::array<GpuTracingSceneSectionLayout, 6> sectionLayouts() const;
    [[nodiscard]] std::size_t uploadByteCount() const;
    [[nodiscard]] std::vector<std::uint8_t> uploadBytes() const;
  };

  struct UnsupportedGpuTracingLight {
    std::uint32_t lightIndex{0};
    std::string type;
    std::string reason;
  };

  struct UnsupportedGpuTracingMaterial {
    std::uint32_t materialId{0};
    std::string type;
    std::string reason;
  };

  struct UnsupportedGpuTracingTexture {
    std::uint32_t textureId{0};
    std::string type;
    std::string reason;
  };

  struct GpuTracingUnsupportedReasonCount {
    std::string reason;
    std::uint64_t count{0};
  };

  struct GpuTracingLightCompilation {
    std::vector<GpuTracingLightRecord> records;
    std::vector<UnsupportedGpuTracingLight> unsupportedLights;

    [[nodiscard]] bool supported() const;
    [[nodiscard]] std::vector<GpuTracingUnsupportedReasonCount> unsupportedReasonCounts() const;
  };

  struct GpuTracingTextureCompilation {
    std::vector<GpuTracingTextureRecord> records;
    std::vector<UnsupportedGpuTracingTexture> unsupportedTextures;

    [[nodiscard]] bool supported() const;
    [[nodiscard]] std::vector<GpuTracingUnsupportedReasonCount> unsupportedReasonCounts() const;
  };

  struct GpuTracingMaterialCompilation {
    std::vector<GpuTracingMaterialRecord> records;
    GpuTracingTextureCompilation textures;
    std::vector<UnsupportedGpuTracingMaterial> unsupportedMaterials;

    [[nodiscard]] bool supported() const;
    [[nodiscard]] std::vector<GpuTracingUnsupportedReasonCount> unsupportedReasonCounts() const;
  };

  struct GpuTracingSceneDiagnostics {
    bool compiled{false};
    std::uint64_t materials{0};
    std::uint64_t textures{0};
    std::uint64_t lights{0};
    std::uint64_t environment{0};
    std::uint64_t debugIds{0};
    std::uint64_t unsupportedPrimitives{0};
    std::uint64_t unsupportedMaterials{0};
    std::uint64_t unsupportedTextures{0};
    std::uint64_t unsupportedLights{0};
    std::map<std::string, std::uint64_t> unsupportedPrimitiveReasons;
    std::map<std::string, std::uint64_t> unsupportedMaterialReasons;
    std::map<std::string, std::uint64_t> unsupportedTextureReasons;
    std::map<std::string, std::uint64_t> unsupportedLightReasons;
    std::uint64_t uploadBytes{0};
  };

  struct GpuTracingSceneCompilation {
    GpuTracingSceneSections sections;
    GpuTracingMaterialCompilation materials;
    GpuTracingLightCompilation lights;
    GpuTracingSceneDiagnostics diagnostics;

    [[nodiscard]] bool supported() const;
  };

  struct GpuDiffusePathLoopSupport {
    bool supported{false};
    std::string reason;
  };

  [[nodiscard]] GpuTracingEnvironmentRecord makeGpuTracingConstantEnvironment(const Colord& color);
  [[nodiscard]] std::optional<GpuTracingTextureRecord>
  makeGpuTracingTextureRecord(const Texturec& texture, std::string* unsupportedReason = nullptr);
  [[nodiscard]] std::optional<GpuTracingMaterialRecord>
  makeGpuTracingMaterialRecord(const Material& material, std::uint32_t albedoTexture,
                               std::uint32_t emissionTexture,
                               std::string* unsupportedReason = nullptr);
  [[nodiscard]] GpuTracingMaterialCompilation
  compileGpuTracingMaterials(const CompiledIntersectionScene& scene);
  [[nodiscard]] std::optional<GpuTracingLightRecord>
  makeGpuTracingLightRecord(const Light& light, std::string* unsupportedReason = nullptr);
  [[nodiscard]] GpuTracingLightCompilation compileGpuTracingLights(const Scene& scene);
  [[nodiscard]] GpuTracingSceneCompilation
  compileGpuTracingScene(const CompiledIntersectionScene& intersectionScene, const Scene& scene);
  [[nodiscard]] GpuTracingSceneCompilation compileGpuTracingScene(const Scene& scene);
  [[nodiscard]] GpuDiffusePathLoopSupport
  gpuDiffusePathLoopSupport(const GpuTracingSceneCompilation& compilation, const Scene& scene);
  [[nodiscard]] bool supportsGpuDiffusePathLoop(const GpuTracingSceneCompilation& compilation,
                                                const Scene& scene);
  [[nodiscard]] std::string
  gpuDiffusePathLoopUnsupportedReason(const GpuTracingSceneCompilation& compilation,
                                      const Scene& scene);
  [[nodiscard]] GpuTracingSceneDiagnostics
  compileGpuTracingSceneDiagnostics(const CompiledIntersectionScene& intersectionScene,
                                    const Scene& scene);
  [[nodiscard]] GpuTracingSceneDiagnostics compileGpuTracingSceneDiagnostics(const Scene& scene);
}
