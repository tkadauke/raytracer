#include "render/GpuDiffusePathLoopSceneSupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace render {
  namespace {
    [[nodiscard]] bool payloadInRange(std::uint32_t payloadOffset, std::uint32_t payloadCount,
                                      std::size_t payloadSize) {
      return payloadCount == 1u && payloadOffset < payloadSize;
    }

    [[nodiscard]] bool rangeInSize(std::uint32_t first, std::uint32_t count, std::size_t size) {
      return first <= size && count <= size - first;
    }

    [[nodiscard]] bool allFinite(const std::array<float, 4>& values) {
      return std::all_of(values.begin(), values.end(),
                         [](float value) { return std::isfinite(value); });
    }
  }

  GpuDiffusePathLoopBackendSupport GpuDiffusePathLoopSceneSupport::support(
    const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings,
    const GpuDiffusePathLoopSceneSupportReasons& reasons) const {
    if (settings.maxDepth == 0u) {
      return {false, reasons.maxDepth};
    }
    if (settings.captureResolvedDisplay &&
        settings.displayResolveTonemap == GpuDisplayResolveTonemap::Unsupported) {
      return {false, reasons.displayResolve};
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
    const GpuIntersectionPrimitiveRecord& primitive, const GpuTracingSceneSections& scene,
    SupportedGeometryCounts& counts) const {
    const GpuIntersectionSceneBuffers& geometry = scene.geometry;
    if (primitive.material >= scene.materials.size() ||
        static_cast<GpuTracingMaterialKind>(scene.materials[primitive.material].kind) ==
          GpuTracingMaterialKind::Unsupported) {
      return false;
    }
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
    if (!bvhUsesSupportedGeometry(geometry)) {
      return false;
    }

    SupportedGeometryCounts counts;
    for (const GpuIntersectionPrimitiveRecord& primitive : geometry.primitives) {
      if (!primitiveUsesSupportedGeometry(primitive, scene, counts)) {
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

  bool GpuDiffusePathLoopSceneSupport::bvhUsesSupportedGeometry(
    const GpuIntersectionSceneBuffers& geometry) const {
    for (std::size_t nodeIndex = 0; nodeIndex != geometry.bvh.size(); ++nodeIndex) {
      const GpuIntersectionBvhNode& node = geometry.bvh[nodeIndex];
      const bool leaf = (node.flags & gpuIntersectionLeafNodeFlag) != 0u;
      if (leaf) {
        if (node.primitiveCount == 0u ||
            !rangeInSize(node.leftOrFirstPrimitive, node.primitiveCount,
                         geometry.primitives.size())) {
          return false;
        }
        continue;
      }

      if (node.leftOrFirstPrimitive >= geometry.bvh.size() ||
          node.primitiveCount >= geometry.bvh.size() || node.leftOrFirstPrimitive <= nodeIndex ||
          node.primitiveCount <= nodeIndex || node.leftOrFirstPrimitive == node.primitiveCount) {
        return false;
      }
    }
    return true;
  }

  bool GpuDiffusePathLoopSceneSupport::hasSupportedMaterials(
    const GpuTracingSceneSections& scene) const {
    for (std::size_t index = 0; index != scene.materials.size(); ++index) {
      const GpuTracingMaterialRecord& material = scene.materials[index];
      const auto kind = static_cast<GpuTracingMaterialKind>(material.kind);
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
      if (!materialUsesSupportedTextures(scene, material)) {
        return false;
      }
    }
    return true;
  }

  bool GpuDiffusePathLoopSceneSupport::materialUsesSupportedTextures(
    const GpuTracingSceneSections& scene, const GpuTracingMaterialRecord& material) const {
    const auto kind = static_cast<GpuTracingMaterialKind>(material.kind);
    switch (kind) {
    case GpuTracingMaterialKind::Matte:
    case GpuTracingMaterialKind::Phong:
    case GpuTracingMaterialKind::Reflective:
    case GpuTracingMaterialKind::Transparent:
      return supportedMaterialTexture(scene, material.albedoTexture);
    case GpuTracingMaterialKind::Emissive:
      return supportedMaterialTexture(scene, material.emissionTexture);
    case GpuTracingMaterialKind::Portal:
      return true;
    case GpuTracingMaterialKind::Unsupported:
      return false;
    }
    return false;
  }

  bool
  GpuDiffusePathLoopSceneSupport::supportedMaterialTexture(const GpuTracingSceneSections& scene,
                                                           std::size_t textureIndex) const {
    if (textureIndex >= scene.textures.size()) {
      return false;
    }
    if (static_cast<GpuTracingTextureKind>(scene.textures[textureIndex].kind) ==
        GpuTracingTextureKind::Unsupported) {
      return false;
    }
    return supportedTexture(scene, textureIndex);
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
        if (kind == GpuTracingLightKind::Point || kind == GpuTracingLightKind::Directional) {
          return allFinite(light.positionOrDirection) && allFinite(light.parameters);
        }
        if (kind == GpuTracingLightKind::RectangularArea) {
          return allFinite(light.positionOrDirection) && allFinite(light.u) && allFinite(light.v) &&
                 allFinite(light.parameters);
        }
        return false;
      });
  }
}
