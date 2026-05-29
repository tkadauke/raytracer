#pragma once

#include "engine/raster/detail/RasterPipelineTypes.h"

#include "core/geometry/Mesh.h"
#include "core/math/BoundingBox.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raster/RasterVisibilitySet.h"
#include "render/HomogeneousClipVolume.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::raster::detail {

  // Worst-case clipped polygon storage for a triangle clipped against the near
  // plane and four viewport planes. The current bound is deliberately generous
  // so the clipper can use fixed storage during emission.
  inline constexpr std::size_t kMaxClipVertices = 32;

  // Scratch polygon type produced by homogeneous clipping before the emitter
  // fan-triangulates the surviving polygon back into RasterTriangle objects.
  using ClipPolygon = std::array<ClipVert, kMaxClipVertices>;

  // Clip one triangle against the rasterizer's homogeneous view volume. Fully
  // visible triangles skip this path in RasterTriangleEmitter; this helper is
  // only for mixed in/out cases.
  std::size_t clipTriangleToView(const render::HomogeneousClipVolume& clipVolume,
                                 const std::array<ClipVert, 3>& input, ClipPolygon& clipped);

  // Small culling policy created from the public Rasterizer cull mode. The
  // emitter applies it after clipping and before RasterVertex construction so
  // culled triangles never reach tile binning or the fragment loop.
  struct TriangleCullPolicy {
    Rasterizer::CullMode overrideMode;
    bool hasOverride;

    bool shouldCull(const RasterMaterialSource& materialSource, const ClipVert& v0,
                    const ClipVert& v1, const ClipVert& v2) const;
  };

  // Front-end of the software raster pipeline. It walks scene leaf primitives,
  // tessellates them, projects vertices once per mesh, clips triangles in
  // homogeneous space, applies culling and the optional late vertex hook, then
  // streams prepared RasterTriangle values to the caller.
  class RasterTriangleEmitter {
  public:
    RasterTriangleEmitter(const render::Scene* scene, std::shared_ptr<render::Camera> camera,
                          int lod, const Rasterizer& rasterizer, const std::atomic<bool>& cancelled,
                          Rasterizer::CullMode cullMode, bool hasCullModeOverride,
                          bool applyVertexShader,
                          std::shared_ptr<const RasterVisibilitySet> visibilitySet = nullptr);

    template<class EmitFn>
    void forEachTriangle(EmitFn&& callback) const {
      std::uint64_t globalFaceIdx = 0;
      std::size_t leafIndex = 0;
      auto emitLeaf = [&](const render::Primitive::TransformedLeaf& leaf) {
        const std::size_t currentLeafIndex = leafIndex++;
        if (m_visibilitySet && !m_visibilitySet->leafVisible(currentLeafIndex)) {
          return;
        }

        if (m_cancelled.load())
          return;

        const render::Primitive* primitive = leaf.primitive;
        std::shared_ptr<render::Material> material = leaf.material;
        auto mesh = tessellatedMeshFor(primitive);
        if (!mesh)
          return;

        const auto& sourceVertices = mesh->vertices();
        std::vector<Mesh::Vertex> vertices;
        vertices.reserve(sourceVertices.size());
        for (const auto& vertex : sourceVertices) {
          vertices.emplace_back(leaf.transformPoint(vertex.point),
                                leaf.transformNormal(vertex.normal).normalizedOrZero(1e-12),
                                vertex.uv);
        }

        const auto& faces = mesh->faces();
        const auto& viewPlane = *m_camera->viewPlane();
        const RasterMaterialSource materialSource = RasterMaterialSource::from(material);

        // Project each mesh vertex once per primitive. Faces then
        // reuse clip/screen data while fan-triangulating polygons.
        std::vector<ProjectedVertex> projected(vertices.size());
        for (std::size_t vi = 0; vi < vertices.size(); ++vi) {
          const auto& vertex = vertices[vi];
          const Vector4d clip = m_camera->projectPointToClipSpace(vertex.point);
          const std::uint8_t outCode = m_clipVolume.outCode(clip);
          projected[vi] = {
            clip, outCode == 0 ? viewPlane.screenFromClipUnchecked(clip) : Vector3d::undefined,
            outCode};
        }

        for (std::size_t fi = 0; fi < faces.size(); ++fi, ++globalFaceIdx) {
          if (m_cancelled.load())
            return;

          const auto& face = faces[fi];
          if (face.size() < 3)
            continue;

          const auto faceColor = mesh->faceColor(fi);
          const RasterMaterialSource faceMaterialSource =
            faceColor ? materialSource.withColorOverride(*faceColor) : materialSource;

          for (std::size_t i = 1; i + 1 < face.size(); ++i) {
            const ProjectedVertex& p0 = projected[face[0]];
            const ProjectedVertex& p1 = projected[face[i]];
            const ProjectedVertex& p2 = projected[face[i + 1]];
            // Cohen-Sutherland-style bit tests: a shared outside bit rejects
            // the triangle before invoking the polygon clipper.
            if ((p0.outCode & p1.outCode & p2.outCode) != 0) {
              continue;
            }

            const std::uint8_t outCodeOr = p0.outCode | p1.outCode | p2.outCode;
            if (outCodeOr == 0) {
              ClipVert v0{vertices[face[0]].point, vertices[face[0]].normal, vertices[face[0]].uv,
                          p0.clip, p0.screen};
              ClipVert v1{vertices[face[i]].point, vertices[face[i]].normal, vertices[face[i]].uv,
                          p1.clip, p1.screen};
              ClipVert v2{vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                          vertices[face[i + 1]].uv, p2.clip, p2.screen};

              emitPreparedTriangle(primitive, material, faceMaterialSource, globalFaceIdx, v0, v1,
                                   v2, callback);
              continue;
            }

            // Sutherland-Hodgman homogeneous clipping. The camera gives us
            // un-divided clip coordinates, so the same polygon clipper handles
            // the near plane and viewport edges before perspective divide can
            // create enormous screen coordinates.
            const std::array<ClipVert, 3> input = {{
              {vertices[face[0]].point, vertices[face[0]].normal, vertices[face[0]].uv, p0.clip,
               p0.screen},
              {vertices[face[i]].point, vertices[face[i]].normal, vertices[face[i]].uv, p1.clip,
               p1.screen},
              {vertices[face[i + 1]].point, vertices[face[i + 1]].normal, vertices[face[i + 1]].uv,
               p2.clip, p2.screen},
            }};

            ClipPolygon clipped;
            const std::size_t clippedCount = clipTriangleToView(m_clipVolume, input, clipped);
            if (clippedCount < 3)
              continue;

            for (std::size_t t = 1; t + 1 < clippedCount; ++t) {
              ClipVert v0 = clipped[0];
              ClipVert v1 = clipped[t];
              ClipVert v2 = clipped[t + 1];

              if (!v0.ensureScreen(viewPlane) || !v1.ensureScreen(viewPlane) ||
                  !v2.ensureScreen(viewPlane)) {
                continue;
              }

              emitPreparedTriangle(primitive, material, faceMaterialSource, globalFaceIdx, v0, v1,
                                   v2, callback);
            }
          }
        }
      };

      if (!m_visibilitySet && canCullPrimitiveBounds()) {
        m_scene->forEachTransformedLeafInBounds(
          [&](const BoundingBoxd& bounds) { return !boundsOutsideClipVolume(bounds); }, nullptr,
          Matrix4d(), Matrix3d(), emitLeaf);
      } else {
        m_scene->forEachTransformedLeaf(nullptr, Matrix4d(), Matrix3d(), emitLeaf);
      }
    }

  private:
    bool canCullPrimitiveBounds() const;

    bool boundsOutsideClipVolume(const BoundingBoxd& bounds) const;

    std::shared_ptr<Mesh> tessellatedMeshFor(const render::Primitive* primitive) const;

    template<class EmitFn>
    void emitPreparedTriangle(const render::Primitive* primitive,
                              const std::shared_ptr<render::Material>& material,
                              const RasterMaterialSource& materialSource, std::uint64_t faceIdx,
                              const ClipVert& v0, const ClipVert& v1, const ClipVert& v2,
                              EmitFn& callback) const {
      if (m_cullPolicy.shouldCull(materialSource, v0, v1, v2)) {
        return;
      }

      RasterTriangle triangle;
      if (makeTriangle(v0, v1, v2, primitive, material, materialSource, faceIdx, triangle)) {
        callback(triangle);
      }
    }

    bool makeVertex(const ClipVert& vertex, const render::Primitive* primitive,
                    const render::Material* material, std::uint64_t faceIdx,
                    RasterVertex& out) const;

    static void uvGradients(const RasterVertex& r0, const RasterVertex& r1, const RasterVertex& r2,
                            Vector2d& uvDx, Vector2d& uvDy);

    static RasterTangentFrame tangentFrame(const RasterVertex& r0, const RasterVertex& r1,
                                           const RasterVertex& r2);

    bool makeTriangle(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2,
                      const render::Primitive* primitive,
                      const std::shared_ptr<render::Material>& material,
                      const RasterMaterialSource& materialSource, std::uint64_t faceIdx,
                      RasterTriangle& out) const;

    const render::Scene* m_scene;
    std::shared_ptr<render::Camera> m_camera;
    std::shared_ptr<const RasterVisibilitySet> m_visibilitySet;
    int m_lod;
    const Rasterizer& m_rasterizer;
    render::HomogeneousClipVolume m_clipVolume;
    TriangleCullPolicy m_cullPolicy;
    bool m_applyVertexShader;
    const std::atomic<bool>& m_cancelled;
    mutable std::unordered_map<const render::Primitive*, std::shared_ptr<Mesh>> m_tessellationCache;
  };

}
