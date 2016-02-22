#include "raytracer/primitives/Mesh.h"
#include "raytracer/primitives/Composite.h"
#include "raytracer/primitives/FlatMeshTriangle.h"
#include "raytracer/primitives/SmoothMeshTriangle.h"

using namespace std;
using namespace raytracer;

struct Mesh::Private {
  Private(Mesh& m) : mesh(m) {}
  
  template<class ConcreteMeshTriangle>
  void addTrianglesTo(Composite* composite, Material* material);

private:
  Mesh& mesh;
};

template<class ConcreteMeshTriangle>
void Mesh::Private::addTrianglesTo(Composite* composite, Material* material) {
  for (const auto& face : mesh.faces) {
    for (unsigned int j = 2; j != face.size(); ++j) {
      auto triangle = std::make_shared<ConcreteMeshTriangle>(&mesh, face[0], face[j-1], face[j]);
      triangle->setMaterial(material);
      composite->add(triangle);
    }
  }
}

Mesh::Mesh()
  : p(std::make_unique<Private>(*this))
{
}

Mesh::~Mesh() {
}

void Mesh::computeNormals(bool flip) {
  // find normal of each face and add it to each vertex adjacent to the face
  for (const auto& face : faces) {
    // determine vectors parallel to two edges of face
    Vector3d v0 = vertices[face[face.size() - 1]].point - vertices[face[0]].point;
    Vector3d v1 = vertices[face[1]].point - vertices[face[0]].point;
    Vector3d v = (v0 ^ v1);
    
    if (v.length() > 0) {
      v.normalize();

      // add this normal to each vertex that is adjacent to face
      for (unsigned int j = 0; j < face.size(); j++) {
        vertices[face[j]].normal += v;
      }
    }
  }

  // normalize all the normals at the vertices
  for (auto& vertex : vertices) {
    if (flip)
      vertex.normal = -vertex.normal.normalized();
    else
      vertex.normal.normalize();
  }
}

void Mesh::addSmoothTrianglesTo(Composite* composite, Material* material) {
  p->addTrianglesTo<SmoothMeshTriangle>(composite, material);
}

void Mesh::addFlatTrianglesTo(Composite* composite, Material* material) {
  p->addTrianglesTo<FlatMeshTriangle>(composite, material);
}
