#include "render/TracingAccumulationLayout.h"

#include <limits>

namespace render {
  namespace {
    std::uint64_t checkedPixelCount(int width, int height) {
      if (width <= 0 || height <= 0) {
        throw std::invalid_argument("tracing accumulation layout requires positive dimensions");
      }
      const auto w = static_cast<std::uint64_t>(width);
      const auto h = static_cast<std::uint64_t>(height);
      if (w > std::numeric_limits<std::uint64_t>::max() / h) {
        throw std::overflow_error("tracing accumulation layout dimensions overflow");
      }
      return w * h;
    }

    std::uint64_t checkedPlaneBytes(std::uint64_t pixels, std::size_t bytesPerPixel) {
      const auto bytes = static_cast<std::uint64_t>(bytesPerPixel);
      if (bytes != 0 && pixels > std::numeric_limits<std::uint64_t>::max() / bytes) {
        throw std::overflow_error("tracing accumulation layout byte size overflows");
      }
      return pixels * bytes;
    }

    std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b) {
      if (a > std::numeric_limits<std::uint64_t>::max() - b) {
        throw std::overflow_error("tracing accumulation layout byte size overflows");
      }
      return a + b;
    }
  }

  TracingAccumulationLayout TracingAccumulationLayout::image(int width, int height) {
    TracingAccumulationLayout layout;
    layout.width = width;
    layout.height = height;
    layout.validate();
    return layout;
  }

  bool TracingAccumulationLayout::hasImageShape() const {
    return width > 0 && height > 0;
  }

  bool TracingAccumulationLayout::hasMomentBuffer() const {
    return momentFormat != TracingAccumulationMomentFormat::None;
  }

  std::uint64_t TracingAccumulationLayout::pixelCount() const {
    return checkedPixelCount(width, height);
  }

  std::uint64_t TracingAccumulationLayout::colorSumBytes() const {
    return checkedPlaneBytes(pixelCount(), bytesPerPixel(colorSumFormat));
  }

  std::uint64_t TracingAccumulationLayout::sampleCountBytes() const {
    return checkedPlaneBytes(pixelCount(), bytesPerPixel(sampleCountFormat));
  }

  std::uint64_t TracingAccumulationLayout::momentBytes() const {
    return checkedPlaneBytes(pixelCount(), bytesPerPixel(momentFormat));
  }

  std::uint64_t TracingAccumulationLayout::resolveBytes() const {
    return checkedPlaneBytes(pixelCount(), bytesPerPixel(resolveFormat));
  }

  std::uint64_t TracingAccumulationLayout::accumulationBytes() const {
    return checkedAdd(checkedAdd(colorSumBytes(), sampleCountBytes()), momentBytes());
  }

  std::uint64_t TracingAccumulationLayout::totalBytes() const {
    return checkedAdd(accumulationBytes(), resolveBytes());
  }

  void TracingAccumulationLayout::validate() const {
    (void)pixelCount();
    (void)colorSumBytes();
    (void)sampleCountBytes();
    (void)momentBytes();
    (void)resolveBytes();
  }

  TracingAccumulationDiagnostics
  TracingAccumulationDiagnostics::forLayout(const TracingAccumulationLayout& layout,
                                            const char* backend, const char* residency) {
    TracingAccumulationLayout validated = layout;
    validated.validate();
    TracingAccumulationDiagnostics diagnostics;
    diagnostics.backend = backend ? backend : "";
    diagnostics.residency = residency ? residency : "";
    diagnostics.layout = validated;
    diagnostics.residentBytes = validated.totalBytes();
    return diagnostics;
  }

  void TracingAccumulationDiagnostics::recordClear(std::uint64_t operations) {
    clearOperations += operations;
  }

  void TracingAccumulationDiagnostics::recordAdd(std::uint64_t samples,
                                                 std::uint64_t operations) {
    addOperations += operations;
    addedSamples += samples;
  }

  void TracingAccumulationDiagnostics::recordResolve(std::uint64_t operations) {
    resolveOperations += operations;
  }

  void TracingAccumulationDiagnostics::recordReadback(std::uint64_t bytes,
                                                      std::uint64_t operations) {
    readbackOperations += operations;
    readbackBytes += bytes;
  }

  std::size_t bytesPerPixel(TracingAccumulationColorFormat format) {
    switch (format) {
    case TracingAccumulationColorFormat::RGBA32Float:
      return 16;
    }
    return 0;
  }

  std::size_t bytesPerPixel(TracingAccumulationSampleCountFormat format) {
    switch (format) {
    case TracingAccumulationSampleCountFormat::UInt32:
      return 4;
    }
    return 0;
  }

  std::size_t bytesPerPixel(TracingAccumulationMomentFormat format) {
    switch (format) {
    case TracingAccumulationMomentFormat::None:
      return 0;
    case TracingAccumulationMomentFormat::RGBA32FloatSecondRawMoment:
      return 16;
    }
    return 0;
  }

  std::size_t bytesPerPixel(TracingResolveFormat format) {
    switch (format) {
    case TracingResolveFormat::RGBA8UnormSrgb:
      return 4;
    }
    return 0;
  }

  const char* toString(TracingAccumulationColorFormat format) {
    switch (format) {
    case TracingAccumulationColorFormat::RGBA32Float:
      return "rgba32_float";
    }
    return "unknown";
  }

  const char* toString(TracingAccumulationSampleCountFormat format) {
    switch (format) {
    case TracingAccumulationSampleCountFormat::UInt32:
      return "uint32";
    }
    return "unknown";
  }

  const char* toString(TracingAccumulationMomentFormat format) {
    switch (format) {
    case TracingAccumulationMomentFormat::None:
      return "none";
    case TracingAccumulationMomentFormat::RGBA32FloatSecondRawMoment:
      return "rgba32_float_second_raw_moment";
    }
    return "unknown";
  }

  const char* toString(TracingResolveFormat format) {
    switch (format) {
    case TracingResolveFormat::RGBA8UnormSrgb:
      return "rgba8_unorm_srgb";
    }
    return "unknown";
  }
}
