#include "core/geometry/Mesh.h"

using namespace std;

void Mesh::computeNormals(bool flip) {
  // find normal of each face and add it to each vertex adjacent to the face
  for (const auto& face : m_faces) {
    // determine vectors parallel to two edges of face
    Vector3d v0 = m_vertices[face[face.size() - 1]].point - m_vertices[face[0]].point;
    Vector3d v1 = m_vertices[face[1]].point - m_vertices[face[0]].point;
    Vector3d v = (v0 ^ v1);
    
    if (v.length() > 0) {
      v.normalize();

      // add this normal to each vertex that is adjacent to face
      for (unsigned int j = 0; j < face.size(); j++) {
        m_vertices[face[j]].normal += v;
      }
    }
  }

  // normalize all the normals at the vertices
  for (auto& vertex : m_vertices) {
    if (flip)
      vertex.normal.reverse();
    vertex.normal.normalize();
  }
}
