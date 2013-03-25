#ifndef ABSTRACT_MESH_TRIANGLE_TEST_H
#define ABSTRACT_MESH_TRIANGLE_TEST_H

#include "raytracer/primitives/Mesh.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

namespace testing {
  template<class MT>
  struct AbstractMeshTriangleTest : public ::testing::Test {
    void SetUp() {
      mesh.vertices.push_back(Mesh::Vertex(Vector3d(-1, -1, 0), Vector3d(0, 0, 1)));
      mesh.vertices.push_back(Mesh::Vertex(Vector3d(-1, 1, 0), Vector3d(0, 0, 1)));
      mesh.vertices.push_back(Mesh::Vertex(Vector3d(1, -1, 0), Vector3d(0, 0, 1)));
    }
    
    Mesh mesh;
  };
  
  TYPED_TEST_CASE_P(AbstractMeshTriangleTest);

  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldInitializeWithValues) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
  }
  
  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldIntersectWithRay) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Ray ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Primitive* primitive = triangle.intersect(ray, hitPoints);
    ASSERT_EQ(primitive, &triangle);
    ASSERT_EQ(Vector3d(0, 0, 0), hitPoints.min().point());
    ASSERT_EQ(Vector3d(0, 0, 1), hitPoints.min().normal());
    ASSERT_EQ(1, hitPoints.min().distance());
  }
  
  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldNotIntersectWithMissingRay) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Ray ray(Vector3d(0, 4, -1), Vector3d(0, 0, 1));
    
    HitPointInterval hitPoints;
    Primitive* primitive = triangle.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldNotIntersectIfPointIsBehindRayOrigin) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Ray ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));
    
    HitPointInterval hitPoints;
    Primitive* primitive = triangle.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, primitive);
    ASSERT_TRUE(hitPoints.min().isUndefined());
  }
  
  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Ray ray(Vector3d(0, 0, -1), Vector3d(0, 0, 1));
    
    ASSERT_TRUE(triangle.intersects(ray));
  }
  
  TYPED_TEST_P(AbstractMeshTriangleTest, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    TypeParam triangle(&this->mesh, 0, 1, 2);
    Ray ray(Vector3d(0, 0, -1), Vector3d(0, 0, -1));
    
    ASSERT_FALSE(triangle.intersects(ray));
  }
  
  REGISTER_TYPED_TEST_CASE_P(AbstractMeshTriangleTest,
                             ShouldInitializeWithValues,
                             ShouldIntersectWithRay,
                             ShouldNotIntersectWithMissingRay,
                             ShouldNotIntersectIfPointIsBehindRayOrigin,
                             ShouldReturnTrueForIntersectsIfThereIsAIntersection,
                             ShouldReturnFalseForIntersectsIfThereIsNoIntersection);
}

#endif
