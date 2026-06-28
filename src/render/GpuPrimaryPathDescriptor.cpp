#include "render/GpuPrimaryPathDescriptor.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace render {
  namespace {
    bool isRectangularPixelDomainMode(std::uint32_t mode) {
      return mode == gpuPrimaryPathGenerationModePinhole ||
             mode == gpuPrimaryPathGenerationModeOrthographic ||
             mode == gpuPrimaryPathGenerationModeThinLens ||
             mode == gpuPrimaryPathGenerationModeEquirectangular ||
             mode == gpuPrimaryPathGenerationModeSpherical ||
             mode == gpuPrimaryPathGenerationModeFishEye ||
             mode == gpuPrimaryPathGenerationModeTiltShift;
    }

    std::uint64_t checkedProduct(std::uint64_t left, std::uint64_t right, const char* label) {
      if (right != 0 && left > std::numeric_limits<std::uint64_t>::max() / right) {
        throw std::overflow_error(std::string(label) + " count overflows");
      }
      return left * right;
    }
  }

  bool GpuPrimaryPathDescriptor::generatesOnDevice() const {
    return mode != gpuPrimaryPathGenerationModeHostPathStates;
  }

  Recti GpuPrimaryPathDescriptor::requestedRect() const {
    if (isRectangularPixelDomainMode(mode)) {
      return Recti(rectilinear.requestedLeft, rectilinear.requestedTop,
                   static_cast<int>(rectilinear.requestedWidth),
                   static_cast<int>(rectilinear.requestedHeight));
    }
    return Recti();
  }

  Recti GpuPrimaryPathDescriptor::actualRect() const {
    if (isRectangularPixelDomainMode(mode)) {
      return Recti(rectilinear.actualLeft, rectilinear.actualTop,
                   static_cast<int>(rectilinear.actualWidth),
                   static_cast<int>(rectilinear.actualHeight));
    }
    return Recti();
  }

  std::uint64_t GpuPrimaryPathDescriptor::pathCount() const {
    if (isRectangularPixelDomainMode(mode)) {
      const std::uint64_t pixelCount =
        checkedProduct(rectilinear.actualWidth, rectilinear.actualHeight, "GPU primary pixel");
      return checkedProduct(pixelCount, rectilinear.samplesPerPixel, "GPU primary path");
    }
    return 0;
  }

  GpuPrimaryPathDescriptor
  GpuPrimaryPathDescriptor::withSampleRange(std::uint32_t firstSample,
                                            std::uint32_t sampleCount) const {
    if (!isRectangularPixelDomainMode(mode)) {
      throw std::invalid_argument("GPU primary sample ranges require a rectangular descriptor");
    }
    if (sampleCount == 0u) {
      throw std::invalid_argument("GPU primary sample range requires a positive sample count");
    }
    const std::uint64_t currentBegin = rectilinear.sampleOffset;
    const std::uint64_t currentEnd = currentBegin + rectilinear.samplesPerPixel;
    const std::uint64_t requestedBegin = firstSample;
    const std::uint64_t requestedEnd = requestedBegin + sampleCount;
    if (requestedBegin < currentBegin || requestedEnd > currentEnd) {
      throw std::out_of_range("GPU primary sample range is outside the descriptor sample domain");
    }

    GpuPrimaryPathDescriptor result = *this;
    result.rectilinear.sampleOffset = firstSample;
    result.rectilinear.samplesPerPixel = sampleCount;
    return result;
  }

  GpuPrimaryPathDescriptor GpuPrimaryPathDescriptor::withActualRect(const Recti& rect) const {
    if (!isRectangularPixelDomainMode(mode)) {
      throw std::invalid_argument("GPU primary actual rects require a rectangular descriptor");
    }
    if (rect.width() <= 0 || rect.height() <= 0) {
      throw std::invalid_argument("GPU primary actual rect requires positive dimensions");
    }

    const Recti requested = requestedRect();
    if (rect.left() < requested.left() || rect.top() < requested.top() ||
        rect.right() > requested.right() || rect.bottom() > requested.bottom()) {
      throw std::out_of_range("GPU primary actual rect is outside the descriptor request domain");
    }

    GpuPrimaryPathDescriptor result = *this;
    result.rectilinear.actualLeft = rect.left();
    result.rectilinear.actualTop = rect.top();
    result.rectilinear.actualWidth =
      static_cast<std::uint32_t>(static_cast<std::uint64_t>(rect.width()));
    result.rectilinear.actualHeight =
      static_cast<std::uint32_t>(static_cast<std::uint64_t>(rect.height()));
    return result;
  }
}
