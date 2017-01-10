#include "core/geometry/Mesh.h"

using namespace std;

struct Mesh::Private {
  inline explicit Private()
  {
  }

  std::vector<Vertex> vertices;
  std::vector<Face> faces;
};

Mesh::Mesh()
  : p(std::make_unique<Private>())
{
}

Mesh::~Mesh() {
}

void Mesh::computeNormals(bool flip) {
  // find normal of each face and add it to each vertex adjacent to the face
  for (const auto& face : p->faces) {
    // determine vectors parallel to two edges of face
    Vector3d v0 = p->vertices[face[face.size() - 1]].point - p->vertices[face[0]].point;
    Vector3d v1 = p->vertices[face[1]].point - p->vertices[face[0]].point;
    Vector3d v = (v0 ^ v1);
    
    if (v.length() > 0) {
      v.normalize();

      // add this normal to each vertex that is adjacent to face
      for (unsigned int j = 0; j < face.size(); j++) {
        p->vertices[face[j]].normal += v;
      }
    }
  }

  // normalize all the normals at the vertices
  for (auto& vertex : p->vertices) {
    if (flip)
      vertex.normal = -vertex.normal.normalized();
    else
      vertex.normal.normalize();
  }
}

void Mesh::addVertex(const Mesh::Vertex& vertex) {
  p->vertices.push_back(vertex);
}

void Mesh::addVertex(const Vector3d& point, const Vector3d& normal) {
  p->vertices.push_back(Vertex(point, normal));
}

void Mesh::addFace(const Mesh::Face& face) {
  if (face.size() < 3) {
    throw InvalidMeshFaceException("Invalid mesh face. Trying to add a mesh face with < 3 vertices", __FILE__, __LINE__);
  }
  p->faces.push_back(face);
}

const std::vector<Mesh::Vertex>& Mesh::vertices() const {
  return p->vertices;
}

const std::vector<Mesh::Face>& Mesh::faces() const {
  return p->faces;
}
