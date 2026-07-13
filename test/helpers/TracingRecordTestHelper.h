#pragma once

#include <gtest/gtest.h>

#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/WavefrontIntersectionBackend.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

namespace testing {
  namespace internal {
    struct TracingRecordTolerance {
      double distance{1e-5};
      double coordinate{1e-5};
      double normal{1e-5};
      double uv{1e-5};
      double barycentric{1e-5};
    };

    inline bool tracingRecordScalarNear(double expected, double actual, double tolerance) {
      return expected == actual || std::fabs(expected - actual) <= tolerance;
    }

    template<typename T, std::size_t Size>
    std::string tracingRecordArrayToString(const std::array<T, Size>& values) {
      std::ostringstream stream;
      stream << "{";
      for (std::size_t i = 0; i < Size; ++i) {
        if (i != 0)
          stream << ", ";
        stream << values[i];
      }
      stream << "}";
      return stream.str();
    }

    inline std::array<double, 4> tracingRecordVector4(const Vector4d& value) {
      return {value.x(), value.y(), value.z(), value.w()};
    }

    inline std::array<double, 4> tracingRecordVector4(const Vector3d& value) {
      return {value.x(), value.y(), value.z(), 0.0};
    }

    inline std::array<double, 4> tracingRecordVector4(const Vector2d& value) {
      return {value.x(), value.y(), 0.0, 0.0};
    }

    inline void tracingRecordAppendScalarDiff(::testing::Message& message, const char* field,
                                              double expected, double actual, double tolerance) {
      if (!tracingRecordScalarNear(expected, actual, tolerance))
        message << "\n  " << field << ": expected " << expected << ", actual " << actual
                << ", tolerance " << tolerance;
    }

    template<typename T>
    inline void tracingRecordAppendExactDiff(::testing::Message& message, const char* field,
                                             const T& expected, const T& actual) {
      if (expected != actual)
        message << "\n  " << field << ": expected " << expected << ", actual " << actual;
    }

    template<typename Expected, typename Actual, std::size_t Size>
    inline void tracingRecordAppendArrayDiff(::testing::Message& message, const char* field,
                                             const std::array<Expected, Size>& expected,
                                             const std::array<Actual, Size>& actual,
                                             double tolerance) {
      bool differs = false;
      for (std::size_t i = 0; i < Size; ++i)
        differs = differs || !tracingRecordScalarNear(expected[i], actual[i], tolerance);

      if (differs)
        message << "\n  " << field << ": expected " << tracingRecordArrayToString(expected)
                << ", actual " << tracingRecordArrayToString(actual) << ", tolerance " << tolerance;
    }

    inline ::testing::AssertionResult tracingRecordResult(const ::testing::Message& message) {
      const std::string text = message.GetString();
      if (text.empty())
        return ::testing::AssertionSuccess();
      return ::testing::AssertionFailure() << text;
    }

    inline void tracingRecordAppendGpuHitPayloadDiff(
      ::testing::Message& message, const render::GpuIntersectionHitRecord& expected,
      const render::GpuIntersectionHitRecord& actual, const TracingRecordTolerance& tolerance) {
      tracingRecordAppendScalarDiff(message, "distance", expected.distance, actual.distance,
                                    tolerance.distance);
      tracingRecordAppendArrayDiff(message, "point", expected.point, actual.point,
                                   tolerance.coordinate);
      tracingRecordAppendArrayDiff(message, "normal", expected.normal, actual.normal,
                                   tolerance.normal);
      tracingRecordAppendArrayDiff(message, "uv", expected.uv, actual.uv, tolerance.uv);
      tracingRecordAppendArrayDiff(message, "barycentric", expected.barycentric, actual.barycentric,
                                   tolerance.barycentric);
    }

    inline ::testing::AssertionResult GpuIntersectionHitRecordNear(
      const render::GpuIntersectionHitRecord& expected,
      const render::GpuIntersectionHitRecord& actual,
      const TracingRecordTolerance& tolerance = TracingRecordTolerance()) {
      ::testing::Message message;
      tracingRecordAppendExactDiff(message, "hit", expected.hit, actual.hit);
      tracingRecordAppendExactDiff(message, "rayIndex", expected.rayIndex, actual.rayIndex);
      tracingRecordAppendExactDiff(message, "material", expected.material, actual.material);
      tracingRecordAppendExactDiff(message, "object", expected.object, actual.object);
      tracingRecordAppendExactDiff(message, "primitiveRecord", expected.primitiveRecord,
                                   actual.primitiveRecord);
      if (expected.hit != 0u && actual.hit != 0u)
        tracingRecordAppendGpuHitPayloadDiff(message, expected, actual, tolerance);
      return tracingRecordResult(message);
    }

    inline ::testing::AssertionResult GpuIntersectionHitRecordNearPredFormat(
      const char* expectedExpr, const char* actualExpr, const char* toleranceExpr,
      const render::GpuIntersectionHitRecord& expected,
      const render::GpuIntersectionHitRecord& actual, const TracingRecordTolerance& tolerance) {
      ::testing::AssertionResult result = GpuIntersectionHitRecordNear(expected, actual, tolerance);
      if (result)
        return result;
      return ::testing::AssertionFailure()
             << "GPU intersection hit records " << expectedExpr << " and " << actualExpr
             << " differ using " << toleranceExpr << ":" << result.message();
    }

    inline ::testing::AssertionResult
    GpuIntersectionOcclusionRecordEqual(const render::GpuIntersectionOcclusionRecord& expected,
                                        const render::GpuIntersectionOcclusionRecord& actual) {
      ::testing::Message message;
      tracingRecordAppendExactDiff(message, "occluded", expected.occluded, actual.occluded);
      tracingRecordAppendExactDiff(message, "rayIndex", expected.rayIndex, actual.rayIndex);
      return tracingRecordResult(message);
    }

    inline ::testing::AssertionResult GpuIntersectionOcclusionRecordEqualPredFormat(
      const char* expectedExpr, const char* actualExpr,
      const render::GpuIntersectionOcclusionRecord& expected,
      const render::GpuIntersectionOcclusionRecord& actual) {
      ::testing::AssertionResult result = GpuIntersectionOcclusionRecordEqual(expected, actual);
      if (result)
        return result;
      return ::testing::AssertionFailure()
             << "GPU intersection occlusion records " << expectedExpr << " and " << actualExpr
             << " differ:" << result.message();
    }

    inline ::testing::AssertionResult CompiledIntersectionHitMatchesGpuRecord(
      const render::CompiledIntersectionHit& expected,
      const render::GpuIntersectionHitRecord& actual, std::uint32_t expectedRayIndex,
      const TracingRecordTolerance& tolerance = TracingRecordTolerance()) {
      ::testing::Message message;
      tracingRecordAppendExactDiff(message, "hit", expected.hit, actual.hit != 0u);
      tracingRecordAppendExactDiff(message, "rayIndex", expectedRayIndex, actual.rayIndex);
      tracingRecordAppendExactDiff(message, "material", expected.material, actual.material);
      tracingRecordAppendExactDiff(message, "object", expected.object, actual.object);
      tracingRecordAppendExactDiff(message, "primitiveRecord", expected.primitiveRecord,
                                   actual.primitiveRecord);
      if (expected.hit && actual.hit != 0u) {
        tracingRecordAppendScalarDiff(message, "distance", expected.distance, actual.distance,
                                      tolerance.distance);
        tracingRecordAppendArrayDiff(message, "point", tracingRecordVector4(expected.point),
                                     actual.point, tolerance.coordinate);
        tracingRecordAppendArrayDiff(message, "normal", tracingRecordVector4(expected.normal),
                                     actual.normal, tolerance.normal);
        tracingRecordAppendArrayDiff(message, "uv", tracingRecordVector4(expected.uv), actual.uv,
                                     tolerance.uv);
        tracingRecordAppendArrayDiff(message, "barycentric",
                                     tracingRecordVector4(expected.barycentric),
                                     actual.barycentric, tolerance.barycentric);
      }
      return tracingRecordResult(message);
    }

    inline ::testing::AssertionResult CompiledIntersectionHitMatchesGpuRecordPredFormat(
      const char* expectedExpr, const char* actualExpr, const char* rayIndexExpr,
      const char* toleranceExpr, const render::CompiledIntersectionHit& expected,
      const render::GpuIntersectionHitRecord& actual, std::uint32_t expectedRayIndex,
      const TracingRecordTolerance& tolerance) {
      ::testing::AssertionResult result =
        CompiledIntersectionHitMatchesGpuRecord(expected, actual, expectedRayIndex, tolerance);
      if (result)
        return result;
      return ::testing::AssertionFailure()
             << "Compiled intersection hit " << expectedExpr << " and GPU record " << actualExpr
             << " differ for " << rayIndexExpr << " using " << toleranceExpr << ":"
             << result.message();
    }

    inline ::testing::AssertionResult WavefrontClosestHitResultNear(
      const render::WavefrontClosestHitResult& expected,
      const render::WavefrontClosestHitResult& actual,
      const TracingRecordTolerance& tolerance = TracingRecordTolerance()) {
      ::testing::Message message;
      tracingRecordAppendExactDiff(message, "hit", expected.hit(), actual.hit());
      tracingRecordAppendExactDiff(message, "primitive", expected.primitive, actual.primitive);
      tracingRecordAppendExactDiff(message, "material", expected.material.get(),
                                   actual.material.get());
      if (expected.hit() && actual.hit()) {
        tracingRecordAppendExactDiff(message, "hitPoint.primitive", expected.hitPoint.primitive(),
                                     actual.hitPoint.primitive());
        tracingRecordAppendScalarDiff(message, "hitPoint.distance", expected.hitPoint.distance(),
                                      actual.hitPoint.distance(), tolerance.distance);
        tracingRecordAppendArrayDiff(
          message, "hitPoint.point", tracingRecordVector4(expected.hitPoint.point()),
          tracingRecordVector4(actual.hitPoint.point()), tolerance.coordinate);
        tracingRecordAppendArrayDiff(
          message, "hitPoint.normal", tracingRecordVector4(expected.hitPoint.normal()),
          tracingRecordVector4(actual.hitPoint.normal()), tolerance.normal);
        tracingRecordAppendArrayDiff(message, "hitPoint.uv",
                                     tracingRecordVector4(expected.hitPoint.uv()),
                                     tracingRecordVector4(actual.hitPoint.uv()), tolerance.uv);
      }
      return tracingRecordResult(message);
    }

    inline ::testing::AssertionResult WavefrontClosestHitResultNearPredFormat(
      const char* expectedExpr, const char* actualExpr, const char* toleranceExpr,
      const render::WavefrontClosestHitResult& expected,
      const render::WavefrontClosestHitResult& actual, const TracingRecordTolerance& tolerance) {
      ::testing::AssertionResult result =
        WavefrontClosestHitResultNear(expected, actual, tolerance);
      if (result)
        return result;
      return ::testing::AssertionFailure()
             << "Wavefront closest-hit results " << expectedExpr << " and " << actualExpr
             << " differ using " << toleranceExpr << ":" << result.message();
    }
  }
}

#define ASSERT_GPU_INTERSECTION_HIT_RECORD_NEAR(expected, actual, tolerance)                       \
  ASSERT_PRED_FORMAT3(::testing::internal::GpuIntersectionHitRecordNearPredFormat, expected,       \
                      actual, tolerance)

#define EXPECT_GPU_INTERSECTION_HIT_RECORD_NEAR(expected, actual, tolerance)                       \
  EXPECT_PRED_FORMAT3(::testing::internal::GpuIntersectionHitRecordNearPredFormat, expected,       \
                      actual, tolerance)

#define ASSERT_GPU_INTERSECTION_OCCLUSION_RECORD_EQ(expected, actual)                              \
  ASSERT_PRED_FORMAT2(::testing::internal::GpuIntersectionOcclusionRecordEqualPredFormat,          \
                      expected, actual)

#define EXPECT_GPU_INTERSECTION_OCCLUSION_RECORD_EQ(expected, actual)                              \
  EXPECT_PRED_FORMAT2(::testing::internal::GpuIntersectionOcclusionRecordEqualPredFormat,          \
                      expected, actual)

#define ASSERT_COMPILED_INTERSECTION_HIT_MATCHES_GPU_RECORD(expected, actual, rayIndex, tolerance) \
  ASSERT_PRED_FORMAT4(::testing::internal::CompiledIntersectionHitMatchesGpuRecordPredFormat,      \
                      expected, actual, rayIndex, tolerance)

#define EXPECT_COMPILED_INTERSECTION_HIT_MATCHES_GPU_RECORD(expected, actual, rayIndex, tolerance) \
  EXPECT_PRED_FORMAT4(::testing::internal::CompiledIntersectionHitMatchesGpuRecordPredFormat,      \
                      expected, actual, rayIndex, tolerance)

#define ASSERT_WAVEFRONT_CLOSEST_HIT_RESULT_NEAR(expected, actual, tolerance)                      \
  ASSERT_PRED_FORMAT3(::testing::internal::WavefrontClosestHitResultNearPredFormat, expected,      \
                      actual, tolerance)

#define EXPECT_WAVEFRONT_CLOSEST_HIT_RESULT_NEAR(expected, actual, tolerance)                      \
  EXPECT_PRED_FORMAT3(::testing::internal::WavefrontClosestHitResultNearPredFormat, expected,      \
                      actual, tolerance)
