#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/ThinLensCamera.h"
#include "core/math/Ray.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "raytracer/samplers/JitteredSampler.h"

#include <cmath>

using namespace raytracer;

namespace {
  // Concentric mapping from a square sample to the unit disc (Shirley
  // 1997, "A Low Distortion Map Between Disk and Square"). Takes a 2D
  // sample in [-1, 1]² and returns a uniformly distributed point on
  // the unit disc. Beats naive rejection sampling because there's no
  // rejection branch *and* the mapping is bijective + low-distortion,
  // so stratification of the input square carries over to the output
  // disc — which is the load-bearing property when we feed it the
  // sampler's stratified per-pixel sub-samples (see rayForPixel below).
  void concentricMapToDisc(double a, double b, double& outU, double& outV) {
    if (a == 0.0 && b == 0.0) {
      outU = outV = 0.0;
      return;
    }

    double r, phi;
    if (a * a > b * b) {
      r = a;
      phi = (M_PI / 4.0) * (b / a);
    } else {
      r = b;
      phi = (M_PI / 2.0) - (M_PI / 4.0) * (a / b);
    }
    outU = r * std::cos(phi);
    outV = r * std::sin(phi);
  }
}

Rayd ThinLensCamera::rayForPixel(double x, double y) const {
  // Reuse the sampler's stratified [0,1]² sub-pixel offset as the
  // lens-disc coordinate. The fractional part of (x, y) IS the sampler
  // sample (Camera::render computes pixel.pixel() + sample, and
  // pixel.pixel() is integer). Map [0,1]² → [-1,1]² → disc via the
  // concentric mapping above; the disc samples then inherit whatever
  // stratification the active ViewPlane sampler provides (jittered,
  // multi-jittered, …).
  //
  // Why this matters: pure-random per-call lens sampling gives
  // O(1/√N) Monte Carlo noise on bokeh — at 1024 spp that's ~3% std
  // dev, clearly visible as graininess in out-of-focus regions. Reusing
  // the stratified input drops the convergence to O(1/N) for the
  // pixel-coverage component (the lens-disc component still has some
  // residual MC noise, but the dominant correlated noise — see Pharr
  // & Humphreys, "Physically Based Rendering" §6.2.3 — is gone).
  //
  // The price: pixel-jitter and lens-disc samples are now correlated
  // within each pixel. For typical DOF this shows up as a slight
  // smoothing of edge transitions and is not visible in practice.
  // Owen-scrambled Sobol or pad-up multi-jitter would decorrelate them
  // properly; that's a future improvement (see topics-backlog §A on
  // sampling theory).
  double subU = x - std::floor(x);
  double subV = y - std::floor(y);
  double a = 2.0 * subU - 1.0;
  double b = 2.0 * subV - 1.0;

  double u, v;
  concentricMapToDisc(a, b, u, v);
  return rayForPixelWithLens(x, y, u, v);
}

Rayd ThinLensCamera::rayForPixelWithLens(double x, double y, double lensU, double lensV) const {
  // Pinhole reference ray — origin at lens centre, target through the
  // pixel on the viewplane at +z=distance from the eye.
  Vector3d eyeOrigin = matrix() * Vector4d(0, 0, -m_distance);
  Vector3d pixel = viewPlane()->pixelAt(x, y);
  Vector3d pinholeDir = (pixel - eyeOrigin).normalized();

  // Find where this pinhole ray would hit the focal plane. focalDistance
  // is the distance from the **camera position** (the user-facing
  // location set by setPosition) to the in-focus plane along the
  // forward axis — not the distance from the internal eye/pinhole.
  // This matches what every photography app and engine means by "focal
  // distance": subject 8 m from the camera → focalDistance=8 puts it
  // in focus, regardless of how far behind the image plane the
  // math-side pinhole lives.
  //
  // The eye sits m_distance units behind the camera position along
  // -forward, so the eye-to-focal-plane distance is (m_distance +
  // m_focalDistance). Project the pinhole ray onto the forward axis to
  // get t — that's how far along the ray we have to travel to reach
  // the focal plane.
  Vector3d forward = Matrix3d(matrix()) * Vector3d(0, 0, 1);
  double t = (m_distance + m_focalDistance) / (pinholeDir * forward);
  Vector3d focalPoint = eyeOrigin + pinholeDir * t;

  // Shift the ray origin along the lens disc; the ray still has to pass
  // through focalPoint so that focal-distance geometry stays sharp. The
  // displaced origin lives on a disc of radius apertureRadius oriented
  // along the camera's local x/y plane.
  Vector3d right = Matrix3d(matrix()) * Vector3d(1, 0, 0);
  Vector3d up    = Matrix3d(matrix()) * Vector3d(0, 1, 0);
  Vector3d lensOffset = (right * lensU + up * lensV) * m_apertureRadius;
  Vector3d lensOrigin = eyeOrigin + lensOffset;

  return Rayd(lensOrigin, (focalPoint - lensOrigin).normalized());
}

void ThinLensCamera::setViewPlane(std::shared_ptr<ViewPlane> plane) {
  Camera::setViewPlane(plane);
  viewPlane()->setPixelSize(1.0 / m_zoom);

  // Auto-install a multi-sample sampler ONLY if the incoming viewplane
  // has the factory-default 1-spp sampler. ThinLens is fundamentally a
  // multi-sample camera — each per-pixel sample picks a fresh point on
  // the lens disc, and the lens-disc samples have to AVERAGE together
  // to produce DOF blur. With 1 sample per pixel, every pixel gets one
  // lens position and the output is pure noise, not bokeh (see the
  // SceneBrowser confetti regression that motivated this hook).
  //
  // The numSamples > 1 guard exists because GeneratedRayTracer's
  // RenderWindow attaches the user's chosen sampler to the viewplane
  // BEFORE calling setViewPlane on the camera — without this guard,
  // we'd silently clobber the user's "1024 spp" UI setting back to 16,
  // and the GUI render would look noisier than the rendercli render of
  // the same scene with the same UI-displayed settings.
  if (viewPlane()->sampler()->numSamples() <= 1) {
    auto jittered = std::make_shared<JitteredSampler>();
    jittered->setup(16, 83);  // 16 spp; 83 sets — same set count rendercli uses
    viewPlane()->setSampler(jittered);
  }
}

static bool dummy = CameraFactory::self().registerClass<ThinLensCamera>("ThinLensCamera");
