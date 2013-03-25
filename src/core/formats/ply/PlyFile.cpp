#include "formats/ply/PlyFile.h"
#include "formats/ply/PlyParseError.h"
#include "raytracer/primitives/Mesh.h"

#include <string>
#include <sstream>

using namespace std;

PlyFile::PlyFile(istream& is) {
  readHeader(is);
}

PlyFile::PlyFile(istream& is, Mesh& mesh) {
  readHeader(is);
  read(is, mesh);
}

void PlyFile::readHeader(istream& is) {
  string line;
  do {
    bool moreToCome = getline(is, line);
    if (!moreToCome)
      break;
    
    istringstream ls(line);
    string token;
    ls >> token;
    
    if (token == "comment" || token == "ply" || token == "") {
      continue;
    } else if (token == "format") {
      parseFormat(ls);
    } else if (token == "element") {
      parseElement(ls);
    } else if (token == "property") {
      parseProperty(ls);
    } else if (token == "end_header") {
      break;
    } else {
      throw PlyParseError(__FILE__, __LINE__);
    }
  } while (true);
}

void PlyFile::parseFormat(istream& is) {
  
}

void PlyFile::parseElement(istream& is) {
  m_elements.push_back(PlyElement(is));
}

void PlyFile::parseProperty(istream& is) {
  m_elements.back().addProperty(PlyProperty(is));
}

void PlyFile::read(istream& is, Mesh& mesh) {
  for (vector<PlyElement>::const_iterator element = m_elements.begin(); element != m_elements.end(); ++element) {
    for (int i = 0; i != element->count(); ++i) {
      if (element->name() == "vertex") {
        Mesh::Vertex vertex;
        for (vector<PlyProperty>::const_iterator property = element->properties().begin(); property != element->properties().end(); ++property) {
          if (property->name() == "x") {
            is >> vertex.point[0];
          } else if (property->name() == "y") {
            is >> vertex.point[1];
          } else if (property->name() == "z") {
            is >> vertex.point[2];
          } else if (property->name() == "nx") {
            is >> vertex.normal[0];
          } else if (property->name() == "ny") {
            is >> vertex.normal[1];
          } else if (property->name() == "nz") {
            is >> vertex.normal[2];
          } else {
            ignoreProperty(is, *property);
          }
        }
        mesh.vertices.push_back(vertex);
      } else if (element->name() == "face") {
        Mesh::Face face;
        for (vector<PlyProperty>::const_iterator property = element->properties().begin(); property != element->properties().end(); ++property) {
          if ((property->name() == "vertex_index" || property->name() == "vertex_indices") && property->isList()) {
            int count;
            is >> count;
            for (int i = 0; i != count; ++i) {
              int value;
              is >> value;
              face.push_back(value);
            }
          } else {
            ignoreProperty(is, *property);
          }
        }
        mesh.faces.push_back(face);
      } else {
        for (vector<PlyProperty>::const_iterator property = element->properties().begin(); property != element->properties().end(); ++property) {
          ignoreProperty(is, *property);
        }
      }
    }
  }
}

void PlyFile::ignoreProperty(istream& is, const PlyProperty& property) {
  string ignore;
  if (property.isList()) {
    int count;
    is >> count;
    for (int i = 0; i != count; ++i) {
      is >> ignore;
    }
  } else {
    is >> ignore;
  }
}
