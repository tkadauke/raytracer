#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace render {
  /**
    * Pixel format for the HDR per-pixel sum owned by a tracing accumulation
    * backend.
    *
    * The stable v1 GPU layout uses one `rgba32_float` texel per pixel. RGB
    * stores the linear HDR radiance sum before division by the sample count.
    * Alpha is reserved and must be written as zero by producers until a future
    * layout revision assigns it a meaning.
    */
  enum class TracingAccumulationColorFormat { RGBA32Float };

  /**
    * Pixel format for per-pixel sample counters.
    *
    * The counter is independent from the HDR color sum so empty pixels and
    * partially accumulated pixels can be resolved without encoding count in a
    * color channel.
    */
  enum class TracingAccumulationSampleCountFormat { UInt32 };

  /**
    * Optional per-pixel moment buffer used by adaptive sampling or variance
    * diagnostics.
    *
    * `RGBA32FloatSecondRawMoment` stores the sum of squared linear RGB sample
    * values. Alpha is reserved and follows the same zero-write rule as the
    * color-sum alpha channel. The buffer is omitted when the value is `None`.
    */
  enum class TracingAccumulationMomentFormat { None, RGBA32FloatSecondRawMoment };

  /**
    * Pixel format for resolved display/export snapshots.
    *
    * Resolve output is separate from accumulation. The default is LDR sRGB for
    * existing display paths, while HDR resolves can be added without changing
    * the accumulation buffers.
    */
  enum class TracingResolveFormat { RGBA8UnormSrgb };

  /**
    * Stable metadata for backend-owned tracing accumulation resources.
    *
    * The layout is image-shaped and plane-based:
    *
    * - `colorSum`: required `RGBA32Float`, one pixel per output pixel.
    * - `sampleCount`: required `UInt32`, one pixel per output pixel.
    * - `moment`: optional `RGBA32FloatSecondRawMoment`, one pixel per output
    *   pixel when variance/adaptive sampling needs it.
    * - `resolve`: required output format, separate from the HDR accumulation
    *   planes.
    *
    * This type deliberately defines shape and byte accounting only. CPU
    * reference operations and platform kernels are separate follow-up work.
    */
  struct TracingAccumulationLayout {
    int width{0};
    int height{0};
    TracingAccumulationColorFormat colorSumFormat{TracingAccumulationColorFormat::RGBA32Float};
    TracingAccumulationSampleCountFormat sampleCountFormat{
      TracingAccumulationSampleCountFormat::UInt32};
    TracingAccumulationMomentFormat momentFormat{TracingAccumulationMomentFormat::None};
    TracingResolveFormat resolveFormat{TracingResolveFormat::RGBA8UnormSrgb};

    static TracingAccumulationLayout image(int width, int height);

    bool hasImageShape() const;
    bool hasMomentBuffer() const;
    std::uint64_t pixelCount() const;
    std::uint64_t colorSumBytes() const;
    std::uint64_t sampleCountBytes() const;
    std::uint64_t momentBytes() const;
    std::uint64_t resolveBytes() const;
    std::uint64_t accumulationBytes() const;
    std::uint64_t totalBytes() const;

    void validate() const;
  };

  std::size_t bytesPerPixel(TracingAccumulationColorFormat format);
  std::size_t bytesPerPixel(TracingAccumulationSampleCountFormat format);
  std::size_t bytesPerPixel(TracingAccumulationMomentFormat format);
  std::size_t bytesPerPixel(TracingResolveFormat format);

  const char* toString(TracingAccumulationColorFormat format);
  const char* toString(TracingAccumulationSampleCountFormat format);
  const char* toString(TracingAccumulationMomentFormat format);
  const char* toString(TracingResolveFormat format);
}
