#include "core/formats/stl/StlFile.h"

#include "core/formats/BinaryRead.h"
#include "core/formats/stl/StlParseError.h"
#include "core/geometry/Mesh.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>

namespace {
  using core::formats::stl::StlParseError;

  constexpr double kNormalEpsilon = 1e-12;
  constexpr std::size_t kBinaryHeaderBytes = 80;
  constexpr std::size_t kBinaryCountBytes = 4;
  constexpr std::size_t kBinaryTriangleBytes = 50;

  [[noreturn]] void parseError(const std::string& message) {
    throw StlParseError(message, __FILE__, __LINE__);
  }

  bool startsWithAsciiSolid(const std::string& bytes) {
    auto first = std::find_if_not(bytes.begin(), bytes.end(), [](unsigned char ch) {
      return std::isspace(ch) != 0;
    });
    const std::string prefix = "solid";
    return static_cast<std::size_t>(std::distance(first, bytes.end())) >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), first);
  }

  using core::formats::readUint32Le;
  using core::formats::readVector3fLe;

  Vector3d normalFromWinding(const std::array<Vector3d, 3>& vertices) {
    Vector3d normal = (vertices[2] - vertices[0]) ^ (vertices[1] - vertices[0]);
    if (normal.length() <= kNormalEpsilon)
      return Vector3d::null;
    normal.normalize();
    return normal;
  }

  bool finiteVector(const Vector3d& vector) {
    return std::isfinite(vector.x()) && std::isfinite(vector.y()) && std::isfinite(vector.z());
  }

  Vector3d normalizedFacetNormal(const Vector3d& facetNormal,
                                 const std::array<Vector3d, 3>& vertices) {
    if (finiteVector(facetNormal) && facetNormal.length() > kNormalEpsilon)
      return facetNormal.normalized();
    return normalFromWinding(vertices);
  }

  void addTriangle(Mesh& mesh, const Vector3d& normal, const std::array<Vector3d, 3>& vertices) {
    const int base = static_cast<int>(mesh.vertices().size());
    mesh.addVertex(vertices[0], normal);
    mesh.addVertex(vertices[1], normal);
    mesh.addVertex(vertices[2], normal);
    mesh.addFace({base, base + 1, base + 2});
  }

  void expectToken(std::istream& input, const std::string& expected) {
    std::string token;
    if (!(input >> token))
      parseError("Unexpected end of ASCII STL while reading '" + expected + "'");
    if (token != expected)
      parseError("Expected ASCII STL token '" + expected + "', got '" + token + "'");
  }

  double readFiniteDouble(std::istream& input, const std::string& context) {
    double value = 0.0;
    if (!(input >> value) || !std::isfinite(value))
      parseError("Expected finite numeric value for " + context);
    return value;
  }

  Vector3d readVector(std::istream& input, const std::string& context) {
    const double x = readFiniteDouble(input, context + ".x");
    const double y = readFiniteDouble(input, context + ".y");
    const double z = readFiniteDouble(input, context + ".z");
    return Vector3d(x, y, z);
  }
}

namespace core::formats::stl {

  StlFile::StlFile(std::istream& input) {
    read(input, static_cast<Mesh*>(nullptr));
  }

  StlFile::StlFile(std::istream& input, Mesh& mesh) {
    read(input, mesh);
  }

  void StlFile::read(std::istream& input, Mesh& mesh) {
    read(input, &mesh);
  }

  void StlFile::read(std::istream& input, Mesh* mesh) {
    readBytes(input);
    parse(mesh);
  }

  void StlFile::readBytes(std::istream& input) {
    m_bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (input.bad())
      parseError("Unable to read STL input stream");
  }

  void StlFile::parse(Mesh* mesh) {
    if (m_bytes.size() >= kBinaryHeaderBytes + kBinaryCountBytes) {
      const auto* data = reinterpret_cast<const std::uint8_t*>(m_bytes.data());
      const std::uint32_t count = readUint32Le(data + kBinaryHeaderBytes);
      const std::uint64_t expectedSize =
        static_cast<std::uint64_t>(kBinaryHeaderBytes + kBinaryCountBytes) +
        static_cast<std::uint64_t>(count) * kBinaryTriangleBytes;
      if (expectedSize == m_bytes.size()) {
        m_encoding = StlEncoding::Binary;
        parseBinary(mesh);
        return;
      }
      if (!startsWithAsciiSolid(m_bytes))
        parseError("Binary STL triangle count does not match file size");
    }

    if (!startsWithAsciiSolid(m_bytes))
      parseError("STL input is neither a valid binary STL nor an ASCII STL solid");

    m_encoding = StlEncoding::Ascii;
    parseAscii(mesh);
  }

  void StlFile::parseAscii(Mesh* mesh) {
    std::istringstream input(m_bytes);
    expectToken(input, "solid");
    std::string restOfSolidLine;
    std::getline(input, restOfSolidLine);

    m_triangleCount = 0;
    std::string token;
    while (input >> token) {
      if (token == "endsolid")
        return;
      if (token != "facet")
        parseError("Expected ASCII STL token 'facet', got '" + token + "'");

      expectToken(input, "normal");
      const Vector3d facetNormal = readVector(input, "facet normal");
      expectToken(input, "outer");
      expectToken(input, "loop");

      std::array<Vector3d, 3> vertices;
      for (auto& vertex : vertices) {
        expectToken(input, "vertex");
        vertex = readVector(input, "vertex");
      }

      expectToken(input, "endloop");
      expectToken(input, "endfacet");

      const Vector3d normal = normalizedFacetNormal(facetNormal, vertices);
      if (mesh)
        addTriangle(*mesh, normal, vertices);
      ++m_triangleCount;
    }

    parseError("ASCII STL ended before 'endsolid'");
  }

  void StlFile::parseBinary(Mesh* mesh) {
    const auto* data = reinterpret_cast<const std::uint8_t*>(m_bytes.data());
    const std::uint32_t count = readUint32Le(data + kBinaryHeaderBytes);
    m_triangleCount = count;

    const std::uint8_t* cursor = data + kBinaryHeaderBytes + kBinaryCountBytes;
    for (std::uint32_t triangle = 0; triangle != count; ++triangle) {
      const Vector3d facetNormal = readVector3fLe(cursor);
      std::array<Vector3d, 3> vertices = {
        readVector3fLe(cursor + 12),
        readVector3fLe(cursor + 24),
        readVector3fLe(cursor + 36),
      };

      if (!finiteVector(vertices[0]) || !finiteVector(vertices[1]) || !finiteVector(vertices[2]))
        parseError("Binary STL contains a non-finite vertex coordinate");

      const Vector3d normal = normalizedFacetNormal(facetNormal, vertices);
      if (mesh)
        addTriangle(*mesh, normal, vertices);
      cursor += kBinaryTriangleBytes;
    }
  }

}
