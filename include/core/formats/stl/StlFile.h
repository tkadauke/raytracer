#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>

class Mesh;

namespace core::formats::stl {

  enum class StlEncoding { Ascii, Binary };

  /**
    * Reads ASCII and binary STL triangle meshes.
    *
    * STL stores only triangle geometry plus optional per-facet normals. It has
    * no unit, material, hierarchy, or UV fields; callers that expose STL to
    * users should report those assumptions at the importer layer.
    */
  class StlFile {
  public:
    explicit StlFile(std::istream& input);
    StlFile(std::istream& input, Mesh& mesh);

    void read(std::istream& input, Mesh& mesh);

    [[nodiscard]] StlEncoding encoding() const {
      return m_encoding;
    }

    [[nodiscard]] std::size_t triangleCount() const {
      return m_triangleCount;
    }

  private:
    void readBytes(std::istream& input);
    void read(std::istream& input, Mesh* mesh);
    void parse(Mesh* mesh);
    void parseAscii(Mesh* mesh);
    void parseBinary(Mesh* mesh);

    std::string m_bytes;
    StlEncoding m_encoding{StlEncoding::Ascii};
    std::size_t m_triangleCount{0};
  };

}
