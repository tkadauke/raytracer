#ifndef CAMERA_H
#define CAMERA_H

#include "Vector.h"
#include "Matrix.h"
#include "MemoizedValue.h"

class Raytracer;
class Buffer;
class ViewPlane;

class Camera {
public:
  Camera() : m_cancelled(false), m_viewPlane(0) {}
  Camera(const Vector3d& position, const Vector3d& target)
    : m_cancelled(false), m_position(position), m_target(target), m_viewPlane(0) {}
  virtual ~Camera();
  
  void setPosition(const Vector3d& position) {
    m_matrix.reset();
    m_position = position;
  }
  
  void setTarget(const Vector3d& target) {
    m_matrix.reset();
    m_target = target;
  }
  
  void setViewPlane(ViewPlane* plane);
  ViewPlane* viewPlane();
  
  const Matrix4d& matrix();
  
  virtual void render(Raytracer* raytracer, Buffer& buffer) = 0;
  
  inline void cancel() { m_cancelled = true; }
  inline bool isCancelled() const { return m_cancelled; }
  inline void uncancel() { m_cancelled = false; }

private:
  bool m_cancelled;
  Vector3d m_position, m_target;
  MemoizedValue<Matrix4d> m_matrix;
  ViewPlane* m_viewPlane;
};

#endif
