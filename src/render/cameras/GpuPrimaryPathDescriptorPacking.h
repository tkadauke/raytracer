#pragma once

#include "render/GpuPrimaryPathDescriptor.h"
#include "core/math/Vector.h"

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace render::detail {
  inline std::uint32_t checkedU32(std::uint64_t value, const char* label) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
      throw std::overflow_error(std::string(label) + " exceeds GPU 32-bit count range");
    }
    return static_cast<std::uint32_t>(value);
  }

  inline std::array<float, 4> gpuFloat4(double x, double y = 0.0, double z = 0.0,
                                        double w = 0.0) {
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
            static_cast<float>(w)};
  }

  inline std::array<float, 4> vector4(const Vector3d& value, float w) {
    return value.toFloat4(w);
  }

  inline std::array<float, 4> parameters4(double x, double y, double z = 0.0, double w = 0.0) {
    return gpuFloat4(x, y, z, w);
  }

  inline void checkGpuPathCount(const Recti& actual, int numSamples) {
    const std::uint64_t pixelCount =
      static_cast<std::uint64_t>(actual.width()) * static_cast<std::uint64_t>(actual.height());
    const std::uint64_t pathCount = pixelCount * static_cast<std::uint64_t>(numSamples);
    if (pixelCount != 0 && pathCount / pixelCount != static_cast<std::uint64_t>(numSamples)) {
      throw std::overflow_error("GPU camera primary path count overflows");
    }
    (void)checkedU32(pathCount, "GPU camera primary path count");
  }

  inline void fillGpuDescriptorViewport(render::GpuRectilinearPrimaryPathDescriptor& rec,
                                         const Recti& rect, const Recti& actual, int numSamples,
                                         std::uint32_t sampleSeed) {
    rec.requestedLeft = rect.left();
    rec.requestedTop = rect.top();
    rec.requestedWidth =
      checkedU32(static_cast<std::uint64_t>(rect.width()), "GPU camera requested width");
    rec.requestedHeight =
      checkedU32(static_cast<std::uint64_t>(rect.height()), "GPU camera requested height");
    rec.actualLeft = actual.left();
    rec.actualTop = actual.top();
    rec.actualWidth =
      checkedU32(static_cast<std::uint64_t>(actual.width()), "GPU camera actual width");
    rec.actualHeight =
      checkedU32(static_cast<std::uint64_t>(actual.height()), "GPU camera actual height");
    rec.samplesPerPixel =
      checkedU32(static_cast<std::uint64_t>(numSamples), "GPU camera samples per pixel");
    rec.sampleSeed = sampleSeed;
  }
}
