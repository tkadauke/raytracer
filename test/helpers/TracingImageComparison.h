#pragma once

#include "core/Buffer.h"
#include "core/Color.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>

namespace test::helpers {

  struct TracingImageComparisonStats {
    int width = 0;
    int height = 0;
    std::uint64_t differingPixels = 0;
    double normalizedRmsDelta = 0.0;
    double maxNormalizedChannelDelta = 0.0;
    bool dimensionsMatch = false;
  };

  namespace detail {

    inline double normalizedChannel(unsigned int pixel, int channel) {
      const int shift = (2 - channel) * 8;
      return static_cast<double>((pixel >> shift) & 0xffu) / 255.0;
    }

    template<class T>
    inline double normalizedChannel(const Color<T>& color, int channel) {
      return static_cast<double>(color[channel]);
    }

    template<class Pixel>
    inline TracingImageComparisonStats compareImagesWithMatchingDimensions(
      const Buffer<Pixel>& expected, const Buffer<Pixel>& actual) {
      TracingImageComparisonStats stats;
      stats.width = expected.width();
      stats.height = expected.height();
      stats.dimensionsMatch = true;

      double squaredDeltaSum = 0.0;
      for (int y = 0; y != expected.height(); ++y) {
        for (int x = 0; x != expected.width(); ++x) {
          bool pixelDiffers = false;
          for (int channel = 0; channel != 3; ++channel) {
            const double delta =
              normalizedChannel(expected[y][x], channel) - normalizedChannel(actual[y][x], channel);
            squaredDeltaSum += delta * delta;
            stats.maxNormalizedChannelDelta =
              std::max(stats.maxNormalizedChannelDelta, std::abs(delta));
            pixelDiffers = pixelDiffers || delta != 0.0;
          }
          if (pixelDiffers) {
            ++stats.differingPixels;
          }
        }
      }

      const double sampleCount = static_cast<double>(expected.width()) *
                                 static_cast<double>(expected.height()) * 3.0;
      stats.normalizedRmsDelta =
        sampleCount > 0.0 ? std::sqrt(squaredDeltaSum / sampleCount) : 0.0;
      return stats;
    }

    inline std::string dimensionsString(int width, int height) {
      std::ostringstream out;
      out << width << "x" << height;
      return out.str();
    }
  }

  template<class Pixel>
  inline TracingImageComparisonStats compareTracingImages(const Buffer<Pixel>& expected,
                                                          const Buffer<Pixel>& actual) {
    if (expected.width() != actual.width() || expected.height() != actual.height()) {
      TracingImageComparisonStats stats;
      stats.width = expected.width();
      stats.height = expected.height();
      stats.dimensionsMatch = false;
      stats.differingPixels = static_cast<std::uint64_t>(expected.width()) *
                              static_cast<std::uint64_t>(expected.height());
      return stats;
    }

    return detail::compareImagesWithMatchingDimensions(expected, actual);
  }

  template<class Pixel>
  inline ::testing::AssertionResult tracingImagesWithinNormalizedRms(
    const char* expectedExpr, const char* actualExpr, const char* maxRmsExpr,
    const Buffer<Pixel>& expected, const Buffer<Pixel>& actual, double maxNormalizedRmsDelta) {
    const TracingImageComparisonStats stats = compareTracingImages(expected, actual);
    if (!stats.dimensionsMatch) {
      return ::testing::AssertionFailure()
             << expectedExpr << " and " << actualExpr << " have different dimensions: "
             << detail::dimensionsString(expected.width(), expected.height()) << " vs "
             << detail::dimensionsString(actual.width(), actual.height());
    }
    if (stats.normalizedRmsDelta <= maxNormalizedRmsDelta) {
      return ::testing::AssertionSuccess();
    }

    return ::testing::AssertionFailure()
           << "normalized RMS delta between " << expectedExpr << " and " << actualExpr << " is "
           << stats.normalizedRmsDelta << ", exceeding " << maxRmsExpr << " ("
           << maxNormalizedRmsDelta << "); max normalized channel delta is "
           << stats.maxNormalizedChannelDelta << " across " << stats.differingPixels
           << " differing pixels in " << detail::dimensionsString(stats.width, stats.height);
  }
}

#define EXPECT_TRACING_IMAGE_NEAR(expected, actual, max_normalized_rms_delta)                       \
  EXPECT_PRED_FORMAT3(::test::helpers::tracingImagesWithinNormalizedRms, expected, actual,          \
                      max_normalized_rms_delta)

#define ASSERT_TRACING_IMAGE_NEAR(expected, actual, max_normalized_rms_delta)                       \
  ASSERT_PRED_FORMAT3(::test::helpers::tracingImagesWithinNormalizedRms, expected, actual,          \
                      max_normalized_rms_delta)
