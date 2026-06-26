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
             mode == gpuPrimaryPathGenerationModeEquirectangular;
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
}
