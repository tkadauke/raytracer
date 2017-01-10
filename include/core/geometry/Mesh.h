#pragma once

#include "core/math/Vector.h"
#include "core/Exception.h"
#include <vector>
#include <array>

/**
  * This exception type is used to signal that a mesh received a face with an
  * invalid number of vertices, i.e. less than three.
  */
class InvalidMeshFaceException : public Exception {
public:
  inline explicit InvalidMeshFaceException(const std::string& message, const std::string& file, int line)
    : Exception(message, file, line) {}
};

/**
  * Represents a mesh.
  *
  * A mesh is a collection of polygons in three-dimensional space that belong
  * together to form an arbitrarily-shaped object.
  */
class Mesh {
public:
  /**
    * Represents a vertex in a Mesh. Consists of a point and a normal.
    */
  struct Vertex {
    inline Vertex() {}
    inline explicit Vertex(const Vector3d& p, const Vector3d& n)
      : point(p), normal(n) {}
    Vector3d point;
    Vector3d normal;
  };
  
  /**
    * A face is a polygon of vertices. It is encoded as a vector of indices of
    * the vertices that make up the face. All vertices of any given face must
    * lie in the same plane.
    */
  typedef std::vector<int> Face;
  
  /**
    * This iterator type allows to iterate over triangles derived from the
    * polygons that make up a mesh. A triangle iterator is a forward iterator,
    * meaning that it can be iterated only in one direction, and random access
    * is not supported.
    */
  class TriangleIterator {
  public:
    /** The Triangle type is used when dereferencing an iterator. */
    typedef std::array<int, 3> Triangle;
    
    inline ~TriangleIterator() {}
    
    /**
      * Dereference operator. Returns the current Triangle. Calling this method
      * multiple times without advancing the iterator is efficient, since the
      * triangle is calculated and cached when the iterator is advanced.
      */
    inline const Triangle& operator*() const {
      return m_current;
    }
    
    /**
      * Advance the iterator and update the current triangle.
      */
    inline TriangleIterator& operator++() {
      increment();
      setCurrent();
      
      return *this;
    }
    
    /**
      * Inequality operator. Returns true if this iterator is different from
      * other.
      */
    inline bool operator!=(const TriangleIterator& other) const {
      return &m_mesh != &other.m_mesh ||
             m_faceIndex != other.m_faceIndex ||
             m_vertexIndex != other.m_vertexIndex;
    }
  
  private:
    friend class Mesh;
    
    inline TriangleIterator(const Mesh& mesh, bool end)
      : m_mesh(mesh),
        m_faceIndex(end ? mesh.faces().size() : 0),
        m_vertexIndex(2)
    {
      setCurrent();
    }
    
    inline void increment() {
      if (++m_vertexIndex >= m_mesh.faces()[m_faceIndex].size()) {
        m_vertexIndex = 2;
        m_faceIndex++;
      }
    }

    inline void setCurrent() {
      if (m_faceIndex >= m_mesh.faces().size()) {
        m_current = {{-1, -1, -1}};
      } else {
        const auto& face = m_mesh.faces()[m_faceIndex];
        m_current = {{face[0], face[m_vertexIndex-1], face[m_vertexIndex]}};
      }
    }
    
    const Mesh& m_mesh;
    Triangle m_current;
    unsigned int m_faceIndex;
    unsigned int m_vertexIndex;
  };

  /**
    * Constructor. Creates an empty Mesh.
    */
  Mesh();
  ~Mesh();

  /**
    * Computes the normals from the vertexes in the given faces. For this
    * computation to work, all the vertices in any given face must lie in the
    * same plane.
    */
  void computeNormals(bool flip = false);
  
  /**
    * Adds the given @p vertex to this Mesh.
    */
  void addVertex(const Vertex& vertex);
  
  /**
    * Adds a new vertex consisting of @p point and @p normal to this Mesh.
    */
  void addVertex(const Vector3d& point, const Vector3d& normal);
  
  /**
    * Adds the given @p face to this mesh.
    */
  void addFace(const Face& face);
  
  /**
    * Returns a const reference to the vector of vertices.
    */
  const std::vector<Vertex>& vertices() const;
  
  /**
    * Returns a const reference to the vector of faces.
    */
  const std::vector<Face>& faces() const;
  
  /**
    * Returns a TriangleIterator that points to the first triangle if the first
    * face in this mesh.
    */
  inline TriangleIterator begin() const {
    return TriangleIterator(*this, false);
  }
  
  /**
    * Returns a TriangleIterator that points past the last triangle if the last
    * face in this mesh.
    */
  inline TriangleIterator end() const {
    return TriangleIterator(*this, true);
  }

private:
  struct Private;
  std::unique_ptr<Private> p;
};
