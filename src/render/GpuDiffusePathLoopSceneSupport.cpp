#include "render/GpuDiffusePathLoopSceneSupport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace render {
  namespace {
    [[nodiscard]] bool payloadInRange(std::uint32_t payloadOffset, std::uint32_t payloadCount,
                                      std::size_t payloadSize) {
      return payloadCount == 1u && payloadOffset < payloadSize;
    }
  }

  GpuDiffusePathLoopBackendSupport GpuDiffusePathLoopSceneSupport::support(
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings,
    const GpuDiffusePathLoopSceneSupportReasons& reasons) const {
    if (settings.maxDepth == 0u) {
      return {false, reasons.maxDepth};
    }
    if (!hasNoGeometry(scene) && !hasSupportedGeometry(scene)) {
      return {false, reasons.geometry};
    }
    if (!hasSupportedMaterials(scene)) {
      return {false, reasons.material};
    }
    if (!hasSupportedTextures(scene)) {
      return {false, reasons.texture};
    }
    if (!hasSupportedLights(scene)) {
      return {false, reasons.light};
    }
    return {true, {}};
  }

  bool GpuDiffusePathLoopSceneSupport::hasNoGeometry(const GpuTracingSceneSections& scene) const {
    const GpuIntersectionSceneBuffers& geometry = scene.geometry;
    return geometry.primitives.empty() && geometry.bvh.empty();
  }

  bool GpuDiffusePathLoopSceneSupport::primitiveUsesSupportedGeometry(
    const GpuIntersectionPrimitiveRecord& primitive, const GpuIntersectionSceneBuffers& geometry,
    SupportedGeometryCounts& counts) const {
    if (primitive.transform != 0u && primitive.transform >= geometry.transforms.size()) {
      return false;
    }

    switch (static_cast<GpuIntersectionPrimitiveKind>(primitive.kind)) {
    case GpuIntersectionPrimitiveKind::Triangle:
      if (!payloadInRange(primitive.payloadOffset, primitive.payloadCount,
                          geometry.triangles.size())) {
        return false;
      }
      ++counts.triangles;
      return true;
    case GpuIntersectionPrimitiveKind::Sphere:
      if (!payloadInRange(primitive.payloadOffset, primitive.payloadCount,
                          geometry.spheres.size())) {
        return false;
      }
      ++counts.spheres;
      return true;
    case GpuIntersectionPrimitiveKind::Plane:
      if (!payloadInRange(primitive.payloadOffset, primitive.payloadCount,
                          geometry.planes.size())) {
        return false;
      }
      ++counts.planes;
      return true;
    case GpuIntersectionPrimitiveKind::Rectangle:
      if (!payloadInRange(primitive.payloadOffset, primitive.payloadCount,
                          geometry.rectangles.size())) {
        return false;
      }
      ++counts.rectangles;
      return true;
    case GpuIntersectionPrimitiveKind::Disk:
      if (!payloadInRange(primitive.payloadOffset, primitive.payloadCount, geometry.disks.size())) {
        return false;
      }
      ++counts.disks;
      return true;
    case GpuIntersectionPrimitiveKind::OpenCylinder:
      if (!payloadInRange(primitive.payloadOffset, primitive.payloadCount,
                          geometry.openCylinders.size())) {
        return false;
      }
      ++counts.openCylinders;
      return true;
    case GpuIntersectionPrimitiveKind::Torus:
      if (!payloadInRange(primitive.payloadOffset, primitive.payloadCount, geometry.tori.size())) {
        return false;
      }
      ++counts.tori;
      return true;
    case GpuIntersectionPrimitiveKind::Unsupported:
      return false;
    }
    return false;
  }

  bool
  GpuDiffusePathLoopSceneSupport::hasSupportedGeometry(const GpuTracingSceneSections& scene) const {
    const GpuIntersectionSceneBuffers& geometry = scene.geometry;
    if (geometry.primitives.empty() || geometry.bvh.empty()) {
      return false;
    }

    SupportedGeometryCounts counts;
    for (const GpuIntersectionPrimitiveRecord& primitive : geometry.primitives) {
      if (!primitiveUsesSupportedGeometry(primitive, geometry, counts)) {
        return false;
      }
    }

    return counts.triangles == geometry.triangles.size() &&
           counts.spheres == geometry.spheres.size() && counts.planes == geometry.planes.size() &&
           counts.rectangles == geometry.rectangles.size() &&
           counts.disks == geometry.disks.size() &&
           counts.openCylinders == geometry.openCylinders.size() &&
           counts.tori == geometry.tori.size();
  }

  bool GpuDiffusePathLoopSceneSupport::hasSupportedMaterials(
    const GpuTracingSceneSections& scene) const {
    for (std::size_t index = 0; index != scene.materials.size(); ++index) {
      const auto kind = static_cast<GpuTracingMaterialKind>(scene.materials[index].kind);
      if (kind == GpuTracingMaterialKind::Unsupported) {
        if (index == 0u) {
          continue;
        }
        return false;
      }
      if (kind != GpuTracingMaterialKind::Matte && kind != GpuTracingMaterialKind::Phong &&
          kind != GpuTracingMaterialKind::Reflective &&
          kind != GpuTracingMaterialKind::Transparent && kind != GpuTracingMaterialKind::Emissive &&
          kind != GpuTracingMaterialKind::Portal) {
        return false;
      }
    }
    return true;
  }

  bool GpuDiffusePathLoopSceneSupport::supportedUntintedTexture(
    const GpuTracingSceneSections& scene, std::size_t textureIndex, std::uint32_t depth) const {
    const GpuTracingTextureRecord& texture = scene.textures[textureIndex];
    const auto kind = static_cast<GpuTracingTextureKind>(texture.kind);
    if (kind == GpuTracingTextureKind::Unsupported) {
      return textureIndex == 0u && depth == 0u;
    }
    if (kind == GpuTracingTextureKind::ConstantColor) {
      return true;
    }
    if (kind == GpuTracingTextureKind::UVColor) {
      return true;
    }
    if (kind == GpuTracingTextureKind::CheckerBoard) {
      const auto mapping =
        static_cast<GpuTracingTextureMappingKind>(texture.flags & gpuTracingTextureMappingMask);
      if (mapping != GpuTracingTextureMappingKind::Planar &&
          mapping != GpuTracingTextureMappingKind::UV) {
        return false;
      }
      if (texture.payloadOffset >= scene.textures.size() ||
          texture.payloadCount >= scene.textures.size()) {
        return false;
      }
      return supportedTexture(scene, texture.payloadOffset, depth + 1u) &&
             supportedTexture(scene, texture.payloadCount, depth + 1u);
    }
    if (kind == GpuTracingTextureKind::Image) {
      const auto mapping =
        static_cast<GpuTracingTextureMappingKind>(texture.flags & gpuTracingTextureMappingMask);
      if (mapping != GpuTracingTextureMappingKind::Planar &&
          mapping != GpuTracingTextureMappingKind::UV) {
        return false;
      }
      const std::uint32_t width = static_cast<std::uint32_t>(std::round(texture.parameters[2]));
      const std::uint32_t height = static_cast<std::uint32_t>(std::round(texture.parameters[3]));
      const std::uint64_t texelCount =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
      if (width == 0u || height == 0u || texture.payloadCount != texelCount ||
          texture.payloadOffset >= scene.textures.size() ||
          static_cast<std::uint64_t>(texture.payloadOffset) + texture.payloadCount >
            scene.textures.size()) {
        return false;
      }
      for (std::uint32_t offset = 0; offset != texture.payloadCount; ++offset) {
        if (static_cast<GpuTracingTextureKind>(
              scene.textures[texture.payloadOffset + offset].kind) !=
            GpuTracingTextureKind::ConstantColor) {
          return false;
        }
      }
      return true;
    }
    return false;
  }

  bool GpuDiffusePathLoopSceneSupport::supportedTexture(const GpuTracingSceneSections& scene,
                                                        std::size_t textureIndex,
                                                        std::uint32_t depth) const {
    constexpr std::uint32_t maxTextureEvaluationDepth = 8u;
    if (depth >= maxTextureEvaluationDepth) {
      return false;
    }
    if (textureIndex >= scene.textures.size()) {
      return false;
    }
    const GpuTracingTextureRecord& texture = scene.textures[textureIndex];
    const auto kind = static_cast<GpuTracingTextureKind>(texture.kind);
    if (kind == GpuTracingTextureKind::Tinted) {
      return texture.payloadOffset < scene.textures.size() &&
             supportedTexture(scene, texture.payloadOffset, depth + 1u);
    }
    return supportedUntintedTexture(scene, textureIndex, depth);
  }

  bool
  GpuDiffusePathLoopSceneSupport::hasSupportedTextures(const GpuTracingSceneSections& scene) const {
    for (std::size_t index = 0; index != scene.textures.size(); ++index) {
      if (!supportedTexture(scene, index)) {
        return false;
      }
    }
    return true;
  }

  bool
  GpuDiffusePathLoopSceneSupport::hasSupportedLights(const GpuTracingSceneSections& scene) const {
    return std::all_of(
      scene.lights.begin(), scene.lights.end(), [](const GpuTracingLightRecord& light) {
        const auto kind = static_cast<GpuTracingLightKind>(light.kind);
        return kind == GpuTracingLightKind::Point || kind == GpuTracingLightKind::Directional ||
               kind == GpuTracingLightKind::RectangularArea;
      });
  }
}
