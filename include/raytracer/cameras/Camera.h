#pragma once

#include "core/math/Vector.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"
#include "core/Color.h"
#include "core/MemoizedValue.h"
#include "raytracer/viewplanes/ViewPlane.h"

template<class T>
class Buffer;
template<class T>
class Rect;

namespace raytracer {
  class Raytracer;

  class Camera {
  public:
    inline Camera()
      : m_cancelled(false),
        m_viewPlane(nullptr)
    {
    }
    
    inline Camera(const Vector3d& position, const Vector3d& target)
      : m_cancelled(false),
        m_position(position),
        m_target(target),
        m_viewPlane(nullptr)
    {
    }
    
    virtual ~Camera();

    void setPosition(const Vector3d& position) {
      m_matrix.reset();
      m_position = position;
    }

    void setTarget(const Vector3d& target) {
      m_matrix.reset();
      m_target = target;
    }

    void setViewPlane(std::shared_ptr<ViewPlane> plane);
    std::shared_ptr<ViewPlane> viewPlane();

    const Matrix4d& matrix();

    void render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer);
    virtual void render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Rect<int>& rect);
    
    virtual Rayd rayForPixel(double x, double y) = 0;
    
    inline Rayd rayForPixel(const Vector2d& pixel) {
      return rayForPixel(pixel.x(), pixel.y());
    }
    
    inline void cancel() {
      m_cancelled = true;
    }
    
    inline bool isCancelled() const {
      return m_cancelled;
    }
    
    inline void uncancel() {
      m_cancelled = false;
    }
    
  protected:
    void plot(Buffer<unsigned int>& buffer, const Recti& rect, const ViewPlane::Iterator& pixel, const Colord& color);

  private:
    bool m_cancelled;
    Vector3d m_position, m_target;
    MemoizedValue<Matrix4d> m_matrix;
    std::shared_ptr<ViewPlane> m_viewPlane;
  };
}
