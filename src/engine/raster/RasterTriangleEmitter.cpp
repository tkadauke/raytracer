#include "engine/raster/detail/RasterTriangleEmitter.h"

#include "render/primitives/MeshTriangle.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

  bool TriangleCullPolicy::hasDegenerateScreenWinding(const ClipVert& v0, const ClipVert& v1,
                                                      const ClipVert& v2) const {
    return signedScreenArea(v0, v1, v2) == 0.0;
  }

  bool TriangleCullPolicy::shouldCull(const RasterMaterialSource& materialSource,
                                      const render::Primitive* primitive, const ClipVert& v0,
                                      const ClipVert& v1, const ClipVert& v2) const {
    if (!hasOverride) {
      if (const auto* meshTriangle = dynamic_cast<const render::MeshTriangle*>(primitive)) {
        if (!meshTriangle->faceMetadata().safeForInferredCulling()) {
          return false;
        }
      }
    }

    const Rasterizer::CullMode mode = hasOverride ? overrideMode : materialSource.defaultCullMode();
    return shouldCull(mode, v0, v1, v2);
  }

  bool TriangleCullPolicy::shouldCull(Rasterizer::CullMode mode, const ClipVert& v0,
                                      const ClipVert& v1, const ClipVert& v2) const {
    if (mode == Rasterizer::CullMode::Both)
      return false;

    // Tessellated primitives use CCW winding when viewed from the outside. With
    // the current camera projection, front-facing triangles have negative
    // projected area and back-facing triangles have positive projected area.
    const double area = signedScreenArea(v0, v1, v2);
    return mode == Rasterizer::CullMode::Back ? area > 0.0 : area < 0.0;
  }

  RasterTriangleEmitter::RasterTriangleEmitter(
    const render::Scene* scene, std::shared_ptr<render::Camera> camera, int lod,
    const Rasterizer& rasterizer, const std::atomic<bool>& cancelled, Rasterizer::CullMode cullMode,
    bool hasCullModeOverride, bool applyVertexShader,
    std::shared_ptr<const RasterVisibilitySet> visibilitySet,
    Rasterizer::RasterRenderMetrics* metrics, bool skipCameraProjection)
      : m_scene(scene),
        m_camera(std::move(camera)),
        m_visibilitySet(std::move(visibilitySet)),
        m_lod(lod),
        m_rasterizer(rasterizer),
        m_clipVolume(rasterizer.nearClipDepth(), rasterizer.farClipDepth()),
        // When the caller skips CPU projection, screen-space cull cannot
        // run (signedScreenArea reads undefined screen coords). Force
        // CullMode::Both so the policy's `shouldCull` returns false; the
        // GPU is expected to handle face culling via GL state.
        m_cullPolicy{skipCameraProjection ? Rasterizer::CullMode::Both : cullMode,
                     skipCameraProjection ? true : hasCullModeOverride},
        m_applyVertexShader(applyVertexShader),
        m_skipCameraProjection(skipCameraProjection),
        m_cancelled(cancelled),
        m_metrics(metrics) {
  }

  bool RasterTriangleEmitter::canCullPrimitiveBounds() const {
    return !m_applyVertexShader || !m_rasterizer.vertexShader();
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

  double RasterTriangleEmitter::projectedPrimitiveExtentPixels(const BoundingBoxd& bounds) const {
    if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite() || !m_camera ||
        !m_camera->viewPlane()) {
      return 0.0;
    }

    const auto& viewPlane = *m_camera->viewPlane();
    bool hasProjectedCorner = false;
    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
    for (const Vector3d& corner : bounds.vertices()) {
      const Vector4d clip = m_camera->projectPointToClipSpace(corner);
      const Vector3d screen = viewPlane.screenFromClip(clip);
      if (screen.isUndefined()) {
        const double width = static_cast<double>(viewPlane.width());
        const double height = static_cast<double>(viewPlane.height());
        return std::sqrt(width * width + height * height);
      }
      hasProjectedCorner = true;
      minX = std::min(minX, screen.x());
      minY = std::min(minY, screen.y());
      maxX = std::max(maxX, screen.x());
      maxY = std::max(maxY, screen.y());
    }
    if (!hasProjectedCorner)
      return 0.0;

    const double width = std::max(0.0, maxX - minX);
    const double height = std::max(0.0, maxY - minY);
    return std::sqrt(width * width + height * height);
  }

  int RasterTriangleEmitter::effectiveLodFor(const render::Primitive::TransformedLeaf& leaf) const {
    const double projectedPixels = projectedPrimitiveExtentPixels(leaf.boundingBox());
    if (m_metrics) {
      m_metrics->tessellation.maxProjectedPrimitivePixels =
        std::max(m_metrics->tessellation.maxProjectedPrimitivePixels, projectedPixels);
    }

    const double maxError = m_rasterizer.maximumScreenSpaceError();
    if (m_lod <= 0 || maxError <= 0.0 || projectedPixels <= 0.0) {
      return m_lod;
    }

    int effective = m_lod;
    double toleratedError = maxError;
    while (effective > 0 && projectedPixels <= toleratedError * 32.0) {
      --effective;
      toleratedError *= 2.0;
    }
    if (m_metrics && effective < m_lod) {
      m_metrics->tessellation.screenSpaceLodReductions +=
        static_cast<std::uint64_t>(m_lod - effective);
    }
    return effective;
  }

  std::shared_ptr<Mesh>
  RasterTriangleEmitter::tessellatedMeshFor(const render::Primitive::TransformedLeaf& leaf) const {
    const render::Primitive* primitive = leaf.primitive;
    const int effectiveLod = effectiveLodFor(leaf);
    const TessellationCacheKey key{primitive, effectiveLod};
    auto cached = m_tessellationCache.find(key);
    if (cached != m_tessellationCache.end()) {
      if (m_metrics)
        ++m_metrics->tessellation.lodVariantCacheHits;
      return cached->second;
    }

    if (m_metrics)
      ++m_metrics->tessellation.lodVariantCacheMisses;
    auto mesh = primitive->tessellate(effectiveLod);
    m_tessellationCache.emplace(key, mesh);
    return mesh;
  }

  void RasterTriangleEmitter::recordMesh(const Mesh& mesh, const render::Material* material) const {
    if (!m_metrics)
      return;
    ++m_metrics->input.leafPrimitiveCount;
    ++m_metrics->input.meshCount;
    m_metrics->tessellation.generatedMeshVertices += mesh.vertices().size();
    m_metrics->tessellation.generatedMeshFaces += mesh.faces().size();
    if (material && m_seenMaterials.insert(material).second) {
      ++m_metrics->input.materialCount;
    }
  }

  void RasterTriangleEmitter::recordPreparedTriangleBeforeCulling() const {
    if (m_metrics) {
      ++m_metrics->tessellation.preparedTrianglesBeforeCulling;
    }
  }

  void RasterTriangleEmitter::recordTriangleRejectedByCulling() const {
    if (m_metrics) {
      ++m_metrics->tessellation.trianglesRejectedByCulling;
    }
  }

  void RasterTriangleEmitter::recordTriangleRejectedByWindingOrDegeneracy() const {
    if (m_metrics) {
      ++m_metrics->tessellation.trianglesRejectedByWindingOrDegeneracy;
    }
  }

  void RasterTriangleEmitter::recordTriangleAfterCulling() const {
    if (m_metrics) {
      ++m_metrics->tessellation.trianglesAfterCulling;
    }
  }

  void RasterTriangleEmitter::recordTriangleAfterClipping() const {
    if (m_metrics) {
      ++m_metrics->tessellation.trianglesAfterClipping;
    }
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

    // Camera-independent fast path: the emitter intentionally leaves
    // `screen`/`clip` undefined because the GPU does projection. The
    // downstream consumer (`OpenGLRasterMeshBuilder::vertexFor` with
    // `cameraIndependent=true`) ignores the screen-space fields. Emit
    // a vertex with sentinel zeros so the triangle survives.
    if (m_skipCameraProjection) {
      out = {point, normal, uv, 1.0, 0.0, 0.0, 0.0};
      return true;
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
