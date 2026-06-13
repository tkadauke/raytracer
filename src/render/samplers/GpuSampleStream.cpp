#include "render/samplers/GpuSampleStream.h"

#include <limits>

namespace render {
  namespace {
    constexpr std::uint32_t kInitialCoordinateState = 0x811c9dc5u;
    constexpr std::uint32_t kCoordinateStep = 0x9e3779b9u;
    constexpr double kOneOverTwoTo24 = 1.0 / 16777216.0;

    std::uint32_t mixCoordinateWord(std::uint32_t state, std::uint32_t value) noexcept {
      return GpuSampleStream::pcgHash32(state ^
                                        (value + kCoordinateStep + (state << 6u) + (state >> 2u)));
    }
  }

  GpuSampleStream::GpuSampleStream(std::uint32_t seed, std::uint32_t pixelIndex,
                                   std::uint32_t primarySampleIndex)
      : m_seed(seed),
        m_pixelIndex(pixelIndex),
        m_primarySampleIndex(primarySampleIndex) {
  }

  std::uint32_t GpuSampleStream::pcgHash32(std::uint32_t input) noexcept {
    const std::uint32_t state = input * 747796405u + 2891336453u;
    const std::uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
  }

  std::uint32_t GpuSampleStream::hash(const GpuSampleCoordinate& coordinate) noexcept {
    std::uint32_t state = kInitialCoordinateState;
    state = mixCoordinateWord(state, coordinate.seed);
    state = mixCoordinateWord(state, coordinate.pixelIndex);
    state = mixCoordinateWord(state, coordinate.primarySampleIndex);
    state = mixCoordinateWord(state, coordinate.dimension);
    state = mixCoordinateWord(state, coordinate.component);
    return pcgHash32(state);
  }

  double GpuSampleStream::sample1D(const GpuSampleCoordinate& coordinate) noexcept {
    return static_cast<double>(hash(coordinate) >> 8u) * kOneOverTwoTo24;
  }

  Vector2d GpuSampleStream::sample2D(std::uint32_t seed, std::uint32_t pixelIndex,
                                     std::uint32_t primarySampleIndex,
                                     std::uint32_t dimension) noexcept {
    return Vector2d(sample1D(GpuSampleCoordinate{seed, pixelIndex, primarySampleIndex, dimension,
                                                 /*component=*/0}),
                    sample1D(GpuSampleCoordinate{seed, pixelIndex, primarySampleIndex, dimension,
                                                 /*component=*/1}));
  }

  Vector2d GpuSampleStream::next2D() {
    const Vector2d sample = sample2D(m_seed, m_pixelIndex, m_primarySampleIndex, m_dim);
    ++m_dim;
    return sample;
  }

  double GpuSampleStream::next1D() {
    const double sample = sample1D(
      GpuSampleCoordinate{m_seed, m_pixelIndex, m_primarySampleIndex, m_dim, /*component=*/0});
    ++m_dim;
    return sample;
  }

  SampleStream::PrimarySample GpuSampleStream::primarySample() {
    const Vector2d pixelSample = sample2D(m_seed, m_pixelIndex, m_primarySampleIndex, m_dim);
    const double timeSample =
      sample1D(GpuSampleCoordinate{m_seed, m_pixelIndex, m_primarySampleIndex, m_dim + 1u,
                                   /*component=*/0});
    m_dim += 2u;
    return SampleStream::PrimarySample{pixelSample, timeSample};
  }

  Vector2d GpuSampleStream::sample2D(SampleDimension dimension, std::uint64_t index) {
    return sample2D(m_seed, m_pixelIndex, m_primarySampleIndex,
                    narrowDimension(sampleDimensionIndex(dimension, index)));
  }

  double GpuSampleStream::sample1D(SampleDimension dimension, std::uint64_t index) {
    return sample1D(GpuSampleCoordinate{m_seed, m_pixelIndex, m_primarySampleIndex,
                                        narrowDimension(sampleDimensionIndex(dimension, index)),
                                        /*component=*/0});
  }

  std::uint32_t GpuSampleStream::narrowDimension(std::uint64_t dimension) noexcept {
    return static_cast<std::uint32_t>(dimension & std::numeric_limits<std::uint32_t>::max());
  }
}
