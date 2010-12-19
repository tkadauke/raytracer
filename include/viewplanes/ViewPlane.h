#ifndef VIEW_PLANE_H
#define VIEW_PLANE_H

#include "math/Matrix.h"
#include "math/Vector.h"
#include "math/Rect.h"

#include <vector>
#include <algorithm>

class ViewPlane {
public:
  class IteratorBase {
  public:
    IteratorBase(const ViewPlane* plane, const Rect& rect);
    IteratorBase(const ViewPlane* plane, const Rect& rect, bool end);
    virtual ~IteratorBase() {}
    
    Vector3d current() const;
    virtual void advance() = 0;
    
    inline bool operator==(const IteratorBase& other) const {
      return m_row == other.m_row && m_column == other.m_column;
    }
    
    inline int column() const { return m_rect.left() + m_column; }
    inline int row() const { return m_rect.top() + m_row; }
    inline int pixelSize() const { return m_pixelSize; }
    
  protected:
    const ViewPlane* m_plane;
    Rect m_rect;
    int m_column, m_row, m_pixelSize;
  };

  class RegularIterator : public IteratorBase {
  public:
    RegularIterator(const ViewPlane* plane, const Rect& rect);
    RegularIterator(const ViewPlane* plane, const Rect& rect, bool);
    
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
  
  inline ViewPlane() {}
  inline ViewPlane(const Matrix4d& matrix, const Rect& window)
    : m_matrix(matrix), m_window(window)
  {
    setupVectors();
  }
  
  inline void setup(const Matrix4d& matrix, const Rect& window) {
    m_matrix = matrix;
    m_window = window;
    setupVectors();
  }
  
  inline int width() const { return m_window.width(); }
  inline int height() const { return m_window.height(); }
  
  virtual Iterator begin(const Rect& rect) const;
  
  inline Iterator end(const Rect& rect) const {
    return Iterator(new RegularIterator(this, rect, true));
  }
  
  const Vector3d& topLeft() const { return m_topLeft; }
  const Vector3d& right() const { return m_right; }
  const Vector3d& down() const { return m_down; }
  
protected:
  void setupVectors();

  friend class Iterator;
  
  Matrix4d m_matrix;
  Rect m_window;
  Vector3d m_topLeft, m_right, m_down;
};

#endif
