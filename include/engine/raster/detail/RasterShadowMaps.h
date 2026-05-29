#pragma once

#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "core/math/BoundingBox.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"
#include "render/cameras/Camera.h"
#include "render/lights/Light.h"
#include "render/viewplanes/ViewPlane.h"

#include <array>
#include <memory>
#include <vector>

namespace engine::raster::detail {

  // Orthonormal basis used by directional-light shadow cameras. It is derived
  // from light direction instead of Camera's usual world-up setup so overhead
  // lights do not hit the same degeneracy as ordinary view cameras.
  struct DirectionalShadowBasis {
    Vector3d forward;
    Vector3d right;
    Vector3d up;
  };

  DirectionalShadowBasis directionalShadowBasis(const Vector3d& lightDirection);

  Vector3d fromDirectionalShadowSpace(const DirectionalShadowBasis& basis, double x, double y,
                                      double z);

  // Light-space fit used for one directional-light shadow pass. `center` is
  // the stabilized XY center of the square orthographic projection;
  // `halfExtent` is its radius. `origin` shares that XY center and is placed
  // just before the nearest fitted point along the light direction so depth
  // precision is not wasted on empty space behind the cascade.
  struct DirectionalShadowFit {
    DirectionalShadowBasis basis;
    Vector3d center;
    Vector3d origin;
    double halfExtent;
  };

  // Snap the light-space center to the shadow-map texel grid. This keeps small
  // camera edits from moving a cascade by fractional texels, which is the source
  // of the most obvious shadow shimmer in the Modeler preview.
  Vector3d stabilizeDirectionalShadowCenter(const Vector3d& center, const Vector3d& lightDirection,
                                            double halfExtent, int shadowMapSize);

  // Orthographic camera used only for directional-light depth passes. It keeps
  // the Camera-shaped projection interface the raster emitter already consumes,
  // but does not support primary rays because no raytracing happens in this
  // pass.
  class DirectionalShadowCamera : public render::Camera {
  public:
    DirectionalShadowCamera(const Vector3d& center, const Vector3d& lightDirection,
                            double halfExtent);

    explicit DirectionalShadowCamera(const DirectionalShadowFit& fit);

    Rayd rayForPixel(double, double, render::SampleStream&) const override;

    std::shared_ptr<render::Camera> clone() const override;
    const char* fingerprintType() const override;

    Vector3d projectPointWithDepth(const Vector3d& worldPoint) const override;

    Vector4d projectPointToClipSpace(const Vector3d& worldPoint) const override;

    const Vector3d& origin() const;
    const Vector3d& forward() const;
    const Vector3d& right() const;
    const Vector3d& up() const;
    double halfExtent() const;

  private:
    DirectionalShadowCamera(const Vector3d& origin, const Vector3d& forward, const Vector3d& right,
                            const Vector3d& up, double halfExtent);

    Vector3d toCameraSpace(const Vector3d& worldPoint) const;

    Vector3d m_origin;
    Vector3d m_forward;
    Vector3d m_right;
    Vector3d m_up;
    double m_halfExtent;
  };

  // One camera-depth slice in a cascaded directional shadow map. The camera
  // defines the light-space projection; the depth buffer stores nearest occluder
  // depth for that projection; the view-depth range chooses the cascade at
  // shading time.
  struct DirectionalShadowCascade {
    std::shared_ptr<DirectionalShadowCamera> camera;
    std::unique_ptr<Buffer<double>> depthBuffer;
    double minViewDepth;
    double maxViewDepth;
  };

  // Visibility query object for one directional light. It chooses a cascade
  // from camera-space depth, projects the shaded point into that cascade, and
  // applies the configured hard/PCF/PCSS depth comparison.
  class DirectionalShadowMap {
  public:
    DirectionalShadowMap(const render::Light* light,
                         std::shared_ptr<const render::Camera> viewCamera,
                         std::vector<DirectionalShadowCascade> cascades, double bias,
                         double slopeBias, int filterRadius,
                         Rasterizer::ShadowFilterMode filterMode);

    const render::Light* light() const;

    const std::vector<DirectionalShadowCascade>& cascades() const;

    double visibility(const Vector3d& worldPos, const Vector3d& receiverNormal,
                      const Vector3d& lightDirection) const;

  private:
    double receiverBias(const Vector3d& receiverNormal, const Vector3d& lightDirection) const;

    const DirectionalShadowCascade* cascadeFor(const Vector3d& worldPos) const;

    double pcfVisibility(const DirectionalShadowCascade& cascade, int x, int y,
                         double receiverDepth, int radius, double bias) const;

    double pcssVisibility(const DirectionalShadowCascade& cascade, int x, int y,
                          double receiverDepth, double bias) const;

    int pcssFilterRadius(double receiverDepth, double blockerDepth, double bias) const;

    double sampleBlockerDepth(const DirectionalShadowCascade& cascade, int x, int y,
                              double receiverDepth, double bias) const;

    double sampleVisibility(const DirectionalShadowCascade& cascade, int x, int y,
                            double receiverDepth, double bias) const;

    const render::Light* m_light;
    std::shared_ptr<const render::Camera> m_viewCamera;
    std::vector<DirectionalShadowCascade> m_cascades;
    double m_bias;
    double m_slopeBias;
    int m_filterRadius;
    Rasterizer::ShadowFilterMode m_filterMode;
  };

  // Frame-level shadow-map collection. It stores directional-light maps built
  // before the camera pass; the material evaluator binds matching maps to its
  // prepared light list once per pass instead of searching this collection for
  // every shaded fragment.
  class ShadowMaps {
  public:
    void add(DirectionalShadowMap shadowMap);

    bool empty() const;

    bool copyFirstDirectionalDepthTo(Buffer<double>& destination) const;

    const std::vector<DirectionalShadowMap>& directionalMaps() const;

    const DirectionalShadowMap* forLight(const render::Light* light) const;

  private:
    std::vector<DirectionalShadowMap> m_directional;
  };

  // Compute the scene-bound depth interval visible to the main camera. Cascades
  // subdivide this interval; invalid or clipped-away bounds fall back to the
  // configured clip range so the shadow builder still has a finite range.
  std::pair<double, double> viewDepthRange(const render::Camera& camera,
                                           const std::array<Vector3d, 8>& corners,
                                           double nearClipDepth, double farClipDepth);

  // Practical cascade splitting blends between linear splits and logarithmic
  // splits. Linear ranges spend equal depth on every cascade; logarithmic
  // ranges spend more cascades near the camera where fixed-size shadow maps
  // need the most texel density.
  double cascadeSplitDepth(double minDepth, double maxDepth, double ratio, double splitLambda);

  // Split the camera-depth interval into cascade ranges. `splitLambda` controls
  // the linear/logarithmic blend: 0 preserves the old uniform split, 1 is fully
  // logarithmic, and the default 0.5 gives practical near-depth emphasis.
  std::vector<std::pair<double, double>> cascadeDepthRanges(double minDepth, double maxDepth,
                                                            int cascadeCount, double splitLambda);

  // Include the point where a scene-bounds edge crosses a cascade split plane.
  // This lets cascade bounds include sliced faces rather than only original box
  // corners that happen to fall inside the depth range.
  void includeDepthPlaneIntersection(std::vector<Vector3d>& points, const Vector3d& a,
                                     double depthA, const Vector3d& b, double depthB,
                                     double splitDepth);

  // Collect the scene-bounds points that define one camera-depth cascade. The
  // point set includes original AABB corners inside the depth interval and the
  // edge/split-plane intersections where the interval cuts through the AABB.
  std::vector<Vector3d> cascadePointsForDepthRange(const std::array<Vector3d, 8>& corners,
                                                   const render::Camera& camera, double minDepth,
                                                   double maxDepth);

  DirectionalShadowFit directionalShadowFitForPoints(const std::vector<Vector3d>& points,
                                                     const Vector3d& lightDirection,
                                                     double nearClipDepth, int shadowMapSize);

}
