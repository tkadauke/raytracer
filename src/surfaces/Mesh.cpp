#include "surfaces/Mesh.h"
#include "surfaces/Composite.h"
#include "surfaces/FlatMeshTriangle.h"
#include "surfaces/SmoothMeshTriangle.h"

using namespace std;

struct Mesh::Private {
  Private(Mesh& m) : mesh(m) {}
  
  template<class ConcreteMeshTriangle>
  void addTrianglesTo(Composite* composite, Material* material);

private:
  Mesh& mesh;
};

template<class ConcreteMeshTriangle>
void Mesh::Private::addTrianglesTo(Composite* composite, Material* material) {
  for (vector<Mesh::Face>::const_iterator i = mesh.faces.begin(); i != mesh.faces.end(); ++i) {
    MeshTriangle* triangle;
    for (unsigned int j = 2; j != i->size(); ++j) {
      triangle = new ConcreteMeshTriangle(&mesh, (*i)[0], (*i)[j-1], (*i)[j]);
      triangle->setMaterial(material);
      composite->add(triangle);
    }
  }
}

Mesh::Mesh()
  : p(new Private(*this))
{
}

Mesh::~Mesh() {
  delete p;
}

void Mesh::computeNormals(bool flip) {
  // find normal of each face and add it to each vertex adjacent to the face
  for (vector<Face>::iterator i = faces.begin(); i != faces.end(); ++i) {
    Face& face = *i;

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
  for (vector<Vertex>::iterator i = vertices.begin(); i != vertices.end(); ++i) {
    if (flip)
      i->normal = -i->normal.normalized();
    else
      i->normal.normalize();
  }
}

void Mesh::addSmoothTrianglesTo(Composite* composite, Material* material) {
  p->addTrianglesTo<SmoothMeshTriangle>(composite, material);
}

void Mesh::addFlatTrianglesTo(Composite* composite, Material* material) {
  p->addTrianglesTo<FlatMeshTriangle>(composite, material);
}
