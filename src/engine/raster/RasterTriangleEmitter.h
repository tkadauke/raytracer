#pragma once

#include "RasterPipelineTypes.h"

#include "core/geometry/Mesh.h"
#include "engine/raster/Rasterizer.h"
#include "render/HomogeneousClipVolume.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::raster::detail {

  // Worst-case clipped polygon storage for a triangle clipped against the near
  // plane and four viewport planes. The current bound is deliberately generous
  // so the clipper can use fixed storage during emission.
  inline constexpr std::size_t kMaxClipVertices = 32;

  // Scratch polygon type produced by homogeneous clipping before the emitter
  // fan-triangulates the surviving polygon back into RasterTriangle objects.
  using ClipPolygon = std::array<ClipVert, kMaxClipVertices>;

  // Shared clip-volume instance for the raster front end. Keeping this as a
  // helper makes outcode generation and full polygon clipping use the exact same
  // near/viewport plane definitions.
  inline const render::HomogeneousClipVolume& clipVolume() {
    static const render::HomogeneousClipVolume volume(kNearClipDepth);
    return volume;
  }

  // Attribute interpolation callback used by HomogeneousClipVolume. Geometry is
  // clipped in homogeneous coordinates, but the attributes remain in primitive
  // space so the later perspective-correct stage starts from meaningful values.
  inline ClipVert interpolateClipVert(const ClipVert& from, const ClipVert& to, double t) {
    return {from.point + (to.point - from.point) * t, from.normal + (to.normal - from.normal) * t,
            from.uv + (to.uv - from.uv) * t, from.clip + (to.clip - from.clip) * t,
            Vector3d::undefined};
  }

  // Coordinate accessor passed into the generic homogeneous clipper.
  inline const Vector4d& clipOf(const ClipVert& vertex) {
    return vertex.clip;
  }

  // Clip one triangle against the rasterizer's homogeneous view volume. Fully
  // visible triangles skip this path in RasterTriangleEmitter; this helper is
  // only for mixed in/out cases.
  inline std::size_t clipTriangleToView(const std::array<ClipVert, 3>& input,
                                        ClipPolygon& clipped) {
    return clipVolume().clipTriangle(input, clipped, clipOf, interpolateClipVert);
  }

  // Projected signed area used only for face-culling decisions. The winding sign
  // convention is pinned by rasterizer and tessellation tests.
  inline double signedScreenArea(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2) {
    return (v1.screen.x() - v0.screen.x()) * (v2.screen.y() - v0.screen.y()) -
           (v1.screen.y() - v0.screen.y()) * (v2.screen.x() - v0.screen.x());
  }

  // Small culling policy created from the public Rasterizer cull mode. The
  // emitter applies it after clipping and before RasterVertex construction so
  // culled triangles never reach tile binning or the fragment loop.
  struct TriangleCullPolicy {
    Rasterizer::CullMode mode;

    bool shouldCull(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2) const {
      if (mode == Rasterizer::CullMode::Both)
        return false;

      // Tessellated primitives use CCW winding when viewed from the
      // outside. With the current camera projection, front-facing
      // triangles have negative projected area and back-facing
      // triangles have positive projected area.
      const double area = signedScreenArea(v0, v1, v2);
      if (area == 0.0)
        return false;

      return mode == Rasterizer::CullMode::Back ? area > 0.0 : area < 0.0;
    }
  };

  // Front-end of the software raster pipeline. It walks scene leaf primitives,
  // tessellates them, projects vertices once per mesh, clips triangles in
  // homogeneous space, applies culling and the optional late vertex hook, then
  // streams prepared RasterTriangle values to the caller.
  class RasterTriangleEmitter {
  public:
    RasterTriangleEmitter(const render::Scene* scene, const std::shared_ptr<render::Camera>& camera,
                          int lod, const Rasterizer& rasterizer, const std::atomic<bool>& cancelled,
                          Rasterizer::CullMode cullMode, bool applyVertexShader)
        : m_scene(scene),
          m_camera(camera),
          m_lod(lod),
          m_rasterizer(rasterizer),
          m_cullPolicy{cullMode},
          m_applyVertexShader(applyVertexShader),
          m_cancelled(cancelled) {
    }

    template<class EmitFn>
    void forEachTriangle(EmitFn&& callback) const {
      std::uint64_t globalFaceIdx = 0;
      m_scene->forEachLeaf(
        [&](const render::Primitive* primitive, std::shared_ptr<render::Material> material) {
          if (m_cancelled.load())
            return;

          auto mesh = primitive->tessellate(m_lod);
          if (!mesh)
            return;

          const auto& vertices = mesh->vertices();
          const auto& faces = mesh->faces();
          const auto& viewPlane = *m_camera->viewPlane();
          const RasterMaterialSource materialSource = RasterMaterialSource::from(material);

          // Project each mesh vertex once per primitive. Faces then
          // reuse clip/screen data while fan-triangulating polygons.
          std::vector<ProjectedVertex> projected(vertices.size());
          for (std::size_t vi = 0; vi < vertices.size(); ++vi) {
            const auto& vertex = vertices[vi];
            const Vector4d clip = m_camera->projectPointToClipSpace(vertex.point);
            const std::uint8_t outCode = clipVolume().outCode(clip);
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

                emitPreparedTriangle(primitive, material, materialSource, globalFaceIdx, v0, v1, v2,
                                     callback);
                continue;
              }

              // Sutherland-Hodgman homogeneous clipping. The camera gives us
              // un-divided clip coordinates, so the same polygon clipper
              // handles the near plane and viewport edges before perspective
              // divide can create enormous screen coordinates.
              const std::array<ClipVert, 3> input = {{
                {vertices[face[0]].point, vertices[face[0]].normal, vertices[face[0]].uv, p0.clip,
                 p0.screen},
                {vertices[face[i]].point, vertices[face[i]].normal, vertices[face[i]].uv, p1.clip,
                 p1.screen},
                {vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                 vertices[face[i + 1]].uv, p2.clip, p2.screen},
              }};

              ClipPolygon clipped;
              const std::size_t clippedCount = clipTriangleToView(input, clipped);
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

                emitPreparedTriangle(primitive, material, materialSource, globalFaceIdx, v0, v1, v2,
                                     callback);
              }
            }
          }
        });
    }

  private:
    template<class EmitFn>
    void emitPreparedTriangle(const render::Primitive* primitive,
                              const std::shared_ptr<render::Material>& material,
                              const RasterMaterialSource& materialSource, std::uint64_t faceIdx,
                              const ClipVert& v0, const ClipVert& v1, const ClipVert& v2,
                              EmitFn& callback) const {
      if (m_cullPolicy.shouldCull(v0, v1, v2)) {
        return;
      }

      RasterTriangle triangle;
      if (makeTriangle(v0, v1, v2, primitive, material, materialSource, faceIdx, triangle)) {
        callback(triangle);
      }
    }

    bool makeVertex(const ClipVert& vertex, const render::Primitive* primitive,
                    const render::Material* material, std::uint64_t faceIdx,
                    RasterVertex& out) const {
      Vector3d point = vertex.point;
      Vector3d normal = vertex.normal;
      Vector2d uv = vertex.uv;
      Vector3d screen = vertex.screen;

      // The optional vertex shader is deliberately late: it sees
      // already-clipped vertices and may adjust the screen-space
      // result used by the teaching/debug shader path.
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
      out = {point,
             normal,
             uv,
             invW,
             screen.z() * invW,
             static_cast<int>(std::lround(screen.x())),
             static_cast<int>(std::lround(screen.y()))};
      return true;
    }

    bool makeTriangle(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2,
                      const render::Primitive* primitive,
                      const std::shared_ptr<render::Material>& material,
                      const RasterMaterialSource& materialSource, std::uint64_t faceIdx,
                      RasterTriangle& out) const {
      RasterVertex r0, r1, r2;
      const render::Material* materialPtr = material.get();
      if (!makeVertex(v0, primitive, materialPtr, faceIdx, r0) ||
          !makeVertex(v1, primitive, materialPtr, faceIdx, r1) ||
          !makeVertex(v2, primitive, materialPtr, faceIdx, r2)) {
        return false;
      }

      out = RasterTriangle{
        {{r0, r1, r2}}, primitive, material, materialSource.forFace(faceIdx), faceIdx};
      return true;
    }

    const render::Scene* m_scene;
    const std::shared_ptr<render::Camera>& m_camera;
    int m_lod;
    const Rasterizer& m_rasterizer;
    TriangleCullPolicy m_cullPolicy;
    bool m_applyVertexShader;
    const std::atomic<bool>& m_cancelled;
  };

}
