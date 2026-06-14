#include "render/GpuTracingScene.h"

#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/lights/PointLight.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"

#include <algorithm>
#include <limits>
#include <map>
#include <type_traits>

using namespace render;

namespace {
  template<typename T>
  constexpr bool isKernelRecord() {
    return std::is_standard_layout_v<T> && alignof(T) == 16 && sizeof(T) % 16 == 0;
  }

  static_assert(isKernelRecord<GpuTracingMaterialRecord>());
  static_assert(isKernelRecord<GpuTracingTextureRecord>());
  static_assert(isKernelRecord<GpuTracingLightRecord>());
  static_assert(isKernelRecord<GpuTracingEnvironmentRecord>());
  static_assert(isKernelRecord<GpuTracingDebugIdRecord>());
  static_assert(isKernelRecord<GpuTracingShadingRecord>());

  template<typename Record>
  GpuTracingSceneSectionLayout sectionLayout(GpuTracingSceneSectionKind kind,
                                             std::uint32_t recordCount, std::uint32_t byteOffset) {
    const std::uint32_t recordSize = static_cast<std::uint32_t>(sizeof(Record));
    return GpuTracingSceneSectionLayout{kind,
                                        gpuTracingSceneLayoutVersion,
                                        recordCount,
                                        recordSize,
                                        static_cast<std::uint32_t>(alignof(Record)),
                                        byteOffset,
                                        recordCount * recordSize};
  }

  std::uint32_t byteCountFor(std::size_t value) {
    return value > std::numeric_limits<std::uint32_t>::max()
             ? std::numeric_limits<std::uint32_t>::max()
             : static_cast<std::uint32_t>(value);
  }

  std::array<float, 4> vector4(const Vector3d& vector, float w) {
    return {static_cast<float>(vector.x()), static_cast<float>(vector.y()),
            static_cast<float>(vector.z()), w};
  }

  std::array<float, 4> color4(const Colord& color) {
    return {static_cast<float>(color.r()), static_cast<float>(color.g()),
            static_cast<float>(color.b()), 1.0f};
  }

  void setUnsupportedReason(std::string* unsupportedReason, const char* reason) {
    if (unsupportedReason) {
      *unsupportedReason = reason;
    }
  }
}

std::array<GpuTracingSceneSectionLayout, 6> GpuTracingSceneSections::sectionLayouts() const {
  std::array<GpuTracingSceneSectionLayout, 6> layouts{};

  std::uint32_t offset = 0;
  const auto geometryBytes = byteCountFor(geometry.uploadByteCount());
  layouts[0] = GpuTracingSceneSectionLayout{GpuTracingSceneSectionKind::Geometry,
                                            gpuTracingSceneLayoutVersion,
                                            static_cast<std::uint32_t>(geometry.primitives.size()),
                                            0,
                                            16,
                                            offset,
                                            geometryBytes};
  offset += geometryBytes;

  layouts[1] = sectionLayout<GpuTracingMaterialRecord>(
    GpuTracingSceneSectionKind::Materials, static_cast<std::uint32_t>(materials.size()), offset);
  offset += layouts[1].byteCount;

  layouts[2] = sectionLayout<GpuTracingTextureRecord>(
    GpuTracingSceneSectionKind::Textures, static_cast<std::uint32_t>(textures.size()), offset);
  offset += layouts[2].byteCount;

  layouts[3] = sectionLayout<GpuTracingLightRecord>(
    GpuTracingSceneSectionKind::Lights, static_cast<std::uint32_t>(lights.size()), offset);
  offset += layouts[3].byteCount;

  layouts[4] = sectionLayout<GpuTracingEnvironmentRecord>(
    GpuTracingSceneSectionKind::Environment, static_cast<std::uint32_t>(environment.size()),
    offset);
  offset += layouts[4].byteCount;

  layouts[5] = sectionLayout<GpuTracingDebugIdRecord>(
    GpuTracingSceneSectionKind::DebugIds, static_cast<std::uint32_t>(debugIds.size()), offset);

  return layouts;
}

std::size_t GpuTracingSceneSections::uploadByteCount() const {
  return geometry.uploadByteCount() + materials.size() * sizeof(GpuTracingMaterialRecord) +
         textures.size() * sizeof(GpuTracingTextureRecord) +
         lights.size() * sizeof(GpuTracingLightRecord) +
         environment.size() * sizeof(GpuTracingEnvironmentRecord) +
         debugIds.size() * sizeof(GpuTracingDebugIdRecord);
}

bool GpuTracingTextureCompilation::fullySupported() const {
  return m_unsupportedTextures.empty();
}

const std::vector<GpuTracingTextureRecord>& GpuTracingTextureCompilation::records() const {
  return m_records;
}

const std::vector<UnsupportedGpuTracingTexture>&
GpuTracingTextureCompilation::unsupportedTextures() const {
  return m_unsupportedTextures;
}

std::vector<UnsupportedGpuTracingReasonCount>
GpuTracingTextureCompilation::unsupportedReasonCounts() const {
  std::vector<UnsupportedGpuTracingReasonCount> counts;
  std::map<std::string, std::size_t> countIndexByReason;

  for (const UnsupportedGpuTracingTexture& texture : m_unsupportedTextures) {
    const auto it = countIndexByReason.find(texture.reason);
    if (it == countIndexByReason.end()) {
      countIndexByReason[texture.reason] = counts.size();
      counts.push_back(UnsupportedGpuTracingReasonCount{texture.reason, 1});
    } else {
      counts[it->second].count++;
    }
  }

  return counts;
}

GpuTracingTextureCompilation
GpuTracingTextureCompiler::compile(const std::vector<std::shared_ptr<Texturec>>& textures) const {
  GpuTracingTextureCompilation compilation;
  compilation.m_records.reserve(textures.size());

  for (std::size_t textureIndex = 0; textureIndex < textures.size(); ++textureIndex) {
    const std::shared_ptr<Texturec>& texture = textures[textureIndex];

    if (!texture) {
      compilation.m_records.push_back(GpuTracingTextureRecord{});
      compilation.m_unsupportedTextures.push_back(
        UnsupportedGpuTracingTexture{static_cast<std::uint32_t>(textureIndex), std::string(),
                                     unsupportedGpuTracingNullTextureReason});
      continue;
    }

    const auto* constantColor = dynamic_cast<const ConstantColorTexture*>(texture.get());
    if (constantColor) {
      compilation.m_records.push_back(makeGpuTracingConstantColorTexture(constantColor->color()));
      continue;
    }

    compilation.m_records.push_back(GpuTracingTextureRecord{});
    compilation.m_unsupportedTextures.push_back(
      UnsupportedGpuTracingTexture{static_cast<std::uint32_t>(textureIndex), texture->name(),
                                   unsupportedGpuTracingTextureTypeReason});
  }

  return compilation;
}

GpuTracingTextureRecord render::makeGpuTracingConstantColorTexture(const Colord& color) {
  GpuTracingTextureRecord record;
  record.kind = static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor);
  record.parameters = {static_cast<float>(color.r()), static_cast<float>(color.g()),
                       static_cast<float>(color.b()), 1.0f};
  return record;
}

GpuTracingEnvironmentRecord render::makeGpuTracingConstantEnvironment(const Colord& color) {
  GpuTracingEnvironmentRecord record;
  record.color = color4(color);
  return record;
}

bool GpuTracingLightCompilation::supported() const {
  return unsupportedLights.empty();
}

std::vector<GpuTracingUnsupportedReasonCount>
GpuTracingLightCompilation::unsupportedReasonCounts() const {
  std::vector<GpuTracingUnsupportedReasonCount> result;
  for (const UnsupportedGpuTracingLight& unsupported : unsupportedLights) {
    const auto existing = std::find_if(
      result.begin(), result.end(), [&unsupported](const GpuTracingUnsupportedReasonCount& count) {
        return count.reason == unsupported.reason;
      });
    if (existing != result.end()) {
      ++existing->count;
    } else {
      result.push_back(GpuTracingUnsupportedReasonCount{unsupported.reason, 1});
    }
  }
  return result;
}

std::optional<GpuTracingLightRecord>
render::makeGpuTracingLightRecord(const Light& light, std::string* unsupportedReason) {
  if (const auto* pointLight = dynamic_cast<const PointLight*>(&light)) {
    GpuTracingLightRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Point);
    record.positionOrDirection = vector4(pointLight->position(), 1.0f);
    record.parameters = color4(pointLight->color());
    return record;
  }

  if (const auto* directionalLight = dynamic_cast<const DirectionalLight*>(&light)) {
    GpuTracingLightRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Directional);
    record.positionOrDirection = vector4(directionalLight->direction(), 0.0f);
    record.parameters = color4(directionalLight->color());
    return record;
  }

  if (const auto* areaLight = dynamic_cast<const RectangularAreaLight*>(&light)) {
    GpuTracingLightRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingLightKind::RectangularArea);
    record.positionOrDirection = vector4(areaLight->center(), 1.0f);
    record.u = vector4(areaLight->edgeU(), 0.0f);
    record.v = vector4(areaLight->edgeV(), 0.0f);
    record.parameters = color4(areaLight->color());
    return record;
  }

  setUnsupportedReason(unsupportedReason,
                       "light type is not supported by GPU tracing scene compiler");
  return std::nullopt;
}

GpuTracingLightCompilation render::compileGpuTracingLights(const Scene& scene) {
  GpuTracingLightCompilation compilation;

  std::uint32_t lightIndex = 0;
  for (const std::shared_ptr<render::Light>& light : scene.lights()) {
    std::string unsupportedReason;
    if (const std::optional<GpuTracingLightRecord> record =
          makeGpuTracingLightRecord(*light, &unsupportedReason)) {
      compilation.records.push_back(*record);
    } else {
      compilation.unsupportedLights.push_back(
        UnsupportedGpuTracingLight{lightIndex, light->fingerprintType(), unsupportedReason});
    }

    if (lightIndex != std::numeric_limits<std::uint32_t>::max()) {
      ++lightIndex;
    }
  }

  return compilation;
}
