#ifndef CAMERA_H
#define CAMERA_H

#include "Vector.h"
#include "Matrix.h"
#include "MemoizedValue.h"

class Raytracer;
class Buffer;

class Camera {
public:
  Camera() {}
  Camera(const Vector3d& position, const Vector3d& target)
    : m_canceled(false), m_position(position), m_target(target) {}
  virtual ~Camera() {}
  
  void setPosition(const Vector3d& position) {
    m_matrix.reset();
    m_position = position;
  }
  
  void setTarget(const Vector3d& target) {
    m_matrix.reset();
    m_target = target;
  }
  
  const Matrix4d& matrix();
  
  virtual void render(Raytracer* raytracer, Buffer& buffer) = 0;
  
  void cancel() {
    m_canceled = true;
  }

protected:
  bool m_canceled;

private:
  Vector3d m_position, m_target;
  MemoizedValue<Matrix4d> m_matrix;
};

#endif
