#include "core/formats/stl/StlFile.h"

#include "core/geometry/Mesh.h"

#include <gtest/gtest.h>

#include <sstream>

namespace StlFileTest {
  TEST(StlFile, ReadsAsciiTriangleMesh) {
    std::istringstream input("solid fixture\n"
                             "  facet normal 0 0 1\n"
                             "    outer loop\n"
                             "      vertex 0 0 0\n"
                             "      vertex 1 0 0\n"
                             "      vertex 0 1 0\n"
                             "    endloop\n"
                             "  endfacet\n"
                             "endsolid fixture\n");

    Mesh mesh;
    StlFile file(input, mesh);

    EXPECT_EQ(3u, mesh.vertices().size());
    ASSERT_EQ(1u, mesh.faces().size());
    EXPECT_EQ((Mesh::Face{0, 1, 2}), mesh.faces()[0]);
  }

  TEST(StlFile, RejectsMalformedAsciiStl) {
    std::istringstream input("solid broken\nfacet normal 0 0 1\nendsolid broken\n");
    Mesh mesh;

    EXPECT_THROW(StlFile file(input, mesh), StlParseError);
  }
}
