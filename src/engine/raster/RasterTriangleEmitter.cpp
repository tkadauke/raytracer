#include "engine/raster/detail/RasterTriangleEmitter.h"

#include <cmath>

namespace engine::raster::detail {
  namespace {
    // Attribute interpolation callback used by HomogeneousClipVolume. Geometry
    // is clipped in homogeneous coordinates, but the attributes remain in
    // primitive space so the later perspective-correct stage starts from
    // meaningful values.
    ClipVert interpolateClipVert(const ClipVert& from, const ClipVert& to, double t) {
      return {from.point + (to.point - from.point) * t, from.normal + (to.normal - from.normal) * t,
              from.uv + (to.uv - from.uv) * t, from.clip + (to.clip - from.clip) * t,
              Vector3d::undefined};
    }

    // Coordinate accessor passed into the generic homogeneous clipper.
    const Vector4d& clipOf(const ClipVert& vertex) {
      return vertex.clip;
    }

    // Projected signed area used only for face-culling decisions. The winding
    // sign convention is pinned by rasterizer and tessellation tests.
    double signedScreenArea(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2) {
      return (v1.screen.x() - v0.screen.x()) * (v2.screen.y() - v0.screen.y()) -
             (v1.screen.y() - v0.screen.y()) * (v2.screen.x() - v0.screen.x());
    }
  }

  std::size_t clipTriangleToView(const render::HomogeneousClipVolume& clipVolume,
                                 const std::array<ClipVert, 3>& input, ClipPolygon& clipped) {
    return clipVolume.clipTriangle(input, clipped, clipOf, interpolateClipVert);
  }

  bool TriangleCullPolicy::shouldCull(const RasterMaterialSource& materialSource,
                                      const ClipVert& v0, const ClipVert& v1,
                                      const ClipVert& v2) const {
    const Rasterizer::CullMode mode = hasOverride ? overrideMode : materialSource.defaultCullMode();
    if (mode == Rasterizer::CullMode::Both)
      return false;

    // Tessellated primitives use CCW winding when viewed from the outside. With
    // the current camera projection, front-facing triangles have negative
    // projected area and back-facing triangles have positive projected area.
    const double area = signedScreenArea(v0, v1, v2);
    if (area == 0.0)
      return false;

    return mode == Rasterizer::CullMode::Back ? area > 0.0 : area < 0.0;
  }

  RasterTriangleEmitter::RasterTriangleEmitter(const render::Scene* scene,
                                               std::shared_ptr<render::Camera> camera, int lod,
                                               const Rasterizer& rasterizer,
                                               const std::atomic<bool>& cancelled,
                                               Rasterizer::CullMode cullMode,
                                               bool hasCullModeOverride, bool applyVertexShader)
      : m_scene(scene),
        m_camera(std::move(camera)),
        m_lod(lod),
        m_rasterizer(rasterizer),
        m_clipVolume(rasterizer.nearClipDepth(), rasterizer.farClipDepth()),
        m_cullPolicy{cullMode, hasCullModeOverride},
        m_applyVertexShader(applyVertexShader),
        m_cancelled(cancelled) {
  }

  bool RasterTriangleEmitter::canCullPrimitiveBounds() const {
    return !m_applyVertexShader || !m_rasterizer.vertexShader();
  }

  bool RasterTriangleEmitter::primitiveBoundsOutsideClipVolume(
    const render::Primitive* primitive) const {
    const BoundingBoxd& bounds = primitive->boundingBox();
    return boundsOutsideClipVolume(bounds);
  }

  bool RasterTriangleEmitter::boundsOutsideClipVolume(const BoundingBoxd& bounds) const {
    if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite()) {
      return false;
    }

    // Conservative AABB/frustum reject: a primitive is skipped only when all
    // eight bounds corners are outside the same homogeneous clip plane. Any
    // mixed case may still intersect the view volume, so it is tessellated.
    std::uint8_t sharedOutCode = render::HomogeneousClipVolume::allBits();
    for (const Vector3d& corner : bounds.vertices()) {
      const Vector4d clip = m_camera->projectPointToClipSpace(corner);
      sharedOutCode &= m_clipVolume.outCode(clip);
      if (sharedOutCode == 0) {
        return false;
      }
    }

    return sharedOutCode != 0;
  }

  bool RasterTriangleEmitter::makeVertex(const ClipVert& vertex, const render::Primitive* primitive,
                                         const render::Material* material, std::uint64_t faceIdx,
                                         RasterVertex& out) const {
    Vector3d point = vertex.point;
    Vector3d normal = vertex.normal;
    Vector2d uv = vertex.uv;
    Vector3d screen = vertex.screen;

    // The optional vertex shader is deliberately late: it sees already-clipped
    // vertices and may adjust the screen-space result used by the
    // teaching/debug shader path.
    if (m_applyVertexShader) {
      if (const auto& shader = m_rasterizer.vertexShader()) {
        Rasterizer::VertexInput input{vertex.point,  vertex.normal, vertex.uv, vertex.clip,
                                      vertex.screen, primitive,     material,  faceIdx};
        const Rasterizer::VertexOutput output = shader(input);
        point = output.worldPosition;
        normal = output.normal;
        uv = output.uv;
        screen = output.screenPosition;
      }
    }

    if (screen.isUndefined() || screen.z() <= 0.0)
      return false;

    const double clipW = vertex.clip.isDefined() ? vertex.clip.w() : screen.z();
    if (clipW <= 0.0)
      return false;

    const double invW = 1.0 / clipW;
    out = {point, normal, uv, invW, screen.z() * invW, screen.x(), screen.y()};
    return true;
  }

  void RasterTriangleEmitter::uvGradients(const RasterVertex& r0, const RasterVertex& r1,
                                          const RasterVertex& r2, Vector2d& uvDx, Vector2d& uvDy) {
    const double x10 = r1.x - r0.x;
    const double y10 = r1.y - r0.y;
    const double x20 = r2.x - r0.x;
    const double y20 = r2.y - r0.y;
    const double determinant = x10 * y20 - x20 * y10;
    if (std::abs(determinant) <= 1e-12) {
      uvDx = Vector2d::null;
      uvDy = Vector2d::null;
      return;
    }

    const Vector2d uv10 = r1.uv - r0.uv;
    const Vector2d uv20 = r2.uv - r0.uv;
    uvDx = (uv10 * y20 - uv20 * y10) / determinant;
    uvDy = (uv20 * x10 - uv10 * x20) / determinant;
  }

  RasterTangentFrame RasterTriangleEmitter::tangentFrame(const RasterVertex& r0,
                                                         const RasterVertex& r1,
                                                         const RasterVertex& r2) {
    const Vector3d edge1 = r1.point - r0.point;
    const Vector3d edge2 = r2.point - r0.point;
    const Vector2d duv1 = r1.uv - r0.uv;
    const Vector2d duv2 = r2.uv - r0.uv;
    const double determinant = duv1.x() * duv2.y() - duv2.x() * duv1.y();
    if (std::abs(determinant) <= 1e-12) {
      return {};
    }

    const double invDet = 1.0 / determinant;
    const Vector3d tangent = (edge1 * duv2.y() - edge2 * duv1.y()) * invDet;
    const Vector3d bitangent = (edge2 * duv1.x() - edge1 * duv2.x()) * invDet;
    if (tangent.length() <= 1e-12 || bitangent.length() <= 1e-12) {
      return {};
    }

    return {tangent.normalized(), bitangent.normalized(), true};
  }

  bool RasterTriangleEmitter::makeTriangle(const ClipVert& v0, const ClipVert& v1,
                                           const ClipVert& v2, const render::Primitive* primitive,
                                           const std::shared_ptr<render::Material>& material,
                                           const RasterMaterialSource& materialSource,
                                           std::uint64_t faceIdx, RasterTriangle& out) const {
    RasterVertex r0, r1, r2;
    const render::Material* materialPtr = material.get();
    if (!makeVertex(v0, primitive, materialPtr, faceIdx, r0) ||
        !makeVertex(v1, primitive, materialPtr, faceIdx, r1) ||
        !makeVertex(v2, primitive, materialPtr, faceIdx, r2)) {
      return false;
    }

    Vector2d uvDx;
    Vector2d uvDy;
    uvGradients(r0, r1, r2, uvDx, uvDy);
    RasterTangentFrame frame = tangentFrame(r0, r1, r2);
    out = RasterTriangle{{{r0, r1, r2}}, primitive, material, materialSource.forFace(faceIdx),
                         frame,          uvDx,      uvDy,     faceIdx};
    return true;
  }
}
