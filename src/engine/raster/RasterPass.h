#pragma once

#include "RasterMaterialEvaluator.h"
#include "RasterPipelineTypes.h"
#include "RasterShadowMaps.h"
#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "core/geometry/Rasterize.h"
#include "render/TilePlan.h"
#include "render/primitives/Scene.h"

#include "../TileRenderTask.h"

#include <QThreadPool>

#include <atomic>
#include <cstdint>
#include <list>
#include <memory>

namespace engine::raster::detail {

  // A render pass owns depth and optional stencil, but borrows the color target.
  // Single-sample rendering writes straight to the final buffer; full-frame MSAA
  // borrows temporary sample buffers through this wrapper, while queued MSAA uses
  // tile-local buffers.
  class PassBuffers {
  public:
    PassBuffers(const Rasterizer& rasterizer, const render::TilePlan& tilePlan,
                Buffer<Colord>& colorBuffer)
        : m_colorBuffer(colorBuffer),
          m_depthBuffer(tilePlan.width(), tilePlan.height()) {
      m_depthBuffer.clear(rasterizer.depthClearValue());
      if (rasterizer.stencilTestEnabled()) {
        m_stencilBuffer =
          std::make_unique<Buffer<std::uint8_t>>(tilePlan.width(), tilePlan.height());
        m_stencilBuffer->clear(rasterizer.stencilClearValue());
      }
    }

    Buffer<Colord>& color() {
      return m_colorBuffer;
    }

    Buffer<double>& depth() {
      return m_depthBuffer;
    }

    Buffer<std::uint8_t>* stencil() {
      return m_stencilBuffer.get();
    }

  private:
    Buffer<Colord>& m_colorBuffer;
    Buffer<double> m_depthBuffer;
    std::unique_ptr<Buffer<std::uint8_t>> m_stencilBuffer;
  };

  // Pure depth comparison state. Kept separate from depth-buffer ownership so
  // write/read-only policies can share the comparison code.
  struct DepthState {
    Rasterizer::DepthFunc func;

    inline bool pass(double incoming, double stored) const {
      switch (func) {
      case Rasterizer::DepthFunc::Never:
        return false;
      case Rasterizer::DepthFunc::Less:
        return incoming < stored;
      case Rasterizer::DepthFunc::Equal:
        return incoming == stored;
      case Rasterizer::DepthFunc::LessEqual:
        return incoming <= stored;
      case Rasterizer::DepthFunc::Greater:
        return incoming > stored;
      case Rasterizer::DepthFunc::GreaterEqual:
        return incoming >= stored;
      case Rasterizer::DepthFunc::NotEqual:
        return incoming != stored;
      case Rasterizer::DepthFunc::Always:
        return true;
      }
      return false;
    }
  };

  // Pure stencil state: compare function plus update operations for
  // stencil-fail, depth-fail, and pass outcomes.
  struct StencilState {
    Rasterizer::StencilFunc func;
    std::uint8_t reference;
    std::uint8_t mask;
    std::uint8_t writeMask;
    Rasterizer::StencilOp failOp;
    Rasterizer::StencilOp depthFailOp;
    Rasterizer::StencilOp passOp;

    inline bool pass(std::uint8_t stored) const {
      const std::uint8_t lhs = reference & mask;
      const std::uint8_t rhs = stored & mask;
      switch (func) {
      case Rasterizer::StencilFunc::Never:
        return false;
      case Rasterizer::StencilFunc::Less:
        return lhs < rhs;
      case Rasterizer::StencilFunc::Equal:
        return lhs == rhs;
      case Rasterizer::StencilFunc::LessEqual:
        return lhs <= rhs;
      case Rasterizer::StencilFunc::Greater:
        return lhs > rhs;
      case Rasterizer::StencilFunc::GreaterEqual:
        return lhs >= rhs;
      case Rasterizer::StencilFunc::NotEqual:
        return lhs != rhs;
      case Rasterizer::StencilFunc::Always:
        return true;
      }
      return false;
    }

    inline std::uint8_t apply(Rasterizer::StencilOp op, std::uint8_t current) const {
      switch (op) {
      case Rasterizer::StencilOp::Keep:
        return current;
      case Rasterizer::StencilOp::Zero:
        return 0;
      case Rasterizer::StencilOp::Replace:
        return reference;
      case Rasterizer::StencilOp::IncrementClamp:
        return current == 0xFF ? current : static_cast<std::uint8_t>(current + 1);
      case Rasterizer::StencilOp::DecrementClamp:
        return current == 0 ? current : static_cast<std::uint8_t>(current - 1);
      case Rasterizer::StencilOp::Invert:
        return static_cast<std::uint8_t>(~current);
      }
      return current;
    }

    inline std::uint8_t update(Rasterizer::StencilOp op, std::uint8_t current) const {
      const std::uint8_t updated = apply(op, current);
      return static_cast<std::uint8_t>((current & ~writeMask) | (updated & writeMask));
    }
  };

  // Null object for disabled stencil. It has the same interface as the real
  // stencil policy, so the inner loop does not branch on "is stencil enabled".
  struct NoStencilPolicy {
    inline bool pass(int, int) const {
      return true;
    }
    inline std::uint8_t value(int, int) const {
      return 0;
    }
    inline void onStencilFail(int, int) const {
    }
    inline void onDepthFail(int, int) const {
    }
    inline void onPass(int, int) const {
    }
  };

  // Real stencil policy: owns access to the pass stencil buffer and applies the
  // configured operation at each fragment outcome.
  template<class BufferView>
  struct RasterStencilPolicy {
    BufferView stencilBuffer;
    StencilState state;

    inline bool pass(int x, int y) const {
      return state.pass(stencilBuffer.at(x, y));
    }

    inline std::uint8_t value(int x, int y) const {
      return stencilBuffer.at(x, y);
    }

    inline void onStencilFail(int x, int y) const {
      update(x, y, state.failOp);
    }

    inline void onDepthFail(int x, int y) const {
      update(x, y, state.depthFailOp);
    }

    inline void onPass(int x, int y) const {
      update(x, y, state.passOp);
    }

  private:
    inline void update(int x, int y, Rasterizer::StencilOp op) const {
      stencilBuffer.at(x, y) = state.update(op, stencilBuffer.at(x, y));
    }
  };

  // Normal depth policy: compare incoming depth, then commit passing fragments
  // back into the z-buffer.
  template<class BufferView>
  struct DepthWritePolicy {
    BufferView zBuffer;
    DepthState state;

    inline bool pass(int x, int y, double depth) const {
      return state.pass(depth, zBuffer.at(x, y));
    }

    inline void write(int x, int y, double depth) const {
      zBuffer.at(x, y) = depth;
    }
  };

  // Depth-test-only policy. Useful for passes that should respect existing
  // depth without modifying it.
  template<class BufferView>
  struct DepthReadOnlyPolicy {
    BufferView zBuffer;
    DepthState state;

    inline bool pass(int x, int y, double depth) const {
      return state.pass(depth, zBuffer.at(x, y));
    }

    inline void write(int, int, double) const {
    }
  };

  // Fixed-function color output state. It resolves the shaded source color
  // against the current framebuffer value, then applies the RGB write mask.
  // There is no alpha channel in `Colord`, so alpha-style blending uses the
  // pass constant alpha because material/fragment opacity is not represented
  // in this path.
  struct ColorOutputState {
    std::uint8_t writeMask;
    bool blendEnabled;
    Rasterizer::BlendFactor sourceFactor;
    Rasterizer::BlendFactor destinationFactor;
    Rasterizer::BlendOp op;
    Colord constantColor;
    double constantAlpha;

    inline double factor(Rasterizer::BlendFactor blendFactor, const Colord& source,
                         const Colord& destination, int channel) const {
      switch (blendFactor) {
      case Rasterizer::BlendFactor::Zero:
        return 0.0;
      case Rasterizer::BlendFactor::One:
        return 1.0;
      case Rasterizer::BlendFactor::SourceColor:
        return source[channel];
      case Rasterizer::BlendFactor::OneMinusSourceColor:
        return 1.0 - source[channel];
      case Rasterizer::BlendFactor::DestinationColor:
        return destination[channel];
      case Rasterizer::BlendFactor::OneMinusDestinationColor:
        return 1.0 - destination[channel];
      case Rasterizer::BlendFactor::ConstantColor:
        return constantColor[channel];
      case Rasterizer::BlendFactor::OneMinusConstantColor:
        return 1.0 - constantColor[channel];
      case Rasterizer::BlendFactor::ConstantAlpha:
        return constantAlpha;
      case Rasterizer::BlendFactor::OneMinusConstantAlpha:
        return 1.0 - constantAlpha;
      }
      return 1.0;
    }

    inline Colord blend(const Colord& source, const Colord& destination) const {
      if (!blendEnabled) {
        return source;
      }

      Colord result;
      for (int i = 0; i != 3; ++i) {
        if (op == Rasterizer::BlendOp::Min) {
          result[i] = std::min(source[i], destination[i]);
        } else if (op == Rasterizer::BlendOp::Max) {
          result[i] = std::max(source[i], destination[i]);
        } else {
          const double sourceTerm = source[i] * factor(sourceFactor, source, destination, i);
          const double destinationTerm =
            destination[i] * factor(destinationFactor, source, destination, i);
          switch (op) {
          case Rasterizer::BlendOp::Add:
            result[i] = sourceTerm + destinationTerm;
            break;
          case Rasterizer::BlendOp::Subtract:
            result[i] = sourceTerm - destinationTerm;
            break;
          case Rasterizer::BlendOp::ReverseSubtract:
            result[i] = destinationTerm - sourceTerm;
            break;
          case Rasterizer::BlendOp::Min:
          case Rasterizer::BlendOp::Max:
            break;
          }
        }
      }
      return result;
    }

    inline Colord resolve(const Colord& source, const Colord& destination) const {
      const Colord blended = blend(source, destination);
      Colord result = destination;
      if (writeMask & Rasterizer::ColorWriteRed) {
        result[0] = blended[0];
      }
      if (writeMask & Rasterizer::ColorWriteGreen) {
        result[1] = blended[1];
      }
      if (writeMask & Rasterizer::ColorWriteBlue) {
        result[2] = blended[2];
      }
      return result;
    }
  };

  template<class BufferView>
  struct ColorOutputPolicy {
    BufferView colorBuffer;
    ColorOutputState state;

    inline void write(int x, int y, const Colord& source) const {
      const Colord destination = colorBuffer.at(x, y);
      colorBuffer.at(x, y) = state.resolve(source, destination);
    }
  };

  template<class BufferView>
  inline ColorOutputPolicy<BufferView> colorOutputPolicy(const Rasterizer& rasterizer,
                                                         BufferView colorBuffer) {
    const ColorOutputState state{
      rasterizer.colorWriteMask(),       rasterizer.blendingEnabled(),
      rasterizer.sourceBlendFactor(),    rasterizer.destinationBlendFactor(),
      rasterizer.blendOp(),              rasterizer.blendConstantColor(),
      rasterizer.blendConstantAlpha()};
    return {colorBuffer, state};
  }

  // Built-in fragment policy: material lookup and direct local material shading.
  // This is the default fixed-function fragment stage.
  struct BuiltInFragmentPolicy {
    MaterialEvaluator materialEvaluator;

    inline Colord shade(const RasterTriangle& triangle, int x, int y, double, double, double,
                        const InterpolatedFragment& fragment) const {
      return materialEvaluator.shade(triangle, x, y, fragment);
    }
  };

  // User fragment-shader policy. It adapts the internal fragment payload to
  // Rasterizer::FragmentInput and calls the callback.
  struct ShaderFragmentPolicy {
    const Rasterizer& rasterizer;

    inline Colord shade(const RasterTriangle& triangle, int x, int y, double w0b, double w1b,
                        double w2b, const InterpolatedFragment& fragment) const {
      const auto& shader = rasterizer.fragmentShader();
      const Vector3d n = fragment.normal.normalized();
      const Rasterizer::FragmentInput input{
        x, y,           fragment.depth,     Vector3d(w0b, w1b, w2b), fragment.worldPos,
        n, fragment.uv, triangle.primitive, triangle.material.get(), triangle.faceIdx};
      return shader(input);
    }
  };

  template<class ColorBuffer, class Stencil, class Depth, class Fragment, class Diagnostics>
  inline void rasterizePreparedTriangleWithPolicies(const RasterTriangle& triangle,
                                                    const Recti& clipRect, ColorBuffer colorBuffer,
                                                    const Vector2d& sampleOffset, Stencil stencil,
                                                    Depth depth, Fragment fragmentPolicy,
                                                    Diagnostics diagnostics) {
    const RasterVertex& v0 = triangle.vertices[0];
    const RasterVertex& v1 = triangle.vertices[1];
    const RasterVertex& v2 = triangle.vertices[2];

    // Hot loop boundary: core::rasterizeTriangleSampled supplies covered pixels
    // and barycentric weights; the policies decide stencil/depth/shading without
    // virtual dispatch.
    core::rasterizeTriangleSampled(
      v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, clipRect.left(), clipRect.top(), clipRect.right(),
      clipRect.bottom(), sampleOffset.x(), sampleOffset.y(),
      [&](int x, int y, double w0b, double w1b, double w2b) {
        if (!stencil.pass(x, y)) {
          stencil.onStencilFail(x, y);
          diagnostics.writeStencil(x, y, stencil.value(x, y));
          return;
        }

        const InterpolatedFragment fragment(v0, v1, v2, w0b, w1b, w2b);
        if (!depth.pass(x, y, fragment.depth)) {
          stencil.onDepthFail(x, y);
          diagnostics.writeStencil(x, y, stencil.value(x, y));
          return;
        }

        stencil.onPass(x, y);
        diagnostics.writeStencil(x, y, stencil.value(x, y));
        const Colord shaded = fragmentPolicy.shade(triangle, x, y, w0b, w1b, w2b, fragment);
        depth.write(x, y, fragment.depth);
        colorBuffer.write(x, y, shaded);
        diagnostics.writeFragment(triangle, x, y, fragment);
      });
  }

  template<class Stencil, class Depth>
  inline void rasterizeDepthOnlyPreparedTriangleWithPolicies(const RasterTriangle& triangle,
                                                             const Recti& clipRect,
                                                             const Vector2d& sampleOffset,
                                                             Stencil stencil, Depth depth) {
    const RasterVertex& v0 = triangle.vertices[0];
    const RasterVertex& v1 = triangle.vertices[1];
    const RasterVertex& v2 = triangle.vertices[2];

    // Shadow maps and other depth prepasses need coverage, stencil, and depth
    // state, but do not shade or write color. Keeping that path separate avoids
    // allocating scratch color buffers just to satisfy the normal color pass.
    core::rasterizeTriangleSampled(
      v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, clipRect.left(), clipRect.top(), clipRect.right(),
      clipRect.bottom(), sampleOffset.x(), sampleOffset.y(),
      [&](int x, int y, double w0b, double w1b, double w2b) {
        if (!stencil.pass(x, y)) {
          stencil.onStencilFail(x, y);
          return;
        }

        const InterpolatedFragment fragment(v0, v1, v2, w0b, w1b, w2b);
        if (!depth.pass(x, y, fragment.depth)) {
          stencil.onDepthFail(x, y);
          return;
        }

        stencil.onPass(x, y);
        depth.write(x, y, fragment.depth);
      });
  }

  template<class ColorBuffer, class Stencil, class Depth, class Fragment, class Diagnostics>
  inline void rasterizeTileWithPolicies(const RasterTriangleSet& triangleSet, const Recti& rect,
                                        std::size_t tileIndex, ColorBuffer colorBuffer,
                                        const Recti& clipRect, const Vector2d& sampleOffset,
                                        const std::atomic<bool>& cancelled, Stencil stencil,
                                        Depth depth, Fragment fragmentPolicy,
                                        Diagnostics diagnostics) {
    const Recti rasterRect = intersectRasterRects(rect, clipRect);
    if (rasterRectEmpty(rasterRect))
      return;

    const auto& triangles = triangleSet.triangles();
    const auto& triangleIndices = triangleSet.tileGrid().triangleIndices(tileIndex);
    for (const std::size_t triangleIndex : triangleIndices) {
      if (cancelled.load())
        return;
      rasterizePreparedTriangleWithPolicies(triangles[triangleIndex], rasterRect, colorBuffer,
                                            sampleOffset, stencil, depth, fragmentPolicy,
                                            diagnostics);
    }
  }

  template<class Stencil, class Depth>
  inline void rasterizeDepthOnlyTileWithPolicies(const RasterTriangleSet& triangleSet,
                                                 const Recti& rect, std::size_t tileIndex,
                                                 const Vector2d& sampleOffset,
                                                 const std::atomic<bool>& cancelled,
                                                 Stencil stencil, Depth depth) {
    const auto& triangles = triangleSet.triangles();
    const auto& triangleIndices = triangleSet.tileGrid().triangleIndices(tileIndex);
    for (const std::size_t triangleIndex : triangleIndices) {
      if (cancelled.load())
        return;
      rasterizeDepthOnlyPreparedTriangleWithPolicies(triangles[triangleIndex], rect, sampleOffset,
                                                     stencil, depth);
    }
  }

  template<class ColorBuffer, class Stencil, class Depth, class Fragment, class Diagnostics>
  inline void rasterizeTriangleSetWithPolicies(
    const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
    QThreadPool& threadPool, std::list<std::shared_ptr<engine::TileRenderTask>>& tasks,
    const std::atomic<bool>& cancelled, ColorBuffer colorBuffer, const Recti& clipRect,
    const Vector2d& sampleOffset, Stencil stencil, Depth depth, Fragment fragmentPolicy,
    Diagnostics diagnostics) {
    if (tilePlan.isSingleTile()) {
      // Avoid QRunnable overhead for the common single-tile path.
      rasterizeTileWithPolicies(triangleSet, tilePlan.fullRect(), 0, colorBuffer, clipRect,
                                sampleOffset, cancelled, stencil, depth, fragmentPolicy,
                                diagnostics);
      return;
    }

    engine::dispatchTileTasks(tilePlan, threadPool, tasks,
                              [&, sampleOffset, stencil, depth, fragmentPolicy,
                               diagnostics](const Recti& rect, std::size_t tileIndex) {
                                rasterizeTileWithPolicies(triangleSet, rect, tileIndex, colorBuffer,
                                                          clipRect, sampleOffset, cancelled,
                                                          stencil, depth, fragmentPolicy,
                                                          diagnostics);
                              });
  }

  template<class Stencil, class Depth>
  inline void rasterizeDepthOnlyTriangleSetWithPolicies(
    const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan, QThreadPool& threadPool,
    std::list<std::shared_ptr<engine::TileRenderTask>>& tasks, const std::atomic<bool>& cancelled,
    const Vector2d& sampleOffset, Stencil stencil, Depth depth) {
    if (tilePlan.isSingleTile()) {
      rasterizeDepthOnlyTileWithPolicies(triangleSet, tilePlan.fullRect(), 0, sampleOffset,
                                         cancelled, stencil, depth);
      return;
    }

    engine::dispatchTileTasks(tilePlan, threadPool, tasks,
                              [&, sampleOffset, stencil, depth](const Recti& rect,
                                                                 std::size_t tileIndex) {
                                rasterizeDepthOnlyTileWithPolicies(
                                  triangleSet, rect, tileIndex, sampleOffset, cancelled, stencil,
                                  depth);
                              });
  }

  template<class DepthBuffer, class Stencil, class Fragment, class RenderFn>
  inline void withPreparedTriangleDepthPolicy(const Rasterizer& rasterizer, DepthBuffer zBuffer,
                                              Stencil stencil, Fragment fragmentPolicy,
                                              RenderFn&& render) {
    const DepthState depthState{rasterizer.depthFunc()};
    if (rasterizer.depthWriteEnabled()) {
      render(stencil, DepthWritePolicy<DepthBuffer>{zBuffer, depthState}, fragmentPolicy);
    } else {
      render(stencil, DepthReadOnlyPolicy<DepthBuffer>{zBuffer, depthState}, fragmentPolicy);
    }
  }

  template<class DepthBuffer, class StencilBuffer, class RenderFn>
  inline void withPreparedTrianglePolicies(const render::Scene* scene, const Rasterizer& rasterizer,
                                           const ShadowMaps& shadowMaps, DepthBuffer zBuffer,
                                           StencilBuffer stencilBuffer, RenderFn&& render) {
    const bool useStencil = rasterizer.stencilTestEnabled();
    const bool useFragmentShader = static_cast<bool>(rasterizer.fragmentShader());

    // One dispatch tree per pass, not per pixel. This is the bridge from runtime
    // engine state to compile-time policy objects.
    if (useStencil) {
      const StencilState stencilState{rasterizer.stencilFunc(),   rasterizer.stencilReference(),
                                      rasterizer.stencilMask(),   rasterizer.stencilWriteMask(),
                                      rasterizer.stencilFailOp(), rasterizer.stencilDepthFailOp(),
                                      rasterizer.stencilPassOp()};
      RasterStencilPolicy<StencilBuffer> stencil{stencilBuffer, stencilState};
      if (useFragmentShader) {
        withPreparedTriangleDepthPolicy(rasterizer, zBuffer, stencil,
                                        ShaderFragmentPolicy{rasterizer}, render);
      } else {
        withPreparedTriangleDepthPolicy(rasterizer, zBuffer, stencil,
                                        BuiltInFragmentPolicy{MaterialEvaluator(
                                          scene, shadowMaps.empty() ? nullptr : &shadowMaps,
                                          rasterizer.camera().get())},
                                        render);
      }
    } else if (useFragmentShader) {
      withPreparedTriangleDepthPolicy(rasterizer, zBuffer, NoStencilPolicy{},
                                      ShaderFragmentPolicy{rasterizer}, render);
    } else {
      withPreparedTriangleDepthPolicy(
        rasterizer, zBuffer, NoStencilPolicy{},
        BuiltInFragmentPolicy{MaterialEvaluator(scene, shadowMaps.empty() ? nullptr : &shadowMaps,
                                                rasterizer.camera().get())},
        render);
    }
  }

}
