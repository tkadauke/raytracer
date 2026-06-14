#include "render/GpuTracingScene.h"

#include "render/IntersectionSceneCompiler.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/lights/PointLight.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/Texture.h"

#include <algorithm>
#include <limits>
#include <map>
#include <type_traits>
#include <typeinfo>

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

  template<typename Unsupported>
  std::vector<GpuTracingUnsupportedReasonCount>
  unsupportedReasonCountsFor(const std::vector<Unsupported>& unsupportedItems) {
    std::vector<GpuTracingUnsupportedReasonCount> result;
    for (const Unsupported& unsupported : unsupportedItems) {
      const auto existing =
        std::find_if(result.begin(), result.end(),
                     [&unsupported](const GpuTracingUnsupportedReasonCount& count) {
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

  const char* materialTypeName(const Material& material) {
    if (typeid(material) == typeid(MatteMaterial)) {
      return "MatteMaterial";
    }
    if (typeid(material) == typeid(EmissiveMaterial)) {
      return "EmissiveMaterial";
    }
    return "Material";
  }

  const char* textureTypeName(const Texturec& texture) {
    if (dynamic_cast<const ConstantColorTexture*>(&texture)) {
      return "ConstantColorTexture";
    }
    return "Texture";
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
  return unsupportedReasonCountsFor(unsupportedLights);
}

bool GpuTracingTextureCompilation::supported() const {
  return unsupportedTextures.empty();
}

std::vector<GpuTracingUnsupportedReasonCount>
GpuTracingTextureCompilation::unsupportedReasonCounts() const {
  return unsupportedReasonCountsFor(unsupportedTextures);
}

bool GpuTracingMaterialCompilation::supported() const {
  return unsupportedMaterials.empty() && textures.supported();
}

std::vector<GpuTracingUnsupportedReasonCount>
GpuTracingMaterialCompilation::unsupportedReasonCounts() const {
  return unsupportedReasonCountsFor(unsupportedMaterials);
}

std::optional<GpuTracingTextureRecord>
render::makeGpuTracingTextureRecord(const Texturec& texture, std::string* unsupportedReason) {
  if (const auto* constantColor = dynamic_cast<const ConstantColorTexture*>(&texture)) {
    GpuTracingTextureRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor);
    record.parameters = color4(constantColor->color());
    return record;
  }

  setUnsupportedReason(unsupportedReason,
                       "texture type is not supported by GPU tracing scene compiler");
  return std::nullopt;
}

std::optional<GpuTracingMaterialRecord>
render::makeGpuTracingMaterialRecord(const Material& material, std::uint32_t albedoTexture,
                                     std::uint32_t emissionTexture,
                                     std::string* unsupportedReason) {
  if (typeid(material) == typeid(MatteMaterial)) {
    const auto& matte = static_cast<const MatteMaterial&>(material);
    if (matte.normalTexture()) {
      setUnsupportedReason(unsupportedReason,
                           "matte normal texture is not supported by GPU tracing scene compiler");
      return std::nullopt;
    }

    GpuTracingMaterialRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte);
    record.albedoTexture = albedoTexture;
    record.parameters = {static_cast<float>(matte.ambientCoefficient()),
                         static_cast<float>(matte.diffuseCoefficient()), 0.0f, 0.0f};
    return record;
  }

  if (typeid(material) == typeid(EmissiveMaterial)) {
    GpuTracingMaterialRecord record;
    record.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Emissive);
    record.emissionTexture = emissionTexture;
    return record;
  }

  setUnsupportedReason(unsupportedReason,
                       "material type is not supported by GPU tracing scene compiler");
  return std::nullopt;
}

GpuTracingMaterialCompilation
render::compileGpuTracingMaterials(const CompiledIntersectionScene& scene) {
  GpuTracingMaterialCompilation compilation;
  if (!scene.materials().empty()) {
    compilation.records.resize(scene.materials().size());
    compilation.textures.records.push_back(GpuTracingTextureRecord{});
  }

  std::map<const Texturec*, std::uint32_t> textureIds;
  auto appendConstantColorTexture = [&compilation](const Colord& color) {
    const std::uint32_t id = static_cast<std::uint32_t>(compilation.textures.records.size());
    compilation.textures.records.push_back(
      *makeGpuTracingTextureRecord(ConstantColorTexture(color)));
    return id;
  };

  auto textureIdFor = [&compilation, &textureIds](const std::shared_ptr<Texturec>& texture) {
    if (!texture) {
      return 0u;
    }

    const auto existing = textureIds.find(texture.get());
    if (existing != textureIds.end()) {
      return existing->second;
    }

    const std::uint32_t id = static_cast<std::uint32_t>(compilation.textures.records.size());
    std::string reason;
    if (const std::optional<GpuTracingTextureRecord> record =
          makeGpuTracingTextureRecord(*texture, &reason)) {
      compilation.textures.records.push_back(*record);
    } else {
      compilation.textures.records.push_back(GpuTracingTextureRecord{});
      compilation.textures.unsupportedTextures.push_back(
        UnsupportedGpuTracingTexture{id, textureTypeName(*texture), reason});
    }
    textureIds.emplace(texture.get(), id);
    return id;
  };

  for (std::uint32_t materialId = 1; materialId < scene.materials().size(); ++materialId) {
    const std::shared_ptr<Material>& material = scene.materials()[materialId];
    if (!material) {
      continue;
    }

    std::uint32_t albedoTexture = 0;
    std::uint32_t emissionTexture = 0;
    if (typeid(*material) == typeid(MatteMaterial)) {
      const auto& matte = static_cast<const MatteMaterial&>(*material);
      albedoTexture = textureIdFor(matte.diffuseTexture());
    } else if (typeid(*material) == typeid(EmissiveMaterial)) {
      const auto& emissive = static_cast<const EmissiveMaterial&>(*material);
      emissionTexture = appendConstantColorTexture(emissive.radiance());
    }

    std::string reason;
    if (const std::optional<GpuTracingMaterialRecord> record =
          makeGpuTracingMaterialRecord(*material, albedoTexture, emissionTexture, &reason)) {
      compilation.records[materialId] = *record;
    } else {
      compilation.records[materialId] = GpuTracingMaterialRecord{};
      compilation.unsupportedMaterials.push_back(
        UnsupportedGpuTracingMaterial{materialId, materialTypeName(*material), reason});
    }
  }

  return compilation;
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
