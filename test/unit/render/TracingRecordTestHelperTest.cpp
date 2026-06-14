#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test/helpers/TracingRecordTestHelper.h"

#include "render/materials/MatteMaterial.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

namespace TracingRecordTestHelperTest {
  using namespace render;
  using ::testing::HasSubstr;

  namespace {
    using ::testing::internal::CompiledIntersectionHitMatchesGpuRecord;
    using ::testing::internal::GpuIntersectionHitRecordNear;
    using ::testing::internal::GpuIntersectionOcclusionRecordEqual;
    using ::testing::internal::TracingRecordTolerance;
    using ::testing::internal::WavefrontClosestHitResultNear;

    GpuIntersectionHitRecord gpuHitRecord() {
      GpuIntersectionHitRecord record;
      record.hit = 1;
      record.material = 2;
      record.object = 3;
      record.primitiveRecord = 4;
      record.rayIndex = 5;
      record.distance = 6.0f;
      record.point = {7.0f, 8.0f, 9.0f, 1.0f};
      record.normal = {0.0f, 1.0f, 0.0f, 0.0f};
      record.uv = {0.25f, 0.75f, 0.0f, 0.0f};
      record.barycentric = {0.2f, 0.3f, 0.5f, 0.0f};
      return record;
    }

    CompiledIntersectionHit compiledHit() {
      CompiledIntersectionHit hit;
      hit.hit = true;
      hit.material = 2;
      hit.object = 3;
      hit.primitiveRecord = 4;
      hit.distance = 6.0;
      hit.point = Vector4d(7.0, 8.0, 9.0, 1.0);
      hit.normal = Vector3d(0.0, 1.0, 0.0);
      hit.uv = Vector2d(0.25, 0.75);
      hit.barycentric = Vector3d(0.2, 0.3, 0.5);
      return hit;
    }

    std::shared_ptr<MatteMaterial> material() {
      return std::make_shared<MatteMaterial>(
        std::make_shared<ConstantColorTexture>(Colord::white()));
    }
  }

  TEST(TracingRecordTestHelper, GpuHitRecordsMatchWithinTolerance) {
    GpuIntersectionHitRecord actual = gpuHitRecord();
    actual.distance += 0.00001f;
    actual.point[0] += 0.00001f;
    actual.normal[1] -= 0.00001f;
    actual.uv[0] += 0.00001f;
    actual.barycentric[2] -= 0.00001f;

    TracingRecordTolerance tolerance;
    tolerance.distance = 0.0001;
    tolerance.coordinate = 0.0001;
    tolerance.normal = 0.0001;
    tolerance.uv = 0.0001;
    tolerance.barycentric = 0.0001;

    EXPECT_TRUE(GpuIntersectionHitRecordNear(gpuHitRecord(), actual, tolerance));
    EXPECT_GPU_INTERSECTION_HIT_RECORD_NEAR(gpuHitRecord(), actual, tolerance);
  }

  TEST(TracingRecordTestHelper, GpuHitRecordMismatchReportsIdsCoordinatesAndMissState) {
    GpuIntersectionHitRecord actual = gpuHitRecord();
    actual.hit = 0;
    actual.material = 20;
    actual.object = 30;
    actual.primitiveRecord = 40;
    actual.rayIndex = 50;

    const ::testing::AssertionResult result =
      GpuIntersectionHitRecordNear(gpuHitRecord(), actual, TracingRecordTolerance());

    ASSERT_FALSE(result);
    EXPECT_THAT(result.message(), HasSubstr("hit"));
    EXPECT_THAT(result.message(), HasSubstr("rayIndex"));
    EXPECT_THAT(result.message(), HasSubstr("material"));
    EXPECT_THAT(result.message(), HasSubstr("object"));
    EXPECT_THAT(result.message(), HasSubstr("primitiveRecord"));
  }

  TEST(TracingRecordTestHelper, GpuHitRecordPayloadMismatchReportsNamedFields) {
    GpuIntersectionHitRecord actual = gpuHitRecord();
    actual.distance = 6.5f;
    actual.point[2] = 9.5f;
    actual.normal[0] = 1.0f;
    actual.uv[1] = 0.5f;
    actual.barycentric[0] = 0.1f;

    const ::testing::AssertionResult result =
      GpuIntersectionHitRecordNear(gpuHitRecord(), actual, TracingRecordTolerance());

    ASSERT_FALSE(result);
    EXPECT_THAT(result.message(), HasSubstr("distance"));
    EXPECT_THAT(result.message(), HasSubstr("point"));
    EXPECT_THAT(result.message(), HasSubstr("normal"));
    EXPECT_THAT(result.message(), HasSubstr("uv"));
    EXPECT_THAT(result.message(), HasSubstr("barycentric"));
  }

  TEST(TracingRecordTestHelper, GpuMissRecordsIgnoreUndefinedPayloadButCompareMissState) {
    GpuIntersectionHitRecord expected;
    expected.rayIndex = 5;
    expected.distance = std::numeric_limits<float>::infinity();

    GpuIntersectionHitRecord actual = expected;
    actual.point = {99.0f, 98.0f, 97.0f, 1.0f};
    actual.normal = {1.0f, 0.0f, 0.0f, 0.0f};

    EXPECT_TRUE(GpuIntersectionHitRecordNear(expected, actual, TracingRecordTolerance()));

    actual.hit = 1;
    const ::testing::AssertionResult result =
      GpuIntersectionHitRecordNear(expected, actual, TracingRecordTolerance());

    ASSERT_FALSE(result);
    EXPECT_THAT(result.message(), HasSubstr("hit"));
  }

  TEST(TracingRecordTestHelper, OcclusionRecordsCompareOcclusionAndRayId) {
    const GpuIntersectionOcclusionRecord expected{1u, 7u, {}};
    EXPECT_TRUE(GpuIntersectionOcclusionRecordEqual(expected, expected));
    EXPECT_GPU_INTERSECTION_OCCLUSION_RECORD_EQ(expected, expected);

    const GpuIntersectionOcclusionRecord actual{0u, 8u, {}};
    const ::testing::AssertionResult result = GpuIntersectionOcclusionRecordEqual(expected, actual);

    ASSERT_FALSE(result);
    EXPECT_THAT(result.message(), HasSubstr("occluded"));
    EXPECT_THAT(result.message(), HasSubstr("rayIndex"));
  }

  TEST(TracingRecordTestHelper, CompiledHitMatchesGpuRecordAndReportsCoordinateMismatches) {
    GpuIntersectionHitRecord actual = gpuHitRecord();
    EXPECT_TRUE(
      CompiledIntersectionHitMatchesGpuRecord(compiledHit(), actual, 5, TracingRecordTolerance()));
    EXPECT_COMPILED_INTERSECTION_HIT_MATCHES_GPU_RECORD(compiledHit(), actual, 5,
                                                        TracingRecordTolerance());

    actual.point[0] = 8.0f;
    actual.normal[1] = 0.0f;
    actual.uv[0] = 0.5f;
    actual.barycentric[2] = 0.4f;

    const ::testing::AssertionResult result =
      CompiledIntersectionHitMatchesGpuRecord(compiledHit(), actual, 5, TracingRecordTolerance());

    ASSERT_FALSE(result);
    EXPECT_THAT(result.message(), HasSubstr("point"));
    EXPECT_THAT(result.message(), HasSubstr("normal"));
    EXPECT_THAT(result.message(), HasSubstr("uv"));
    EXPECT_THAT(result.message(), HasSubstr("barycentric"));
  }

  TEST(TracingRecordTestHelper, CompiledMissMatchesGpuMissAndReportsHitMismatch) {
    CompiledIntersectionHit expected;
    GpuIntersectionHitRecord actual;
    actual.rayIndex = 12;

    EXPECT_TRUE(
      CompiledIntersectionHitMatchesGpuRecord(expected, actual, 12, TracingRecordTolerance()));

    actual.hit = 1;
    const ::testing::AssertionResult result =
      CompiledIntersectionHitMatchesGpuRecord(expected, actual, 12, TracingRecordTolerance());

    ASSERT_FALSE(result);
    EXPECT_THAT(result.message(), HasSubstr("hit"));
  }

  TEST(TracingRecordTestHelper, WavefrontClosestHitResultsMatchWithinTolerance) {
    auto primitive = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto matte = material();
    const WavefrontClosestHitResult expected{primitive.get(), matte,
                                             HitPoint(primitive.get(), 3.0, Vector4d(0, 0, 1, 1),
                                                      Vector3d(0, 0, 1), Vector2d(0.25, 0.75))};
    const WavefrontClosestHitResult actual{
      primitive.get(), matte,
      HitPoint(primitive.get(), 3.00001, Vector4d(0, 0, 1.00001, 1), Vector3d(0, 0, 0.99999),
               Vector2d(0.25001, 0.75001))};
    TracingRecordTolerance tolerance;
    tolerance.distance = 0.0001;
    tolerance.coordinate = 0.0001;
    tolerance.normal = 0.0001;
    tolerance.uv = 0.0001;

    EXPECT_TRUE(WavefrontClosestHitResultNear(expected, actual, tolerance));
    EXPECT_WAVEFRONT_CLOSEST_HIT_RESULT_NEAR(expected, actual, tolerance);
  }

  TEST(TracingRecordTestHelper, WavefrontClosestHitResultMismatchReportsMissAndHitPointFields) {
    auto expectedPrimitive = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto actualPrimitive = std::make_shared<Sphere>(Vector3d(1, 0, 0), 1.0);
    auto expectedMaterial = material();
    auto actualMaterial = material();
    const WavefrontClosestHitResult expected{expectedPrimitive.get(), expectedMaterial,
                                             HitPoint(expectedPrimitive.get(), 3.0,
                                                      Vector4d(0, 0, 1, 1), Vector3d(0, 0, 1),
                                                      Vector2d(0.25, 0.75))};
    const WavefrontClosestHitResult actual{actualPrimitive.get(), actualMaterial,
                                           HitPoint(actualPrimitive.get(), 4.0,
                                                    Vector4d(0, 1, 1, 1), Vector3d(0, 1, 0),
                                                    Vector2d(0.5, 0.5))};

    const ::testing::AssertionResult result =
      WavefrontClosestHitResultNear(expected, actual, TracingRecordTolerance());

    ASSERT_FALSE(result);
    EXPECT_THAT(result.message(), HasSubstr("primitive"));
    EXPECT_THAT(result.message(), HasSubstr("material"));
    EXPECT_THAT(result.message(), HasSubstr("hitPoint.distance"));
    EXPECT_THAT(result.message(), HasSubstr("hitPoint.point"));
    EXPECT_THAT(result.message(), HasSubstr("hitPoint.normal"));
    EXPECT_THAT(result.message(), HasSubstr("hitPoint.uv"));
  }

  TEST(TracingRecordTestHelper, WavefrontClosestHitResultMissStateMismatchIsExplicit) {
    auto primitive = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    const WavefrontClosestHitResult expected;
    const WavefrontClosestHitResult actual{
      primitive.get(), nullptr,
      HitPoint(primitive.get(), 3.0, Vector4d(0, 0, 1, 1), Vector3d(0, 0, 1))};

    const ::testing::AssertionResult result =
      WavefrontClosestHitResultNear(expected, actual, TracingRecordTolerance());

    ASSERT_FALSE(result);
    EXPECT_THAT(result.message(), HasSubstr("hit"));
    EXPECT_THAT(result.message(), HasSubstr("primitive"));
  }
}
