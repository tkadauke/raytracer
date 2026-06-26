#include "render/GpuTracingTextureEvaluator.h"

#include "render/GpuTracingScene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace render {
  GpuTracingTextureEvaluator::GpuTracingTextureEvaluator(const GpuTracingSceneSections& scene)
      : m_scene(scene) {
  }

  Colord GpuTracingTextureEvaluator::evaluate(std::uint32_t textureId,
                                              const GpuIntersectionHitRecord& hit) const {
    return evaluate(textureId, hit, 0);
  }

  Colord GpuTracingTextureEvaluator::imageTexelColor(const GpuTracingTextureRecord& texture, int x,
                                                     int y, int width,
                                                     const GpuIntersectionHitRecord& hit,
                                                     std::uint32_t depth) const {
    const std::uint32_t texelTexture =
      texture.payloadOffset + static_cast<std::uint32_t>(y * width + x);
    return evaluate(texelTexture, hit, depth + 1);
  }

  Colord GpuTracingTextureEvaluator::evaluate(std::uint32_t textureId,
                                              const GpuIntersectionHitRecord& hit,
                                              std::uint32_t depth) const {
    if (textureId >= m_scene.textures.size()) {
      return Colord::black();
    }
    if (depth >= maxEvaluationDepth) {
      return Colord::black();
    }

    const GpuTracingTextureRecord& texture = m_scene.textures[textureId];
    const auto kind = static_cast<GpuTracingTextureKind>(texture.kind);
    if (kind == GpuTracingTextureKind::ConstantColor) {
      return Colord(texture.parameters);
    }

    if (kind == GpuTracingTextureKind::UVColor) {
      return Colord(hit.uv[0], hit.uv[1], 0.0);
    }

    if (kind == GpuTracingTextureKind::Tinted) {
      return evaluate(texture.payloadOffset, hit, depth + 1) * Colord(texture.parameters);
    }

    if (kind == GpuTracingTextureKind::CheckerBoard) {
      const Vector2d st = textureCoordinates(texture, hit);
      const int parity =
        static_cast<int>(std::floor(st.x())) + static_cast<int>(std::floor(st.y()));
      const std::uint32_t childTexture =
        parity % 2 == 0 ? texture.payloadOffset : texture.payloadCount;
      return evaluate(childTexture, hit, depth + 1);
    }

    if (kind == GpuTracingTextureKind::Image) {
      const int width = static_cast<int>(std::round(texture.parameters[2]));
      const int height = static_cast<int>(std::round(texture.parameters[3]));
      const std::uint64_t texelCount =
        width > 0 && height > 0
          ? static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height)
          : 0u;
      if (width <= 0 || height <= 0 || texture.payloadOffset >= m_scene.textures.size() ||
          texture.payloadCount != texelCount ||
          static_cast<std::uint64_t>(texture.payloadOffset) + texture.payloadCount >
            m_scene.textures.size()) {
        return Colord::black();
      }

      const Vector2d st = textureCoordinates(texture, hit);
      if ((texture.flags & gpuTracingTextureFilterBilinearFlag) != 0u) {
        const double x = normalizedTextureCoordinate(texture, st.x()) * width - 0.5;
        const double y = normalizedTextureCoordinate(texture, st.y()) * height - 0.5;
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const double tx = x - x0;
        const double ty = y - y0;

        const Colord c00 =
          imageTexelColor(texture, imageTextureWrappedCoordinate(texture, x0, width),
                          imageTextureWrappedCoordinate(texture, y0, height), width, hit, depth);
        const Colord c10 =
          imageTexelColor(texture, imageTextureWrappedCoordinate(texture, x0 + 1, width),
                          imageTextureWrappedCoordinate(texture, y0, height), width, hit, depth);
        const Colord c01 = imageTexelColor(
          texture, imageTextureWrappedCoordinate(texture, x0, width),
          imageTextureWrappedCoordinate(texture, y0 + 1, height), width, hit, depth);
        const Colord c11 = imageTexelColor(
          texture, imageTextureWrappedCoordinate(texture, x0 + 1, width),
          imageTextureWrappedCoordinate(texture, y0 + 1, height), width, hit, depth);

        return lerpColor(lerpColor(c00, c10, tx), lerpColor(c01, c11, tx), ty);
      }

      const int x = imageTextureCoordinate(texture, st.x(), width);
      const int y = imageTextureCoordinate(texture, st.y(), height);
      return imageTexelColor(texture, x, y, width, hit, depth);
    }

    return Colord::black();
  }

  Vector2d GpuTracingTextureEvaluator::textureCoordinates(const GpuTracingTextureRecord& texture,
                                                          const GpuIntersectionHitRecord& hit) {
    const auto mapping =
      static_cast<GpuTracingTextureMappingKind>(texture.flags & gpuTracingTextureMappingMask);
    if (mapping == GpuTracingTextureMappingKind::UV) {
      return Vector2d(hit.uv[0] * texture.parameters[0], hit.uv[1] * texture.parameters[1]);
    }
    if (mapping == GpuTracingTextureMappingKind::Planar) {
      return Vector2d(hit.point[0], hit.point[2]);
    }
    return Vector2d::null;
  }

  double
  GpuTracingTextureEvaluator::normalizedTextureCoordinate(const GpuTracingTextureRecord& texture,
                                                          double coordinate) {
    if ((texture.flags & gpuTracingTextureWrapClampFlag) != 0u) {
      return std::clamp(coordinate, 0.0, 1.0);
    }
    return coordinate - std::floor(coordinate);
  }

  int GpuTracingTextureEvaluator::imageTextureWrappedCoordinate(
    const GpuTracingTextureRecord& texture, int coordinate, int size) {
    if ((texture.flags & gpuTracingTextureWrapClampFlag) != 0u) {
      return std::clamp(coordinate, 0, size - 1);
    }
    int wrapped = coordinate % size;
    if (wrapped < 0) {
      wrapped += size;
    }
    return wrapped;
  }

  int GpuTracingTextureEvaluator::imageTextureCoordinate(const GpuTracingTextureRecord& texture,
                                                         double coordinate, int size) {
    const int result =
      static_cast<int>(std::floor(normalizedTextureCoordinate(texture, coordinate) * size));
    return imageTextureWrappedCoordinate(texture, result, size);
  }

  Colord GpuTracingTextureEvaluator::lerpColor(const Colord& a, const Colord& b, double t) {
    return a * (1.0 - t) + b * t;
  }
}
