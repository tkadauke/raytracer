#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/TiltShiftCamera.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"

#include <cmath>

using namespace raytracer;

Rayd TiltShiftCamera::rayForPixelWithLens(double x, double y, double lensU, double lensV) const {
  // Pinhole reference ray, with optional lateral shift baked into
  // the principal direction. Conceptually, `setShift({sx, sy})`
  // slides the lens parallel to the sensor by `(sx, sy)` in the
  // camera's right/up basis — which is geometrically equivalent to
  // adding `(sx, sy)` to the target point in the same basis without
  // moving the eye. The projection onto the focal plane below picks
  // up the same offset, so the focal-plane convergence guarantee is
  // preserved.
  Vector3d eyeOrigin = matrix() * Vector4d(0, 0, -distance());
  Vector3d pixelPoint = viewPlane()->pixelAt(x, y);
  Vector3d right = Matrix3d(matrix()) * Vector3d(1, 0, 0);
  Vector3d up    = Matrix3d(matrix()) * Vector3d(0, 1, 0);
  Vector3d shiftedPixel = pixelPoint + right * m_shift.x() + up * m_shift.y();
  Vector3d pinholeDir = (shiftedPixel - eyeOrigin).normalized();

  // Tilt the focal-plane normal off the forward axis. Rotation is
  // around the camera's local right axis: positive tilt rotates the
  // top of the focal plane toward the camera (the canonical
  // "miniature" direction). At tilt=0 this collapses to `forward`
  // and the math reduces to ThinLens's perpendicular focal plane.
  Vector3d forward = Matrix3d(matrix()) * Vector3d(0, 0, 1);
  double tiltRad = m_tilt.radians();
  double cosT = std::cos(tiltRad);
  double sinT = std::sin(tiltRad);
  // Rodrigues for rotating `forward` around `right` by tiltRad.
  // Since `forward · right = 0` (orthogonal basis), the formula
  // simplifies to: forward * cosT + (right × forward) * sinT.
  Vector3d tiltedNormal = forward * cosT + (right ^ forward) * sinT;

  // Focal-plane intersection: plane passes through
  //   P0 = eyeOrigin + (distance + focalDistance) * forward
  // (same anchor point as ThinLens — what changes is the *normal*).
  // Ray: eyeOrigin + t * pinholeDir.
  // Solve t = ((P0 - eyeOrigin) · n) / (pinholeDir · n).
  double focalAlongForward = distance() + focalDistance();
  double numerator = focalAlongForward * (forward * tiltedNormal);
  double denom = pinholeDir * tiltedNormal;
  // denom approaches zero when pinholeDir is parallel to the tilted
  // plane (extreme tilt + grazing pixel). We don't handle that here
  // — practical tilts (≤ 60°) keep the denominator well away from
  // zero for typical FOVs.
  double t = numerator / denom;
  Vector3d focalPoint = eyeOrigin + pinholeDir * t;

  // Lens-disc origin offset — same as ThinLens. The displaced ray
  // still has to pass through `focalPoint`, so the focal-plane
  // convergence guarantee carries over: every lens sample for the
  // same pixel converges at the (now tilted) focal point, and
  // points on the tilted focal plane stay sharp.
  Vector3d lensOffset = (right * lensU + up * lensV) * apertureRadius();
  Vector3d lensOrigin = eyeOrigin + lensOffset;

  return Rayd(lensOrigin, (focalPoint - lensOrigin).normalized());
}

static bool dummy = CameraFactory::self().registerClass<TiltShiftCamera>("TiltShiftCamera");
