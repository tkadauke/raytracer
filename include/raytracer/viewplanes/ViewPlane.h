#pragma once

#include "core/math/Matrix.h"
#include "core/math/Vector.h"
#include "core/math/Rect.h"

#include <vector>
#include <algorithm>

namespace raytracer {
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

    inline ViewPlane() : m_pixelSize(1) {}
    inline ViewPlane(const Matrix4d& matrix, const Rect& window)
      : m_matrix(matrix), m_window(window), m_pixelSize(1)
    {
      setupVectors();
    }

    virtual ~ViewPlane();

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

    inline const Vector3d& topLeft() const { return m_topLeft; }
    inline const Vector3d& right() const { return m_right; }
    inline const Vector3d& down() const { return m_down; }
    inline double pixelSize() const { return m_pixelSize; }
    inline void setPixelSize(double pixelSize) { m_pixelSize = pixelSize; }
    inline Vector3d pixelAt(int x, int y) { return (m_topLeft + m_right * x + m_down * y) * m_pixelSize; }

  protected:
    void setupVectors();

    friend class Iterator;

    Matrix4d m_matrix;
    Rect m_window;
    Vector3d m_topLeft, m_right, m_down;
    float m_pixelSize;
  };
}
