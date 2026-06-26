#include "render/GpuPrimaryPathDescriptor.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace render {
  namespace {
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
    if (mode == gpuPrimaryPathGenerationModePinhole) {
      return Recti(pinhole.requestedLeft, pinhole.requestedTop,
                   static_cast<int>(pinhole.requestedWidth),
                   static_cast<int>(pinhole.requestedHeight));
    }
    return Recti();
  }

  Recti GpuPrimaryPathDescriptor::actualRect() const {
    if (mode == gpuPrimaryPathGenerationModePinhole) {
      return Recti(pinhole.actualLeft, pinhole.actualTop, static_cast<int>(pinhole.actualWidth),
                   static_cast<int>(pinhole.actualHeight));
    }
    return Recti();
  }

  std::uint64_t GpuPrimaryPathDescriptor::pathCount() const {
    if (mode == gpuPrimaryPathGenerationModePinhole) {
      const std::uint64_t pixelCount =
        checkedProduct(pinhole.actualWidth, pinhole.actualHeight, "GPU pinhole primary pixel");
      return checkedProduct(pixelCount, pinhole.samplesPerPixel, "GPU pinhole primary path");
    }
    return 0;
  }
}
