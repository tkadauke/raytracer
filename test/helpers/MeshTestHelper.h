#ifndef MESH_TEST_HELPER_H
#define MESH_TEST_HELPER_H

#include <gtest/gtest.h>

#include "core/geometry/Mesh.h"

namespace testing {
  namespace internal {
    inline void expectMeshFacesWoundWithVertexNormals(const Mesh& mesh) {
      ASSERT_GT(mesh.vertices().size(), 0u);
      ASSERT_GT(mesh.faces().size(), 0u);

      const auto& vertices = mesh.vertices();
      for (const auto& face : mesh.faces()) {
        ASSERT_GE(face.size(), 3u);

        const Vector3d& p0 = vertices[face[0]].point;
        Vector3d faceNormal = Vector3d::null;
        for (std::size_t i = 1; i + 1 < face.size(); ++i) {
          faceNormal += (vertices[face[i]].point - p0) ^ (vertices[face[i + 1]].point - p0);
        }
        if (faceNormal.length() == 0.0)
          continue;

        Vector3d averageVertexNormal = Vector3d::null;
        for (int index : face) {
          averageVertexNormal += vertices[index].normal;
        }

        EXPECT_GT(faceNormal * averageVertexNormal, 0.0)
          << "face winding opposes its vertex normals";
      }
    }
  }
}

#define EXPECT_MESH_FACES_WOUND_WITH_VERTEX_NORMALS(mesh)                                          \
  ::testing::internal::expectMeshFacesWoundWithVertexNormals(mesh)

#endif
