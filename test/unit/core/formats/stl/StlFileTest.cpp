#include <gtest/gtest.h>

#include "core/formats/stl/StlFile.h"
#include "core/formats/stl/StlParseError.h"
#include "core/geometry/Mesh.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace StlFileTest {
  using core::formats::stl::StlEncoding;
  using core::formats::stl::StlFile;
  using core::formats::stl::StlParseError;

  void appendUint32LE(std::string& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<char>(value & 0xff));
    bytes.push_back(static_cast<char>((value >> 8) & 0xff));
    bytes.push_back(static_cast<char>((value >> 16) & 0xff));
    bytes.push_back(static_cast<char>((value >> 24) & 0xff));
  }

  void appendFloat32LE(std::string& bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(value));
    appendUint32LE(bytes, bits);
  }

  void appendVector(std::string& bytes, float x, float y, float z) {
    appendFloat32LE(bytes, x);
    appendFloat32LE(bytes, y);
    appendFloat32LE(bytes, z);
  }

  void appendBinaryTriangle(std::string& bytes) {
    appendVector(bytes, 0.0f, 0.0f, 1.0f);
    appendVector(bytes, 0.0f, 0.0f, 0.0f);
    appendVector(bytes, 1.0f, 0.0f, 0.0f);
    appendVector(bytes, 0.0f, 1.0f, 0.0f);
    bytes.push_back('\0');
    bytes.push_back('\0');
  }

  std::string binaryStlWithDeclaredCount(std::uint32_t count) {
    std::string bytes(80, ' ');
    appendUint32LE(bytes, count);
    appendBinaryTriangle(bytes);
    return bytes;
  }

  TEST(StlFile, ImportsAsciiTrianglesFromFixture) {
    std::ifstream input("test/fixtures/stl/triangle_ascii.stl");
    Mesh mesh;

    StlFile file(input, mesh);

    EXPECT_EQ(StlEncoding::Ascii, file.encoding());
    EXPECT_EQ(1u, file.triangleCount());
    ASSERT_EQ(3u, mesh.vertices().size());
    ASSERT_EQ(1u, mesh.faces().size());
    EXPECT_EQ(Vector3d(0, 0, 0), mesh.vertices()[0].point);
    EXPECT_EQ(Vector3d(1, 0, 0), mesh.vertices()[1].point);
    EXPECT_EQ(Vector3d(0, 1, 0), mesh.vertices()[2].point);
    EXPECT_EQ(Vector3d(0, 0, 1), mesh.vertices()[0].normal);
  }

  TEST(StlFile, ImportsBinaryTrianglesFromFixture) {
    std::ifstream input("test/fixtures/stl/triangle_binary.stl", std::ios::binary);
    Mesh mesh;

    StlFile file(input, mesh);

    EXPECT_EQ(StlEncoding::Binary, file.encoding());
    EXPECT_EQ(2u, file.triangleCount());
    EXPECT_EQ(6u, mesh.vertices().size());
    EXPECT_EQ(2u, mesh.faces().size());
  }

  TEST(StlFile, ValidatesBinaryTriangleCountAgainstFileSize) {
    const std::string bytes = binaryStlWithDeclaredCount(2);
    std::istringstream input(bytes);
    Mesh mesh;

    EXPECT_THROW(StlFile file(input, mesh), StlParseError);
  }

  TEST(StlFile, RejectsMalformedAsciiFixture) {
    std::ifstream input("test/fixtures/stl/malformed_ascii.stl");
    Mesh mesh;

    EXPECT_THROW(StlFile file(input, mesh), StlParseError);
  }

  TEST(StlFile, ComputesNormalFromWindingWhenFacetNormalIsZero) {
    std::istringstream input("solid fallback\n"
                             "facet normal 0 0 0\n"
                             "outer loop\n"
                             "vertex 0 0 0\n"
                             "vertex 1 0 0\n"
                             "vertex 0 1 0\n"
                             "endloop\n"
                             "endfacet\n"
                             "endsolid fallback\n");
    Mesh mesh;

    StlFile file(input, mesh);

    ASSERT_EQ(3u, mesh.vertices().size());
    EXPECT_EQ(Vector3d(0, 0, -1), mesh.vertices()[0].normal);
    EXPECT_EQ(Vector3d(0, 0, -1), mesh.vertices()[1].normal);
    EXPECT_EQ(Vector3d(0, 0, -1), mesh.vertices()[2].normal);
  }

  TEST(StlFile, NormalizesFacetNormalsDeterministically) {
    std::istringstream input("solid normals\n"
                             "facet normal 0 0 5\n"
                             "outer loop\n"
                             "vertex 0 0 0\n"
                             "vertex 1 0 0\n"
                             "vertex 0 1 0\n"
                             "endloop\n"
                             "endfacet\n"
                             "endsolid normals\n");
    Mesh mesh;

    StlFile file(input, mesh);

    ASSERT_EQ(3u, mesh.vertices().size());
    EXPECT_EQ(Vector3d(0, 0, 1), mesh.vertices()[0].normal);
  }
}
