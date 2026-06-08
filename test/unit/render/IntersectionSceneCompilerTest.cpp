#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "render/IntersectionSceneCompiler.h"
#include "render/State.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Disk.h"
#include "render/primitives/FlatMeshTriangle.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Triangle.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/geometry/Mesh.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Matrix.h"

namespace IntersectionSceneCompilerTest {
  using namespace render;

  namespace {
    std::shared_ptr<MatteMaterial> material(const Colord& color = Colord::white()) {
      return std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color));
    }

    std::size_t countPrimitivesOfKind(const CompiledIntersectionScene& scene,
                                      IntersectionPrimitiveKind kind) {
      return static_cast<std::size_t>(std::count_if(
        scene.primitives().begin(), scene.primitives().end(),
        [kind](const IntersectionPrimitiveRecord& primitive) { return primitive.kind == kind; }));
    }

    std::unique_ptr<Mesh> triangleMesh() {
      auto mesh = std::make_unique<Mesh>();
      mesh->addVertex(Vector3d(0, 0, 0), Vector3d(0, 0, 1), Vector2d(0, 0));
      mesh->addVertex(Vector3d(1, 0, 0), Vector3d(0, 0, 1), Vector2d(1, 0));
      mesh->addVertex(Vector3d(0, 1, 0), Vector3d(0, 0, 1), Vector2d(0, 1));
      mesh->addFace({0, 1, 2});
      return mesh;
    }

    void expectVectorNear(const Vector2d& actual, const Vector2d& expected, double tolerance) {
      EXPECT_NEAR(expected.x(), actual.x(), tolerance);
      EXPECT_NEAR(expected.y(), actual.y(), tolerance);
    }

    void expectVectorNear(const Vector3d& actual, const Vector3d& expected, double tolerance) {
      EXPECT_NEAR(expected.x(), actual.x(), tolerance);
      EXPECT_NEAR(expected.y(), actual.y(), tolerance);
      EXPECT_NEAR(expected.z(), actual.z(), tolerance);
    }

    void expectVectorNear(const Vector4d& actual, const Vector4d& expected, double tolerance) {
      EXPECT_NEAR(expected.x(), actual.x(), tolerance);
      EXPECT_NEAR(expected.y(), actual.y(), tolerance);
      EXPECT_NEAR(expected.z(), actual.z(), tolerance);
      EXPECT_NEAR(expected.w(), actual.w(), tolerance);
    }

    void expectCompiledClosestHitMatchesRuntime(Scene& scene, const Rayd& ray,
                                                double tolerance = 1e-9) {
      const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
      const CompiledIntersectionHit compiledHit =
        CompiledIntersectionSceneIntersector().intersectClosest(compiled, ray);

      State state;
      HitPointInterval hitPoints;
      const Primitive* runtimeHit = scene.intersect(ray, hitPoints, state);

      ASSERT_NE(nullptr, runtimeHit);
      ASSERT_TRUE(compiledHit.hit);
      ASSERT_GT(compiled.objects().size(), compiledHit.object);
      EXPECT_EQ(runtimeHit, compiled.objects()[compiledHit.object]);
      EXPECT_NEAR(hitPoints.min().distance(), compiledHit.distance, tolerance);
      expectVectorNear(compiledHit.point, hitPoints.min().point(), tolerance);
      expectVectorNear(compiledHit.normal, hitPoints.min().normal(), tolerance);
      expectVectorNear(compiledHit.uv, hitPoints.min().uv(), tolerance);
    }

    void expectCompiledAnyHitMatchesRuntime(Scene& scene, const Rayd& ray, double maxDistance) {
      const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

      State runtimeState;
      const bool runtimeOccluded = scene.occludes(ray, runtimeState, maxDistance);
      const bool compiledOccluded =
        CompiledIntersectionSceneIntersector().intersectAny(compiled, ray, maxDistance);

      EXPECT_EQ(runtimeOccluded, compiledOccluded);
    }
  }

  TEST(IntersectionSceneCompiler, CompilesSupportedPrimitivePayloads) {
    Scene scene;
    scene.add(std::make_shared<Triangle>(Vector3d(0, 0, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0)));
    scene.add(std::make_shared<Sphere>(Vector3d(2, 0, 0), 0.5));
    scene.add(std::make_shared<Rectangle>(Vector3d(3, 0, 0), Vector3d(1, 0, 0), Vector3d(0, 1, 0)));
    scene.add(std::make_shared<Disk>(Vector3d(5, 0, 0), Vector3d(0, 1, 0), 0.25));
    scene.add(std::make_shared<Plane>(Vector3d(0, 1, 0), -1.0));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    ASSERT_EQ(5u, compiled.primitives().size());
    EXPECT_TRUE(compiled.fullySupported());
    EXPECT_EQ(1u, compiled.triangles().size());
    EXPECT_EQ(1u, compiled.spheres().size());
    EXPECT_EQ(1u, compiled.rectangles().size());
    EXPECT_EQ(1u, compiled.disks().size());
    EXPECT_EQ(1u, compiled.planes().size());
    EXPECT_EQ(1u, countPrimitivesOfKind(compiled, IntersectionPrimitiveKind::Triangle));
    EXPECT_EQ(1u, countPrimitivesOfKind(compiled, IntersectionPrimitiveKind::Sphere));
    EXPECT_EQ(1u, countPrimitivesOfKind(compiled, IntersectionPrimitiveKind::Rectangle));
    EXPECT_EQ(1u, countPrimitivesOfKind(compiled, IntersectionPrimitiveKind::Disk));
    EXPECT_EQ(1u, countPrimitivesOfKind(compiled, IntersectionPrimitiveKind::Plane));
    EXPECT_EQ(Vector3d(2, 0, 0), compiled.spheres()[0].center);
    EXPECT_EQ(0.5, compiled.spheres()[0].radius);
    EXPECT_EQ(Vector3d(0, 1, 0), compiled.planes()[0].normal);
    EXPECT_EQ(-1.0, compiled.planes()[0].distance);
  }

  TEST(IntersectionSceneCompiler, CompilesMeshTrianglesAsTrianglePayloads) {
    auto mesh = triangleMesh();
    auto primitive = std::make_shared<FlatMeshTriangle>(mesh.get(), 0, 1, 2);
    Scene scene;
    scene.add(primitive);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    ASSERT_EQ(1u, compiled.primitives().size());
    ASSERT_EQ(1u, compiled.triangles().size());
    EXPECT_EQ(IntersectionPrimitiveKind::Triangle, compiled.primitives()[0].kind);
    EXPECT_EQ(Vector3d(1, 0, 0), compiled.triangles()[0].point1);
    EXPECT_EQ(Vector2d(0, 1), compiled.triangles()[0].uv2);
  }

  TEST(IntersectionSceneCompiler, RecordsUnsupportedPrimitiveReasons) {
    auto torus = std::make_shared<Torus>(2.0, 0.5);
    torus->setName("exact torus");
    Scene scene;
    scene.add(torus);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    ASSERT_EQ(1u, compiled.primitives().size());
    ASSERT_EQ(1u, compiled.unsupportedPrimitives().size());
    EXPECT_FALSE(compiled.fullySupported());
    EXPECT_EQ(IntersectionPrimitiveKind::Unsupported, compiled.primitives()[0].kind);
    EXPECT_EQ(compiled.primitives()[0].object, compiled.unsupportedPrimitives()[0].object);
    EXPECT_EQ("exact torus", compiled.unsupportedPrimitives()[0].primitiveName);
    EXPECT_EQ("primitive is not supported by GPU intersection scene compiler",
              compiled.unsupportedPrimitives()[0].reason);
  }

  TEST(IntersectionSceneCompiler, RoundTripsMaterialAndObjectIds) {
    auto sharedMaterial = material(Colord::red());
    auto first = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    first->setMaterial(sharedMaterial);
    auto second = std::make_shared<Sphere>(Vector3d(3, 0, 0), 1.0);
    second->setMaterial(sharedMaterial);
    Scene scene;
    scene.add(first);
    scene.add(second);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    ASSERT_EQ(2u, compiled.primitives().size());
    ASSERT_GT(compiled.materials().size(), compiled.primitives()[0].material);
    ASSERT_GT(compiled.objects().size(), compiled.primitives()[0].object);
    EXPECT_EQ(compiled.primitives()[0].material, compiled.primitives()[1].material);
    EXPECT_EQ(sharedMaterial.get(), compiled.materials()[compiled.primitives()[0].material].get());
    EXPECT_EQ(first.get(), compiled.objects()[compiled.primitives()[0].object]);
    EXPECT_EQ(second.get(), compiled.objects()[compiled.primitives()[1].object]);
  }

  TEST(IntersectionSceneCompiler, RecordsStaticInstanceTransformPayloads) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto instance = std::make_shared<Instance>(sphere);
    instance->setMatrix(Matrix4d::translate(4, 5, 6));
    Scene scene;
    scene.add(instance);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    ASSERT_EQ(1u, compiled.primitives().size());
    const IntersectionTransformId transformId = compiled.primitives()[0].transform;
    ASSERT_NE(0u, transformId);
    ASSERT_GT(compiled.transforms().size(), transformId);
    EXPECT_EQ(Matrix4d::translate(4, 5, 6), compiled.transforms()[transformId].pointMatrix);
  }

  TEST(IntersectionSceneCompiler, RejectsMovingInstancesBeforeFlattening) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto instance = std::make_shared<Instance>(sphere);
    instance->setVelocity(Vector3d(1, 0, 0));
    Scene scene;
    scene.add(instance);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    ASSERT_EQ(1u, compiled.primitives().size());
    ASSERT_EQ(1u, compiled.unsupportedPrimitives().size());
    EXPECT_EQ(IntersectionPrimitiveKind::Unsupported, compiled.primitives()[0].kind);
    EXPECT_EQ(instance.get(), compiled.objects()[compiled.primitives()[0].object]);
    EXPECT_EQ("moving instances are not supported by GPU intersection scene compiler",
              compiled.unsupportedPrimitives()[0].reason);
  }

  TEST(IntersectionSceneCompiler, BvhRootBoundsMatchRuntimeBounds) {
    auto sphere = std::make_shared<Sphere>(Vector3d(1, 2, 3), 2.0);
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-2, 0, 0), Vector3d(-1, 0, 0), Vector3d(-2, 1, 0));
    Scene scene;
    scene.add(sphere);
    scene.add(triangle);

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    ASSERT_EQ(1u, compiled.bvh().size());
    BoundingBoxd expected;
    expected.include(sphere->boundingBox());
    expected.include(triangle->boundingBox());
    EXPECT_EQ(expected, compiled.bvh()[0].bounds);
    EXPECT_TRUE(compiled.bvh()[0].isLeaf());
    EXPECT_EQ(0u, compiled.bvh()[0].firstPrimitive());
    EXPECT_EQ(compiled.primitives().size(), compiled.bvh()[0].leafPrimitiveCount());
  }

  TEST(IntersectionSceneCompiler, BuildsBoundedLeafBvhForManyPrimitives) {
    Scene scene;
    BoundingBoxd expected;
    for (int index = 0; index != 9; ++index) {
      auto sphere = std::make_shared<Sphere>(Vector3d(index * 3.0, 0, 0), 1.0);
      expected.include(sphere->boundingBox());
      scene.add(sphere);
    }

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);

    ASSERT_GT(compiled.bvh().size(), 1u);
    EXPECT_EQ(expected, compiled.bvh()[0].bounds);
    EXPECT_FALSE(compiled.bvh()[0].isLeaf());

    std::size_t leafCount = 0;
    for (std::size_t nodeIndex = 0; nodeIndex != compiled.bvh().size(); ++nodeIndex) {
      const FlatIntersectionBvhNode& node = compiled.bvh()[nodeIndex];
      if (!node.isLeaf()) {
        EXPECT_LT(node.leftChild(), compiled.bvh().size());
        EXPECT_LT(node.rightChild(), compiled.bvh().size());
        EXPECT_NE(node.leftChild(), node.rightChild());
        continue;
      }

      ++leafCount;
      ASSERT_GT(node.leafPrimitiveCount(), 0u);
      EXPECT_LE(node.leafPrimitiveCount(), 4u);
      ASSERT_LE(node.firstPrimitive() + node.leafPrimitiveCount(), compiled.primitives().size());

      BoundingBoxd leafBounds;
      for (std::uint32_t offset = 0; offset != node.leafPrimitiveCount(); ++offset) {
        leafBounds.include(compiled.primitives()[node.firstPrimitive() + offset].bounds);
      }
      EXPECT_EQ(leafBounds, node.bounds);
    }
    EXPECT_GE(leafCount, 3u);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsTriangleLikeRuntimeScene) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    Scene scene;
    scene.add(triangle);
    const Rayd ray(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const CompiledIntersectionHit compiledHit =
      CompiledIntersectionSceneIntersector().intersectClosest(compiled, ray);

    State state;
    HitPointInterval hitPoints;
    const Primitive* runtimeHit = scene.intersect(ray, hitPoints, state);

    ASSERT_NE(nullptr, runtimeHit);
    ASSERT_TRUE(compiledHit.hit);
    EXPECT_EQ(runtimeHit, compiled.objects()[compiledHit.object]);
    EXPECT_NEAR(hitPoints.min().distance(), compiledHit.distance, 1e-9);
    EXPECT_EQ(hitPoints.min().point(), compiledHit.point);
    EXPECT_EQ(hitPoints.min().normal(), compiledHit.normal);
    EXPECT_NEAR(0.25, compiledHit.barycentric.x(), 1e-9);
    EXPECT_NEAR(0.25, compiledHit.barycentric.y(), 1e-9);
    EXPECT_NEAR(0.5, compiledHit.barycentric.z(), 1e-9);
    EXPECT_NEAR(0.25, compiledHit.uv.x(), 1e-9);
    EXPECT_NEAR(0.5, compiledHit.uv.y(), 1e-9);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsSphereLikeRuntimeScene) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    const Rayd ray(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1));

    expectCompiledClosestHitMatchesRuntime(scene, ray);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsPlaneLikeRuntimeScene) {
    Scene scene;
    scene.add(std::make_shared<Plane>(Vector3d(0, 0, 1), 0.0));
    const Rayd ray(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1));

    expectCompiledClosestHitMatchesRuntime(scene, ray);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsRectangleLikeRuntimeScene) {
    Scene scene;
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const Rayd ray(Vector4d(0.5, 0.25, -3, 1), Vector3d(0, 0, 1));

    expectCompiledClosestHitMatchesRuntime(scene, ray);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsDiskLikeRuntimeScene) {
    Scene scene;
    scene.add(std::make_shared<Disk>(Vector3d(0, 0, 0), Vector3d(0, 0, 1), 1.0));
    const Rayd ray(Vector4d(0.25, 0.5, -3, 1), Vector3d(0, 0, 1));

    expectCompiledClosestHitMatchesRuntime(scene, ray);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsStaticInstanceTransformLikeRuntimeScene) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    auto instance = std::make_shared<Instance>(triangle);
    instance->setMatrix(Matrix4d::translate(0, 0, 2));
    Scene scene;
    scene.add(instance);
    const Rayd ray(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const CompiledIntersectionHit compiledHit =
      CompiledIntersectionSceneIntersector().intersectClosest(compiled, ray);

    State state;
    HitPointInterval hitPoints;
    const Primitive* runtimeHit = scene.intersect(ray, hitPoints, state);

    ASSERT_NE(nullptr, runtimeHit);
    ASSERT_TRUE(compiledHit.hit);
    EXPECT_EQ(runtimeHit, compiled.objects()[compiledHit.object]);
    EXPECT_NEAR(hitPoints.min().distance(), compiledHit.distance, 1e-9);
    EXPECT_EQ(hitPoints.min().point(), compiledHit.point);
    EXPECT_EQ(hitPoints.min().normal(), compiledHit.normal);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsScaledInstanceSphereLikeRuntimeScene) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0);
    auto instance = std::make_shared<Instance>(sphere);
    instance->setMatrix(Matrix4d::translate(0, 0, 2) * Matrix4d(Matrix3d::scale(2, 3, 4)));
    Scene scene;
    scene.add(instance);
    const Rayd ray(Vector4d(0, 0, -6, 1), Vector3d(0, 0, 1));

    expectCompiledClosestHitMatchesRuntime(scene, ray);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsStaticInstanceRectangleLikeRuntimeScene) {
    auto rectangle =
      std::make_shared<Rectangle>(Vector3d(-1, -1, 0), Vector3d(2, 0, 0), Vector3d(0, 2, 0));
    auto instance = std::make_shared<Instance>(rectangle);
    instance->setMatrix(Matrix4d::translate(0, 0, 2));
    Scene scene;
    scene.add(instance);
    const Rayd ray(Vector4d(0.25, 0.5, -3, 1), Vector3d(0, 0, 1));

    expectCompiledClosestHitMatchesRuntime(scene, ray);
  }

  TEST(CompiledIntersectionSceneIntersector, ReportsMissLikeRuntimeScene) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    Scene scene;
    scene.add(triangle);
    const Rayd ray(Vector4d(3, 3, -3, 1), Vector3d(0, 0, 1));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const CompiledIntersectionHit compiledHit =
      CompiledIntersectionSceneIntersector().intersectClosest(compiled, ray);

    State state;
    HitPointInterval hitPoints;
    const Primitive* runtimeHit = scene.intersect(ray, hitPoints, state);

    EXPECT_EQ(nullptr, runtimeHit);
    EXPECT_FALSE(compiledHit.hit);
  }

  TEST(CompiledIntersectionSceneIntersector, SelectsClosestTriangleLikeRuntimeScene) {
    auto farTriangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 2), Vector3d(1, -1, 2), Vector3d(0, 1, 2));
    auto nearTriangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    Scene scene;
    scene.add(farTriangle);
    scene.add(nearTriangle);
    const Rayd ray(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1));

    const CompiledIntersectionScene compiled = IntersectionSceneCompiler().compile(scene);
    const CompiledIntersectionHit compiledHit =
      CompiledIntersectionSceneIntersector().intersectClosest(compiled, ray);

    State state;
    HitPointInterval hitPoints;
    const Primitive* runtimeHit = scene.intersect(ray, hitPoints, state);

    ASSERT_NE(nullptr, runtimeHit);
    ASSERT_TRUE(compiledHit.hit);
    EXPECT_EQ(nearTriangle.get(), runtimeHit);
    EXPECT_EQ(runtimeHit, compiled.objects()[compiledHit.object]);
    EXPECT_NEAR(hitPoints.min().distance(), compiledHit.distance, 1e-9);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsAnyLikeRuntimeScene) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    const Rayd ray(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1));

    expectCompiledAnyHitMatchesRuntime(scene, ray, std::numeric_limits<double>::infinity());
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsAnyHonorsFiniteLightDistance) {
    Scene scene;
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    const Rayd ray(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1));

    expectCompiledAnyHitMatchesRuntime(scene, ray, 1.5);
    expectCompiledAnyHitMatchesRuntime(scene, ray, 2.0);
    expectCompiledAnyHitMatchesRuntime(scene, ray, 2.5);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsAnyFindsNearestEligibleHit) {
    Scene scene;
    scene.add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 4), Vector3d(1, -1, 4), Vector3d(0, 1, 4)));
    scene.add(
      std::make_shared<Rectangle>(Vector3d(-1, -1, 1), Vector3d(2, 0, 0), Vector3d(0, 2, 0)));
    const Rayd ray(Vector4d(0, 0, -3, 1), Vector3d(0, 0, 1));

    expectCompiledAnyHitMatchesRuntime(scene, ray, 4.5);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsAnyStaticInstanceLikeRuntimeScene) {
    auto disk = std::make_shared<Disk>(Vector3d(0, 0, 0), Vector3d(0, 0, 1), 1.0);
    auto instance = std::make_shared<Instance>(disk);
    instance->setMatrix(Matrix4d::translate(0, 0, 2));
    Scene scene;
    scene.add(instance);
    const Rayd ray(Vector4d(0.25, 0.5, -3, 1), Vector3d(0, 0, 1));

    expectCompiledAnyHitMatchesRuntime(scene, ray, 5.5);
  }

  TEST(CompiledIntersectionSceneIntersector, IntersectsAnyMissesLikeRuntimeScene) {
    Scene scene;
    scene.add(std::make_shared<Disk>(Vector3d(0, 0, 0), Vector3d(0, 0, 1), 1.0));
    const Rayd ray(Vector4d(2, 2, -3, 1), Vector3d(0, 0, 1));

    expectCompiledAnyHitMatchesRuntime(scene, ray, std::numeric_limits<double>::infinity());
  }
}
