#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/State.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Box.h"
#include "render/primitives/Disk.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"
#include "render/textures/ConstantColorTexture.h"

namespace GpuIntersectionSceneTest {
  using namespace render;

  namespace {
    std::shared_ptr<MatteMaterial> material(const Colord& color = Colord::white()) {
      return std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color));
    }

    template<typename Record>
    void expectKernelRecordLayout() {
      EXPECT_TRUE(std::is_standard_layout_v<Record>);
      EXPECT_EQ(16u, alignof(Record));
      EXPECT_EQ(0u, sizeof(Record) % 16u);
    }

    void expectVector(const std::array<float, 4>& actual, float x, float y, float z, float w) {
      EXPECT_FLOAT_EQ(x, actual[0]);
      EXPECT_FLOAT_EQ(y, actual[1]);
      EXPECT_FLOAT_EQ(z, actual[2]);
      EXPECT_FLOAT_EQ(w, actual[3]);
    }

    void expectVectorNear(const std::array<float, 4>& actual, const Vector4d& expected,
                          float tolerance = 1e-5f) {
      EXPECT_NEAR(static_cast<float>(expected.x()), actual[0], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.y()), actual[1], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.z()), actual[2], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.w()), actual[3], tolerance);
    }

    void expectVectorNear(const std::array<float, 4>& actual, const Vector3d& expected,
                          float tolerance = 1e-5f) {
      EXPECT_NEAR(static_cast<float>(expected.x()), actual[0], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.y()), actual[1], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.z()), actual[2], tolerance);
      EXPECT_FLOAT_EQ(0.0f, actual[3]);
    }

    void expectVectorNear(const std::array<float, 4>& actual, const Vector2d& expected,
                          float tolerance = 1e-5f) {
      EXPECT_NEAR(static_cast<float>(expected.x()), actual[0], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.y()), actual[1], tolerance);
      EXPECT_FLOAT_EQ(0.0f, actual[2]);
      EXPECT_FLOAT_EQ(0.0f, actual[3]);
    }

    CompiledIntersectionScene compileSingleTriangleScene() {
      auto triangle =
        std::make_shared<Triangle>(Vector3d(-1, -2, 3), Vector3d(4, -2, 3), Vector3d(-1, 5, 3));
      triangle->setMaterial(material(Colord::red()));
      Scene scene;
      scene.add(triangle);
      return IntersectionSceneCompiler().compile(scene);
    }

    void expectPackedClosestHitMatchesCompiled(const Scene& scene, const Rayd& ray,
                                               std::uint32_t rayIndex = 31,
                                               bool expectedBasicHitKernelEligible = false,
                                               bool expectedTriangleHitKernelEligible = false) {
      const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
      const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
      const GpuIntersectionRay packedRay = GpuIntersectionScenePacker().packRay(ray, rayIndex);

      ASSERT_TRUE(buffers.packedClosestHitKernelEligible());
      ASSERT_EQ(expectedTriangleHitKernelEligible, buffers.triangleClosestHitKernelEligible());
      ASSERT_EQ(expectedBasicHitKernelEligible, buffers.basicHitKernelEligible());

      const CompiledIntersectionHit compiledHit =
        CompiledIntersectionSceneIntersector().intersectClosest(compiled, ray);
      const GpuIntersectionHitRecord packedHit =
        GpuIntersectionIntersector().intersectClosest(buffers, packedRay);

      ASSERT_TRUE(compiledHit.hit);
      ASSERT_TRUE(packedHit.hit);
      EXPECT_EQ(rayIndex, packedHit.rayIndex);
      EXPECT_EQ(compiledHit.material, packedHit.material);
      EXPECT_EQ(compiledHit.object, packedHit.object);
      EXPECT_EQ(compiledHit.primitiveRecord, packedHit.primitiveRecord);
      EXPECT_NEAR(static_cast<float>(compiledHit.distance), packedHit.distance, 1e-5f);
      expectVectorNear(packedHit.point, compiledHit.point);
      expectVectorNear(packedHit.normal, compiledHit.normal);
      expectVectorNear(packedHit.uv, compiledHit.uv);
      EXPECT_NEAR(static_cast<float>(compiledHit.barycentric.x()), packedHit.barycentric[0], 1e-5f);
      EXPECT_NEAR(static_cast<float>(compiledHit.barycentric.y()), packedHit.barycentric[1], 1e-5f);
      EXPECT_NEAR(static_cast<float>(compiledHit.barycentric.z()), packedHit.barycentric[2], 1e-5f);
    }

    void expectPackedAnyHitMatchesCompiled(const Scene& scene, const Rayd& ray,
                                           double maxDistance) {
      const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
      const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
      const GpuIntersectionRay packedRay =
        GpuIntersectionScenePacker().packRay(ray, 59, 0.0, maxDistance);

      ASSERT_TRUE(buffers.packedClosestHitKernelEligible());

      const bool compiledHit =
        CompiledIntersectionSceneIntersector().intersectAny(compiled, ray, maxDistance);
      const bool packedHit = GpuIntersectionIntersector().intersectAny(buffers, packedRay);

      EXPECT_EQ(compiledHit, packedHit);
    }
  }

  TEST(GpuIntersectionScene, PackedRecordsHaveStableKernelFriendlyLayout) {
    expectKernelRecordLayout<GpuIntersectionBounds>();
    expectKernelRecordLayout<GpuIntersectionBvhNode>();
    expectKernelRecordLayout<GpuIntersectionPrimitiveRecord>();
    expectKernelRecordLayout<GpuIntersectionTrianglePayload>();
    expectKernelRecordLayout<GpuIntersectionSpherePayload>();
    expectKernelRecordLayout<GpuIntersectionPlanePayload>();
    expectKernelRecordLayout<GpuIntersectionRectanglePayload>();
    expectKernelRecordLayout<GpuIntersectionDiskPayload>();
    expectKernelRecordLayout<GpuIntersectionTransformPayload>();
    expectKernelRecordLayout<GpuIntersectionRay>();
    expectKernelRecordLayout<GpuIntersectionHitRecord>();
    expectKernelRecordLayout<GpuIntersectionOcclusionRecord>();
  }

  TEST(GpuIntersectionScene, PacksBvhPrimitiveAndTriangleArraysForKernelUpload) {
    const CompiledIntersectionScene compiled = compileSingleTriangleScene();

    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);

    ASSERT_EQ(compiled.bvh().size(), buffers.bvh.size());
    ASSERT_EQ(1u, buffers.bvh.size());
    EXPECT_EQ(gpuIntersectionLeafNodeFlag, buffers.bvh[0].flags);
    EXPECT_EQ(0u, buffers.bvh[0].leftOrFirstPrimitive);
    EXPECT_EQ(1u, buffers.bvh[0].primitiveCount);
    expectVector(buffers.bvh[0].bounds.minimum, -1.0f, -2.0f, 3.0f, 0.0f);
    expectVector(buffers.bvh[0].bounds.maximum, 4.0f, 5.0f, 3.0f, 0.0f);

    ASSERT_EQ(1u, buffers.primitives.size());
    const GpuIntersectionPrimitiveRecord& primitive = buffers.primitives[0];
    EXPECT_EQ(static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Triangle), primitive.kind);
    EXPECT_EQ(compiled.primitives()[0].material, primitive.material);
    EXPECT_EQ(compiled.primitives()[0].object, primitive.object);
    EXPECT_EQ(0u, primitive.transform);
    EXPECT_EQ(0u, primitive.payloadOffset);
    EXPECT_EQ(1u, primitive.payloadCount);
    expectVector(primitive.bounds.minimum, -1.0f, -2.0f, 3.0f, 0.0f);
    expectVector(primitive.bounds.maximum, 4.0f, 5.0f, 3.0f, 0.0f);

    ASSERT_EQ(1u, buffers.triangles.size());
    const GpuIntersectionTrianglePayload& triangle = buffers.triangles[0];
    expectVector(triangle.point0, -1.0f, -2.0f, 3.0f, 0.0f);
    expectVector(triangle.point1, 4.0f, -2.0f, 3.0f, 0.0f);
    expectVector(triangle.point2, -1.0f, 5.0f, 3.0f, 0.0f);
    expectVector(triangle.normal0, 0.0f, 0.0f, 1.0f, 0.0f);
    expectVector(triangle.uv0, 0.0f, 0.0f, 0.0f, 0.0f);
    expectVector(triangle.uv1, 1.0f, 0.0f, 0.0f, 0.0f);
    expectVector(triangle.uv2, 0.0f, 1.0f, 0.0f, 0.0f);

    EXPECT_TRUE(buffers.transforms.empty());
    EXPECT_TRUE(buffers.spheres.empty());
    EXPECT_TRUE(buffers.planes.empty());
    EXPECT_TRUE(buffers.rectangles.empty());
    EXPECT_TRUE(buffers.disks.empty());
    EXPECT_TRUE(buffers.triangleClosestHitKernelEligible());
    EXPECT_TRUE(buffers.basicHitKernelEligible());
    EXPECT_TRUE(buffers.packedClosestHitKernelEligible());
    EXPECT_EQ(buffers.bvh.size() * sizeof(GpuIntersectionBvhNode) +
                buffers.primitives.size() * sizeof(GpuIntersectionPrimitiveRecord) +
                buffers.triangles.size() * sizeof(GpuIntersectionTrianglePayload),
              buffers.uploadByteCount());
  }

  TEST(GpuIntersectionScene, PacksExactPrimitivePayloadArraysForKernelUpload) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(1, 2, 3), 4.0));
    scene.add(std::make_shared<Plane>(Vector3d(0, 1, 0), -2.0));
    scene.add(std::make_shared<Rectangle>(Vector3d(3, 4, 5), Vector3d(6, 0, 0), Vector3d(0, 7, 0)));
    scene.add(std::make_shared<Disk>(Vector3d(8, 9, 10), Vector3d(0, 0, 1), 11.0));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);

    ASSERT_EQ(4u, buffers.primitives.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Sphere),
              buffers.primitives[0].kind);
    EXPECT_EQ(0u, buffers.primitives[0].payloadOffset);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Plane),
              buffers.primitives[1].kind);
    EXPECT_EQ(0u, buffers.primitives[1].payloadOffset);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Rectangle),
              buffers.primitives[2].kind);
    EXPECT_EQ(0u, buffers.primitives[2].payloadOffset);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Disk),
              buffers.primitives[3].kind);
    EXPECT_EQ(0u, buffers.primitives[3].payloadOffset);

    ASSERT_EQ(1u, buffers.spheres.size());
    expectVector(buffers.spheres[0].centerRadius, 1.0f, 2.0f, 3.0f, 4.0f);

    ASSERT_EQ(1u, buffers.planes.size());
    expectVector(buffers.planes[0].normalDistance, 0.0f, 1.0f, 0.0f, -2.0f);

    ASSERT_EQ(1u, buffers.rectangles.size());
    expectVector(buffers.rectangles[0].corner, 3.0f, 4.0f, 5.0f, 0.0f);
    expectVector(buffers.rectangles[0].leg1, 6.0f, 0.0f, 0.0f, 0.0f);
    expectVector(buffers.rectangles[0].leg2, 0.0f, 7.0f, 0.0f, 0.0f);
    expectVector(buffers.rectangles[0].normal, 0.0f, 0.0f, 1.0f, 0.0f);

    ASSERT_EQ(1u, buffers.disks.size());
    expectVector(buffers.disks[0].centerRadius, 8.0f, 9.0f, 10.0f, 11.0f);
    expectVector(buffers.disks[0].normal, 0.0f, 0.0f, 1.0f, 0.0f);

    EXPECT_EQ(buffers.bvh.size() * sizeof(GpuIntersectionBvhNode) +
                buffers.primitives.size() * sizeof(GpuIntersectionPrimitiveRecord) +
                buffers.spheres.size() * sizeof(GpuIntersectionSpherePayload) +
                buffers.planes.size() * sizeof(GpuIntersectionPlanePayload) +
                buffers.rectangles.size() * sizeof(GpuIntersectionRectanglePayload) +
                buffers.disks.size() * sizeof(GpuIntersectionDiskPayload),
              buffers.uploadByteCount());
    EXPECT_FALSE(buffers.triangleClosestHitKernelEligible());
    EXPECT_TRUE(buffers.basicHitKernelEligible());
    EXPECT_TRUE(buffers.packedClosestHitKernelEligible());
  }

  TEST(GpuIntersectionScene, PacksRayFrontierWorkItemsAndMissRecords) {
    const Rayd ray(Vector4d(1, 2, 3, 1), Vector3d(0.25, -0.5, 0.75));

    const GpuIntersectionRay packed =
      GpuIntersectionScenePacker().packRay(ray, 42, 0.125, 123.5, 0.75, 0x10u);
    const GpuIntersectionHitRecord miss = GpuIntersectionScenePacker().packMiss(42);

    expectVector(packed.origin, 1.0f, 2.0f, 3.0f, 1.0f);
    expectVector(packed.direction, 0.25f, -0.5f, 0.75f, 0.0f);
    EXPECT_FLOAT_EQ(0.125f, packed.minDistance);
    EXPECT_FLOAT_EQ(123.5f, packed.maxDistance);
    EXPECT_FLOAT_EQ(0.75f, packed.timeSample);
    EXPECT_EQ(0x10u, packed.flags);
    EXPECT_EQ(42u, packed.rayIndex);

    EXPECT_EQ(0u, miss.hit);
    EXPECT_EQ(42u, miss.rayIndex);
    EXPECT_TRUE(std::isinf(miss.distance));
  }

  TEST(GpuIntersectionScene, BasicHitKernelAcceptsExactPayloadsAndStaticTransforms) {
    Scene sphereScene;
    sphereScene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    const GpuIntersectionSceneBuffers sphereBuffers =
      GpuIntersectionScenePacker().packScene(IntersectionSceneCompiler().compile(sphereScene));

    EXPECT_FALSE(sphereBuffers.triangleClosestHitKernelEligible());
    EXPECT_TRUE(sphereBuffers.basicHitKernelEligible());
    EXPECT_TRUE(sphereBuffers.packedClosestHitKernelEligible());
    ASSERT_EQ(1u, sphereBuffers.primitives.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Sphere),
              sphereBuffers.primitives[0].kind);

    auto triangle =
      std::make_shared<Triangle>(Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto instance = std::make_shared<Instance>(triangle);
    instance->setMatrix(Matrix4d::translate(4, 5, 6));
    Scene transformedScene;
    transformedScene.add(instance);
    const GpuIntersectionSceneBuffers transformedBuffers =
      GpuIntersectionScenePacker().packScene(IntersectionSceneCompiler().compile(transformedScene));

    EXPECT_FALSE(transformedBuffers.triangleClosestHitKernelEligible());
    EXPECT_TRUE(transformedBuffers.basicHitKernelEligible());
    EXPECT_TRUE(transformedBuffers.packedClosestHitKernelEligible());
    ASSERT_EQ(1u, transformedBuffers.primitives.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Triangle),
              transformedBuffers.primitives[0].kind);
    EXPECT_NE(0u, transformedBuffers.primitives[0].transform);
    EXPECT_GT(transformedBuffers.transforms.size(), transformedBuffers.primitives[0].transform);
  }

  TEST(GpuIntersectionScene, PacksStaticTransformPayloadsAsRowMajorMatrices) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
    auto instance = std::make_shared<Instance>(triangle);
    instance->setMatrix(Matrix4d::translate(4, 5, 6));
    Scene scene;
    scene.add(instance);

    const GpuIntersectionSceneBuffers buffers =
      GpuIntersectionScenePacker().packScene(IntersectionSceneCompiler().compile(scene));

    ASSERT_EQ(1u, buffers.primitives.size());
    const std::uint32_t transformId = buffers.primitives[0].transform;
    ASSERT_NE(0u, transformId);
    ASSERT_GT(buffers.transforms.size(), transformId);
    const GpuIntersectionTransformPayload& transform = buffers.transforms[transformId];
    EXPECT_FLOAT_EQ(1.0f, transform.pointMatrix[0]);
    EXPECT_FLOAT_EQ(1.0f, transform.pointMatrix[5]);
    EXPECT_FLOAT_EQ(1.0f, transform.pointMatrix[10]);
    EXPECT_FLOAT_EQ(4.0f, transform.pointMatrix[3]);
    EXPECT_FLOAT_EQ(5.0f, transform.pointMatrix[7]);
    EXPECT_FLOAT_EQ(6.0f, transform.pointMatrix[11]);
    EXPECT_FLOAT_EQ(1.0f, transform.pointMatrix[15]);

    EXPECT_FLOAT_EQ(-4.0f, transform.inversePointMatrix[3]);
    EXPECT_FLOAT_EQ(-5.0f, transform.inversePointMatrix[7]);
    EXPECT_FLOAT_EQ(-6.0f, transform.inversePointMatrix[11]);
    EXPECT_FLOAT_EQ(1.0f, transform.normalMatrix[0]);
    EXPECT_FLOAT_EQ(1.0f, transform.normalMatrix[5]);
    EXPECT_FLOAT_EQ(1.0f, transform.normalMatrix[10]);
    EXPECT_FLOAT_EQ(1.0f, transform.normalMatrix[15]);
  }

  TEST(GpuIntersectionScene, PackedTriangleClosestHitMatchesCompiledSceneHit) {
    const CompiledIntersectionScene compiled = compileSingleTriangleScene();
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));
    const GpuIntersectionRay packedRay = GpuIntersectionScenePacker().packRay(ray, 17);

    const CompiledIntersectionHit compiledHit =
      CompiledIntersectionSceneIntersector().intersectClosest(compiled, ray);
    const GpuIntersectionHitRecord packedHit =
      GpuIntersectionIntersector().intersectClosest(buffers, packedRay);

    ASSERT_TRUE(compiledHit.hit);
    ASSERT_TRUE(packedHit.hit);
    EXPECT_EQ(17u, packedHit.rayIndex);
    EXPECT_EQ(compiledHit.material, packedHit.material);
    EXPECT_EQ(compiledHit.object, packedHit.object);
    EXPECT_EQ(compiledHit.primitiveRecord, packedHit.primitiveRecord);
    EXPECT_NEAR(static_cast<float>(compiledHit.distance), packedHit.distance, 1e-5f);
    expectVectorNear(packedHit.point, compiledHit.point);
    expectVectorNear(packedHit.normal, compiledHit.normal);
    expectVectorNear(packedHit.uv, compiledHit.uv);
    EXPECT_NEAR(static_cast<float>(compiledHit.barycentric.x()), packedHit.barycentric[0], 1e-5f);
    EXPECT_NEAR(static_cast<float>(compiledHit.barycentric.y()), packedHit.barycentric[1], 1e-5f);
    EXPECT_NEAR(static_cast<float>(compiledHit.barycentric.z()), packedHit.barycentric[2], 1e-5f);
  }

  TEST(GpuIntersectionScene, PackedSphereClosestHitMatchesCompiledSceneHit) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(1, 2, 3), 2.0));
    const Rayd ray(Vector4d(1, 2, -3, 1), Vector3d(0, 0, 1));

    expectPackedClosestHitMatchesCompiled(scene, ray, 21, true);
  }

  TEST(GpuIntersectionScene, PackedPlaneClosestHitMatchesCompiledSceneHit) {
    Scene scene;
    scene.add(std::make_shared<Plane>(Vector3d(0, 0, 1), -3.0));
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    expectPackedClosestHitMatchesCompiled(scene, ray, 22, true);
  }

  TEST(GpuIntersectionScene, PackedRectangleClosestHitMatchesCompiledSceneHit) {
    Scene scene;
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 3), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    expectPackedClosestHitMatchesCompiled(scene, ray, 23, true);
  }

  TEST(GpuIntersectionScene, PackedDiskClosestHitMatchesCompiledSceneHit) {
    Scene scene;
    scene.add(std::make_shared<Disk>(Vector3d(0, 0, 3), Vector3d(0, 0, 1), 2.0));
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    expectPackedClosestHitMatchesCompiled(scene, ray, 24, true);
  }

  TEST(GpuIntersectionScene, PackedBoxTriangleClosestHitMatchesCompiledSceneHit) {
    Scene scene;
    scene.add(std::make_shared<Box>(Vector3d(0, 0, 3), Vector3d(1, 1, 1)));
    const Rayd ray(Vector4d(0.25, 0.5, 0, 1), Vector3d(0, 0, 1));

    expectPackedClosestHitMatchesCompiled(scene, ray, 25, true, true);
    expectPackedAnyHitMatchesCompiled(scene, ray, 4.0);
  }

  TEST(GpuIntersectionScene, PackedTraversalCullsPrimitiveRecordsByBounds) {
    const auto bounds = [](float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
      return GpuIntersectionBounds{{minX, minY, minZ, 0.0f}, {maxX, maxY, maxZ, 0.0f}};
    };

    GpuIntersectionSceneBuffers buffers;
    buffers.bvh.push_back(GpuIntersectionBvhNode{bounds(-10.0f, -10.0f, -1.0f, 10.0f, 10.0f, 5.0f),
                                                 0, 1, gpuIntersectionLeafNodeFlag, 0});
    buffers.primitives.push_back(GpuIntersectionPrimitiveRecord{
      bounds(20.0f, 20.0f, 20.0f, 21.0f, 21.0f, 21.0f),
      static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Triangle),
      0,
      0,
      0,
      0,
      1,
      {}});
    buffers.triangles.push_back(GpuIntersectionTrianglePayload{{-1.0f, -1.0f, 3.0f, 0.0f},
                                                               {1.0f, -1.0f, 3.0f, 0.0f},
                                                               {0.0f, 1.0f, 3.0f, 0.0f},
                                                               {0.0f, 0.0f, 1.0f, 0.0f},
                                                               {0.0f, 0.0f, 1.0f, 0.0f},
                                                               {0.0f, 0.0f, 1.0f, 0.0f},
                                                               {},
                                                               {},
                                                               {}});

    const GpuIntersectionRay ray =
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 61);

    EXPECT_FALSE(GpuIntersectionIntersector().intersectClosest(buffers, ray).hit);
    EXPECT_FALSE(GpuIntersectionIntersector().intersectAny(buffers, ray));
  }

  TEST(GpuIntersectionScene, PackedClosestTraversalPrunesBoundsBeyondCurrentHit) {
    const auto bounds = [](float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
      return GpuIntersectionBounds{{minX, minY, minZ, 0.0f}, {maxX, maxY, maxZ, 0.0f}};
    };
    const auto triangleAt = [](float z) {
      return GpuIntersectionTrianglePayload{{-1.0f, -1.0f, z, 0.0f},
                                            {1.0f, -1.0f, z, 0.0f},
                                            {0.0f, 1.0f, z, 0.0f},
                                            {0.0f, 0.0f, 1.0f, 0.0f},
                                            {0.0f, 0.0f, 1.0f, 0.0f},
                                            {0.0f, 0.0f, 1.0f, 0.0f},
                                            {},
                                            {},
                                            {}};
    };

    GpuIntersectionSceneBuffers buffers;
    buffers.bvh.push_back(GpuIntersectionBvhNode{bounds(-10.0f, -10.0f, -1.0f, 10.0f, 10.0f, 20.0f),
                                                 0, 2, gpuIntersectionLeafNodeFlag, 0});
    buffers.primitives.push_back(GpuIntersectionPrimitiveRecord{
      bounds(-1.0f, -1.0f, 3.0f, 1.0f, 1.0f, 3.0f),
      static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Triangle),
      0,
      0,
      0,
      0,
      1,
      {}});
    buffers.primitives.push_back(GpuIntersectionPrimitiveRecord{
      bounds(-1.0f, -1.0f, 10.0f, 1.0f, 1.0f, 10.0f),
      static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Triangle),
      0,
      0,
      0,
      1,
      1,
      {}});
    buffers.triangles.push_back(triangleAt(3.0f));
    buffers.triangles.push_back(triangleAt(2.0f));

    const GpuIntersectionRay ray =
      GpuIntersectionScenePacker().packRay(Rayd(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)), 62);

    const GpuIntersectionHitRecord hit =
      GpuIntersectionIntersector().intersectClosest(buffers, ray);

    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(0u, hit.primitiveRecord);
    EXPECT_FLOAT_EQ(3.0f, hit.distance);
  }

  TEST(GpuIntersectionScene, PackedTriangleClosestHitSelectsNearestPrimitiveThroughBvh) {
    Scene scene;
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 6), Vector3d(1, -1, 6), Vector3d(0, 1, 6)));
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 2), Vector3d(1, -1, 2), Vector3d(0, 1, 2)));
    for (int index = 0; index != 6; ++index) {
      const double x = 10.0 + index * 3.0;
      scene.add(
        std::make_shared<Triangle>(Vector3d(x, 0, 4), Vector3d(x + 1, 0, 4), Vector3d(x, 1, 4)));
    }
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    ASSERT_GT(buffers.bvh.size(), 1u);
    const GpuIntersectionHitRecord packedHit = GpuIntersectionIntersector().intersectClosest(
      buffers, GpuIntersectionScenePacker().packRay(ray, 0));

    ASSERT_TRUE(packedHit.hit);
    EXPECT_NEAR(2.0f, packedHit.distance, 1e-5f);
    ASSERT_GT(compiled.primitives().size(), packedHit.primitiveRecord);
    EXPECT_EQ(1u, compiled.primitives()[packedHit.primitiveRecord].payloadOffset);
  }

  TEST(GpuIntersectionScene, PackedClosestHitSelectsNearestExactPrimitiveThroughBvh) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 8), 1.0));
    scene.add(std::make_shared<Disk>(Vector3d(0, 0, 5), Vector3d(0, 0, 1), 1.0));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 3), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    for (int index = 0; index != 6; ++index) {
      const double x = 10.0 + index * 3.0;
      scene.add(std::make_shared<Sphere>(Vector3d(x, 0, 4), 1.0));
    }
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    ASSERT_GT(buffers.bvh.size(), 1u);
    ASSERT_TRUE(buffers.packedClosestHitKernelEligible());
    ASSERT_FALSE(buffers.triangleClosestHitKernelEligible());
    ASSERT_TRUE(buffers.basicHitKernelEligible());
    const GpuIntersectionHitRecord packedHit = GpuIntersectionIntersector().intersectClosest(
      buffers, GpuIntersectionScenePacker().packRay(ray, 0));

    ASSERT_TRUE(packedHit.hit);
    EXPECT_NEAR(3.0f, packedHit.distance, 1e-5f);
    ASSERT_GT(compiled.primitives().size(), packedHit.primitiveRecord);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Rectangle),
              buffers.primitives[packedHit.primitiveRecord].kind);
  }

  TEST(GpuIntersectionScene, PackedTriangleClosestHitHonorsRayDistanceBounds) {
    const CompiledIntersectionScene compiled = compileSingleTriangleScene();
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    const GpuIntersectionHitRecord tooNear = GpuIntersectionIntersector().intersectClosest(
      buffers, GpuIntersectionScenePacker().packRay(ray, 3, 3.1, 100.0));
    const GpuIntersectionHitRecord tooFar = GpuIntersectionIntersector().intersectClosest(
      buffers, GpuIntersectionScenePacker().packRay(ray, 4, 0.0, 2.9));
    const GpuIntersectionHitRecord insideBounds = GpuIntersectionIntersector().intersectClosest(
      buffers, GpuIntersectionScenePacker().packRay(ray, 5, 0.0, 3.1));

    EXPECT_FALSE(tooNear.hit);
    EXPECT_EQ(3u, tooNear.rayIndex);
    EXPECT_FALSE(tooFar.hit);
    EXPECT_EQ(4u, tooFar.rayIndex);
    EXPECT_TRUE(insideBounds.hit);
    EXPECT_EQ(5u, insideBounds.rayIndex);
  }

  TEST(GpuIntersectionScene, PackedAnyHitMatchesCompiledSceneVisibility) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    scene.add(std::make_shared<Disk>(Vector3d(2, 0, 3), Vector3d(0, 0, 1), 1.0));
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    expectPackedAnyHitMatchesCompiled(scene, ray, 3.0);
    expectPackedAnyHitMatchesCompiled(scene, ray, 1.0);
  }

  TEST(GpuIntersectionScene, PackedAnyHitHonorsFiniteLightDistanceEpsilon) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    ASSERT_TRUE(buffers.packedClosestHitKernelEligible());
    EXPECT_FALSE(GpuIntersectionIntersector().intersectAny(
      buffers, GpuIntersectionScenePacker().packRay(ray, 0, 0.0, 2.0)));
    EXPECT_TRUE(GpuIntersectionIntersector().intersectAny(
      buffers, GpuIntersectionScenePacker().packRay(ray, 0, 0.0, 2.01)));
  }

  TEST(GpuIntersectionScene, PackedAnyHitHonorsRayMinimumDistanceForNearSurfaceVisibility) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d::null, 1.0));
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const Rayd nearSurfaceRay(Vector4d(0, 0, -1.0 + 5.0e-5, 1), Vector3d(0, 0, -1));

    ASSERT_TRUE(buffers.packedClosestHitKernelEligible());
    EXPECT_TRUE(GpuIntersectionIntersector().intersectAny(
      buffers, GpuIntersectionScenePacker().packRay(nearSurfaceRay, 0, 0.0, 10.0)));
    EXPECT_FALSE(GpuIntersectionIntersector().intersectAny(
      buffers, GpuIntersectionScenePacker().packRay(nearSurfaceRay, 0, Ray<float>::epsilon, 10.0)));
  }

  TEST(GpuIntersectionScene, PackedAnyHitMatchesGeometryOnlyTransparentSceneOcclusion) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0);
    sphere->setMaterial(std::make_shared<TransparentMaterial>());
    Scene scene;
    scene.add(sphere);
    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const GpuIntersectionSceneBuffers buffers = GpuIntersectionScenePacker().packScene(compiled);
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    ASSERT_TRUE(buffers.packedClosestHitKernelEligible());
    State sceneState;
    EXPECT_TRUE(scene.occludes(ray, sceneState, 3.0));
    EXPECT_TRUE(GpuIntersectionIntersector().intersectAny(
      buffers, GpuIntersectionScenePacker().packRay(ray, 0, 0.0, 3.0)));
  }

  TEST(GpuIntersectionScene, PackedTransformedTriangleClosestHitMatchesCompiledSceneHit) {
    Scene scene;
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    auto instance = std::make_shared<Instance>(triangle);
    instance->setMatrix(Matrix4d::translate(0, 0, 3));
    scene.add(instance);
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    expectPackedClosestHitMatchesCompiled(scene, ray, 41, true);
  }

  TEST(GpuIntersectionScene, PackedScaledSphereClosestHitMatchesCompiledSceneHit) {
    Scene scene;
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto instance = std::make_shared<Instance>(sphere);
    instance->setMatrix(Matrix4d::translate(0, 0, 2) * Matrix4d(Matrix3d::scale(2, 3, 4)));
    scene.add(instance);
    const Rayd ray(Vector4d(0, 0, -6, 1), Vector3d(0, 0, 1));

    expectPackedClosestHitMatchesCompiled(scene, ray, 42, true);
  }

  TEST(GpuIntersectionScene, PackedAnyHitSupportsStaticTransforms) {
    Scene scene;
    auto disk = std::make_shared<Disk>(Vector3d(0, 0, 0), Vector3d(0, 0, 1), 2.0);
    auto instance = std::make_shared<Instance>(disk);
    instance->setMatrix(Matrix4d::translate(0, 0, 4));
    scene.add(instance);
    const Rayd ray(Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1));

    const GpuIntersectionSceneBuffers buffers =
      GpuIntersectionScenePacker().packScene(IntersectionSceneCompiler().compile(scene));
    EXPECT_TRUE(buffers.basicHitKernelEligible());

    expectPackedAnyHitMatchesCompiled(scene, ray, 5.0);
    expectPackedAnyHitMatchesCompiled(scene, ray, 3.0);
  }
}
