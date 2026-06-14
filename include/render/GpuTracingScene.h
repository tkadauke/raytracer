#pragma once

#include "core/Color.h"
#include "render/GpuIntersectionScene.h"
#include "render/textures/Texture.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace render {
  class Light;
  class Material;
  class Scene;

  inline constexpr std::uint32_t gpuTracingSceneLayoutVersion = 1u;

  enum class GpuTracingSceneSectionKind : std::uint32_t {
    Geometry = 1,
    Materials = 2,
    Textures = 3,
    Lights = 4,
    Environment = 5,
    DebugIds = 6
  };

  enum class GpuTracingMaterialKind : std::uint32_t { Unsupported = 0, Matte = 1, Emissive = 2 };

  enum class GpuTracingTextureKind : std::uint32_t {
    Unsupported = 0,
    ConstantColor = 1,
    CheckerBoard = 2
  };

  enum class GpuTracingLightKind : std::uint32_t {
    Unsupported = 0,
    Point = 1,
    Directional = 2,
    RectangularArea = 3
  };

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

  struct GpuTracingSceneSections {
    GpuIntersectionSceneBuffers geometry;
    std::vector<GpuTracingMaterialRecord> materials;
    std::vector<GpuTracingTextureRecord> textures;
    std::vector<GpuTracingLightRecord> lights;
    std::vector<GpuTracingEnvironmentRecord> environment;
    std::vector<GpuTracingDebugIdRecord> debugIds;

    [[nodiscard]] std::array<GpuTracingSceneSectionLayout, 6> sectionLayouts() const;
    [[nodiscard]] std::size_t uploadByteCount() const;
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
}
