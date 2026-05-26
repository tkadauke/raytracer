#include "core/formats/stl/StlFile.h"

#include "core/geometry/Mesh.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <istream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
  float readFloat32(const char* bytes) {
    float value;
    std::memcpy(&value, bytes, sizeof(float));
    return value;
  }

  std::uint32_t readUInt32LE(const char* bytes) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 8u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 16u) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3])) << 24u);
  }

  Vector3d vectorFrom(const char* bytes) {
    return Vector3d(readFloat32(bytes), readFloat32(bytes + 4), readFloat32(bytes + 8));
  }

  void addTriangle(Mesh& mesh, const Vector3d& normal, const std::array<Vector3d, 3>& vertices) {
    const int offset = static_cast<int>(mesh.vertices().size());
    mesh.addVertex(vertices[0], normal);
    mesh.addVertex(vertices[1], normal);
    mesh.addVertex(vertices[2], normal);
    mesh.addFace({offset, offset + 1, offset + 2});
  }

  bool tryReadBinary(const std::string& bytes, Mesh& mesh) {
    if (bytes.size() < 84)
      return false;

    const std::uint32_t triangleCount = readUInt32LE(bytes.data() + 80);
    const std::uint64_t expectedSize = 84ull + 50ull * triangleCount;
    if (expectedSize != bytes.size())
      return false;

    for (std::uint32_t i = 0; i != triangleCount; ++i) {
      const char* triangle = bytes.data() + 84 + 50 * i;
      const Vector3d normal = vectorFrom(triangle);
      std::array<Vector3d, 3> vertices = {
        vectorFrom(triangle + 12),
        vectorFrom(triangle + 24),
        vectorFrom(triangle + 36),
      };
      addTriangle(mesh, normal, vertices);
    }
    return true;
  }

  bool readVector(std::istream& input, Vector3d* vector) {
    double x;
    double y;
    double z;
    if (!(input >> x >> y >> z))
      return false;
    *vector = Vector3d(x, y, z);
    return true;
  }

  void expectToken(std::istream& input, const std::string& expected) {
    std::string token;
    if (!(input >> token) || token != expected) {
      throw StlParseError("Invalid ASCII STL: expected '" + expected + "'", __FILE__, __LINE__);
    }
  }

  void readAscii(const std::string& bytes, Mesh& mesh) {
    std::istringstream input(bytes);
    expectToken(input, "solid");
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string token;
    while (input >> token) {
      if (token == "endsolid")
        return;
      if (token != "facet")
        throw StlParseError("Invalid ASCII STL: expected facet", __FILE__, __LINE__);

      expectToken(input, "normal");
      Vector3d normal;
      if (!readVector(input, &normal))
        throw StlParseError("Invalid ASCII STL: malformed facet normal", __FILE__, __LINE__);

      expectToken(input, "outer");
      expectToken(input, "loop");

      std::array<Vector3d, 3> vertices;
      for (auto& vertex : vertices) {
        expectToken(input, "vertex");
        if (!readVector(input, &vertex))
          throw StlParseError("Invalid ASCII STL: malformed vertex", __FILE__, __LINE__);
      }

      expectToken(input, "endloop");
      expectToken(input, "endfacet");
      addTriangle(mesh, normal, vertices);
    }

    throw StlParseError("Invalid ASCII STL: missing endsolid", __FILE__, __LINE__);
  }
}

StlFile::StlFile(std::istream& input, Mesh& mesh) {
  read(input, mesh);
}

void StlFile::read(std::istream& input, Mesh& mesh) {
  const std::string bytes((std::istreambuf_iterator<char>(input)),
                          std::istreambuf_iterator<char>());
  if (bytes.empty())
    throw StlParseError("Invalid STL: empty file", __FILE__, __LINE__);

  if (tryReadBinary(bytes, mesh))
    return;

  readAscii(bytes, mesh);
}
