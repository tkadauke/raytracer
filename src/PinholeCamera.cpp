#include "PinholeCamera.h"
#include "Buffer.h"
#include "Raytracer.h"
#include "LinearInterpolation.h"
#include "Ray.h"

void PinholeCamera::render(Raytracer* raytracer, Buffer& buffer) {
  Matrix4d m = matrix();

  Vector3d position = m * Vector3d(0, 0, -5);
  Vector3d topLeft = m * Vector3d(-4, -3, 0),
           topRight = m * Vector3d(4, -3, 0),
           bottomLeft = m * Vector3d(-4, 3, 0),
           bottomRight = m * Vector3d(4, 3, 0);

  LinearInterpolation<Vector3d> leftEnd(topLeft, bottomLeft, buffer.height()), rightEnd(topRight, bottomRight, buffer.height());

  for (LinearInterpolation<Vector3d>::Iterator left = leftEnd.begin(), right = rightEnd.begin(); left != leftEnd.end(); ++left, ++right) {
    LinearInterpolation<Vector3d> row(*left, *right, buffer.width());
    for (LinearInterpolation<Vector3d>::Iterator pixel = row.begin(); pixel != row.end(); ++pixel) {
      Ray ray(position, (*pixel - position).normalized());
      buffer[left.current()][pixel.current()] = raytracer->rayColor(ray);
    }
  }
}
