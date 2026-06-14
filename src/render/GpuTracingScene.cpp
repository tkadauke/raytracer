#include "render/GpuTracingScene.h"

#include "render/IntersectionSceneCompiler.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"

#include <algorithm>
#include <limits>
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

  std::array<float, 4> packColor(const Colord& color, float alpha = 1.0f) {
    return {static_cast<float>(color.r()), static_cast<float>(color.g()),
            static_cast<float>(color.b()), alpha};
  }
}

bool GpuTracingMaterialCompilation::fullySupported() const {
  return unsupportedMaterials.empty();
}

std::vector<GpuTracingUnsupportedReasonCount>
GpuTracingMaterialCompilation::unsupportedReasonCounts() const {
  std::vector<GpuTracingUnsupportedReasonCount> result;
  for (const UnsupportedGpuTracingMaterial& unsupported : unsupportedMaterials) {
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

GpuTracingMaterialCompilation
GpuTracingMaterialCompiler::compile(const std::vector<std::shared_ptr<Material>>& materials) const {
  GpuTracingMaterialCompilation result;
  result.records.reserve(materials.size());
  for (std::size_t materialIndex = 0; materialIndex != materials.size(); ++materialIndex) {
    const auto materialId = static_cast<std::uint32_t>(materialIndex);
    const std::shared_ptr<Material>& material = materials[materialIndex];
    if (!material) {
      result.records.push_back(GpuTracingMaterialRecord{});
      continue;
    }

    result.records.push_back(compileRecord(*material));
    if (result.records.back().kind ==
        static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported)) {
      result.unsupportedMaterials.push_back(
        UnsupportedGpuTracingMaterial{materialId, unsupportedReason(*material)});
    }
  }
  return result;
}

GpuTracingMaterialCompilation
GpuTracingMaterialCompiler::compile(const CompiledIntersectionScene& scene) const {
  return compile(scene.materials());
}

GpuTracingMaterialRecord GpuTracingMaterialCompiler::compileRecord(const Material& material) const {
  GpuTracingMaterialRecord record;
  if (typeid(material) == typeid(MatteMaterial)) {
    const auto& matte = static_cast<const MatteMaterial&>(material);
    record.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte);
    record.parameters = {static_cast<float>(matte.ambientCoefficient()),
                         static_cast<float>(matte.diffuseCoefficient()), 0.0f, 0.0f};
  } else if (typeid(material) == typeid(EmissiveMaterial)) {
    const auto& emissive = static_cast<const EmissiveMaterial&>(material);
    record.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Emissive);
    record.parameters = packColor(emissive.radiance());
  }
  return record;
}

std::string GpuTracingMaterialCompiler::unsupportedReason(const Material&) const {
  return "material is outside the GPU tracing Matte/Emissive subset";
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
  record.color = packColor(color);
  return record;
}
