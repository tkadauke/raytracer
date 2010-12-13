#ifndef VIEW_PLANE_H
#define VIEW_PLANE_H

#include "Matrix.h"
#include "Vector.h"

#include <vector>
#include <algorithm>

class ViewPlane {
public:
  class IteratorBase {
  public:
    IteratorBase(const ViewPlane* plane);
    IteratorBase(const ViewPlane* plane, bool end);
    virtual ~IteratorBase() {}
    
    Vector3d current() const;
    virtual void advance() = 0;
    
    inline bool operator==(const IteratorBase& other) const {
      return m_row == other.m_row && m_column == other.m_column;
    }
    
    inline int column() const { return m_column; }
    inline int row() const { return m_row; }
    inline int pixelSize() const { return m_pixelSize; }
    
  protected:
    const ViewPlane* m_plane;
    int m_column, m_row, m_pixelSize;
  };

  class RegularIterator : public IteratorBase {
  public:
    RegularIterator(const ViewPlane* plane);
    RegularIterator(const ViewPlane* plane, bool);
    
    virtual void advance();
  };
  
  class Iterator {
  public:
    Iterator(IteratorBase* iteratorImpl) {
      m_iteratorImpl = iteratorImpl;
    }
    
    ~Iterator() {
      delete m_iteratorImpl;
    }
    
    Vector3d operator*() const {
      return m_iteratorImpl->current();
    }
    
    virtual Iterator& operator++() {
      m_iteratorImpl->advance();
      return *this;
    }
    
    inline bool operator==(const Iterator& other) const {
      return *m_iteratorImpl == *(other.m_iteratorImpl);
    }
    
    inline bool operator!=(const Iterator& other) const {
      return !(*this == other);
    }
    
    inline int column() const { return m_iteratorImpl->column(); }
    inline int row() const { return m_iteratorImpl->row(); }
    
    inline int pixelSize() const { return m_iteratorImpl->pixelSize(); }
    
  protected:
    IteratorBase* m_iteratorImpl;
  };
  
  inline ViewPlane()
    : m_width(0), m_height(0) {}
  inline ViewPlane(const Matrix4d& matrix, int width, int height)
    : m_matrix(matrix), m_width(width), m_height(height)
  {
    setupVectors();
  }
  
  inline void setup(const Matrix4d& matrix, int width, int height) {
    m_matrix = matrix;
    m_width = width;
    m_height = height;
    setupVectors();
  }
  
  inline int width() const { return m_width; }
  inline int height() const { return m_height; }
  
  virtual Iterator begin() const;
  
  inline Iterator end() const {
    return Iterator(new RegularIterator(this, true));
  }
  
  const Vector3d& topLeft() const { return m_topLeft; }
  const Vector3d& right() const { return m_right; }
  const Vector3d& down() const { return m_down; }
  
protected:
  void setupVectors();

  friend class Iterator;
  
  Matrix4d m_matrix;
  int m_width, m_height;
  Vector3d m_topLeft, m_right, m_down;
};

#endif
