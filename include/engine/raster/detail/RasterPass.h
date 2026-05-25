#pragma once

#include "engine/raster/detail/RasterMaterialEvaluator.h"
#include "engine/raster/detail/RasterMSAA.h"
#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "core/geometry/Rasterize.h"
#include "render/TilePlan.h"
#include "render/primitives/Scene.h"

#include "engine/TileRenderTask.h"

#include <QThreadPool>

#include <atomic>
#include <cstdint>
#include <list>
#include <memory>

namespace engine::raster::detail {

  template<class T>
  bool rasterBufferMatches(const Buffer<T>* buffer, int width, int height) {
    return buffer && buffer->width() == width && buffer->height() == height;
  }

  template<class T>
  void copyRasterBuffer(Buffer<T>& target, const Buffer<T>& source) {
    for (int y = 0; y != target.height(); ++y)
      for (int x = 0; x != target.width(); ++x)
        target[y][x] = source[y][x];
  }

  // A render pass owns depth and optional stencil, but borrows the color target.
  // Single-sample rendering writes straight to the final buffer; full-frame MSAA
  // borrows temporary sample buffers through this wrapper, while queued MSAA uses
  // tile-local buffers.
  class PassBuffers {
  public:
    PassBuffers(const Rasterizer& rasterizer, const render::TilePlan& tilePlan,
                Buffer<Colord>& colorBuffer, bool useExternalAttachments = true);

    ~PassBuffers();

    Buffer<Colord>& color();

    Buffer<double>& depth();

    Buffer<std::uint8_t>* stencil();

  private:
    Buffer<Colord>& m_colorBuffer;
    Buffer<double> m_depthBuffer;
    std::unique_ptr<Buffer<std::uint8_t>> m_stencilBuffer;
    Buffer<double>* m_depthAttachment{nullptr};
    Buffer<std::uint8_t>* m_stencilAttachment{nullptr};
    Rasterizer::AttachmentStoreOp m_depthStoreOp{Rasterizer::AttachmentStoreOp::Store};
    Rasterizer::AttachmentStoreOp m_stencilStoreOp{Rasterizer::AttachmentStoreOp::Store};
  };

  // Pure depth comparison state. Kept separate from depth-buffer ownership so
  // write/read-only policies can share the comparison code.
  struct DepthState {
    Rasterizer::DepthFunc func;
    double bias = 0.0;

    double biasedDepth(double depth) const;

    bool pass(double incoming, double stored) const;
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

    bool pass(std::uint8_t stored) const;

    std::uint8_t apply(Rasterizer::StencilOp op, std::uint8_t current) const;

    std::uint8_t update(Rasterizer::StencilOp op, std::uint8_t current) const;
  };

  struct AlphaTestState {
    bool enabled;
    Rasterizer::AlphaFunc func;
    double reference;

    bool pass(double alpha) const;
  };

  // Null object for disabled stencil. It has the same interface as the real
  // stencil policy, so the inner loop does not branch on "is stencil enabled".
  struct NoStencilPolicy {
    bool pass(int, int) const;
    std::uint8_t value(int, int) const;
    void onStencilFail(int, int) const;
    void onDepthFail(int, int) const;
    void onPass(int, int) const;
  };

  // Real stencil policy: owns access to the pass stencil buffer and applies the
  // configured operation at each fragment outcome.
  template<class BufferView>
  struct RasterStencilPolicy {
    BufferView stencilBuffer;
    StencilState state;

    bool pass(int x, int y) const {
      return state.pass(stencilBuffer.at(x, y));
    }

    std::uint8_t value(int x, int y) const {
      return stencilBuffer.at(x, y);
    }

    void onStencilFail(int x, int y) const {
      update(x, y, state.failOp);
    }

    void onDepthFail(int x, int y) const {
      update(x, y, state.depthFailOp);
    }

    void onPass(int x, int y) const {
      update(x, y, state.passOp);
    }

  private:
    void update(int x, int y, Rasterizer::StencilOp op) const {
      stencilBuffer.at(x, y) = state.update(op, stencilBuffer.at(x, y));
    }
  };

  // Normal depth policy: compare incoming depth, then commit passing fragments
  // back into the z-buffer.
  template<class BufferView>
  struct DepthWritePolicy {
    BufferView zBuffer;
    DepthState state;

    double biasedDepth(double depth) const {
      return state.biasedDepth(depth);
    }

    bool pass(int x, int y, double depth) const {
      return state.pass(biasedDepth(depth), zBuffer.at(x, y));
    }

    void write(int x, int y, double depth) const {
      zBuffer.at(x, y) = biasedDepth(depth);
    }
  };

  // Depth-test-only policy. Useful for passes that should respect existing
  // depth without modifying it.
  template<class BufferView>
  struct DepthReadOnlyPolicy {
    BufferView zBuffer;
    DepthState state;

    double biasedDepth(double depth) const {
      return state.biasedDepth(depth);
    }

    bool pass(int x, int y, double depth) const {
      return state.pass(biasedDepth(depth), zBuffer.at(x, y));
    }

    void write(int, int, double) const {
    }
  };

  // Fixed-function color output state. It resolves the shaded source color
  // against the current framebuffer value, then applies the RGB write mask.
  // Alpha is transient per fragment; the framebuffer remains RGB-only.
  struct ColorOutputState {
    std::uint8_t writeMask;
    bool blendEnabled;
    Rasterizer::BlendFactor sourceFactor;
    Rasterizer::BlendFactor destinationFactor;
    Rasterizer::BlendOp op;
    Colord constantColor;
    double constantAlpha;

    double factor(Rasterizer::BlendFactor blendFactor, const RasterFragment& source,
                  const Colord& destination, int channel) const;

    Colord blend(const RasterFragment& source, const Colord& destination) const;

    Colord resolve(const RasterFragment& source, const Colord& destination) const;
  };

  template<class BufferView>
  struct ColorOutputPolicy {
    BufferView colorBuffer;
    ColorOutputState state;

    void write(int x, int y, const RasterFragment& source) const {
      const Colord destination = colorBuffer.at(x, y);
      colorBuffer.at(x, y) = state.resolve(source, destination);
    }
  };

  template<class BufferView>
  ColorOutputPolicy<BufferView> colorOutputPolicy(const Rasterizer& rasterizer,
                                                  BufferView colorBuffer) {
    const ColorOutputState state{
      rasterizer.colorWriteMask(),    rasterizer.blendingEnabled(),
      rasterizer.sourceBlendFactor(), rasterizer.destinationBlendFactor(),
      rasterizer.blendOp(),           rasterizer.blendConstantColor(),
      rasterizer.blendConstantAlpha()};
    return {colorBuffer, state};
  }

  // Built-in fragment policy: material lookup and direct local material shading.
  // This is the default fixed-function fragment stage.
  struct BuiltInFragmentPolicy {
    MaterialEvaluator materialEvaluator;

    RasterFragment shade(const RasterTriangle& triangle, int x, int y, double, double, double,
                         const InterpolatedFragment& fragment) const;
  };

  // User fragment-shader policy. It adapts the internal fragment payload to
  // Rasterizer::FragmentInput and calls the callback.
  struct ShaderFragmentPolicy {
    const Rasterizer& rasterizer;

    RasterFragment shade(const RasterTriangle& triangle, int x, int y, double w0b, double w1b,
                         double w2b, const InterpolatedFragment& fragment) const;
  };

  template<class ColorBuffer, class Stencil, class Depth, class Fragment, class Diagnostics>
  void rasterizePreparedTriangleWithPolicies(const RasterTriangle& triangle, const Recti& clipRect,
                                             ColorBuffer colorBuffer, const Vector2d& sampleOffset,
                                             Stencil stencil, Depth depth, Fragment fragmentPolicy,
                                             AlphaTestState alphaTest, Diagnostics diagnostics) {
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

        const double committedDepth = depth.biasedDepth(fragment.depth);
        const RasterFragment shaded = fragmentPolicy.shade(triangle, x, y, w0b, w1b, w2b, fragment);
        if (!alphaTest.pass(shaded.alpha)) {
          return;
        }

        stencil.onPass(x, y);
        diagnostics.writeStencil(x, y, stencil.value(x, y));
        depth.write(x, y, fragment.depth);
        colorBuffer.write(x, y, shaded);
        diagnostics.writeFragment(triangle, x, y, fragment, committedDepth);
      });
  }

  template<class Fragment, class RenderFn>
  void withMSAAFragmentShadingPolicy(const Rasterizer& rasterizer,
                                     MSAAFragmentShadeCache* shadeCache, Fragment fragmentPolicy,
                                     RenderFn&& render) {
    if (shadeCache && rasterizer.msaaShadingMode() == Rasterizer::MSAAShadingMode::PerFragment) {
      render(CachedMSAAFragmentPolicy<Fragment>{fragmentPolicy, shadeCache});
      return;
    }

    render(fragmentPolicy);
  }

  template<class Stencil, class Depth>
  void rasterizeDepthOnlyPreparedTriangleWithPolicies(const RasterTriangle& triangle,
                                                      const Recti& clipRect,
                                                      const Vector2d& sampleOffset, Stencil stencil,
                                                      Depth depth) {
    const RasterVertex& v0 = triangle.vertices[0];
    const RasterVertex& v1 = triangle.vertices[1];
    const RasterVertex& v2 = triangle.vertices[2];

    // Shadow maps and other depth prepasses need coverage, stencil, and depth
    // state, but do not shade or write color. Keeping that path separate avoids
    // allocating scratch color buffers just to satisfy the normal color pass.
    core::rasterizeTriangleSampled(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, clipRect.left(),
                                   clipRect.top(), clipRect.right(), clipRect.bottom(),
                                   sampleOffset.x(), sampleOffset.y(),
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
  void rasterizeTileWithPolicies(const RasterTriangleSet& triangleSet, const Recti& rect,
                                 std::size_t tileIndex, ColorBuffer colorBuffer,
                                 const Recti& clipRect, const Vector2d& sampleOffset,
                                 const std::atomic<bool>& cancelled, Stencil stencil, Depth depth,
                                 Fragment fragmentPolicy, AlphaTestState alphaTest,
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
                                            sampleOffset, stencil, depth, fragmentPolicy, alphaTest,
                                            diagnostics);
    }
  }

  template<class Stencil, class Depth>
  void rasterizeDepthOnlyTileWithPolicies(const RasterTriangleSet& triangleSet, const Recti& rect,
                                          std::size_t tileIndex, const Vector2d& sampleOffset,
                                          const std::atomic<bool>& cancelled, Stencil stencil,
                                          Depth depth) {
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
  void rasterizeTriangleSetWithPolicies(const RasterTriangleSet& triangleSet,
                                        const render::TilePlan& tilePlan, QThreadPool& threadPool,
                                        std::list<std::shared_ptr<engine::TileRenderTask>>& tasks,
                                        const std::atomic<bool>& cancelled, ColorBuffer colorBuffer,
                                        const Recti& clipRect, const Vector2d& sampleOffset,
                                        Stencil stencil, Depth depth, Fragment fragmentPolicy,
                                        AlphaTestState alphaTest, Diagnostics diagnostics) {
    if (tilePlan.isSingleTile()) {
      // Avoid QRunnable overhead for the common single-tile path.
      rasterizeTileWithPolicies(triangleSet, tilePlan.fullRect(), 0, colorBuffer, clipRect,
                                sampleOffset, cancelled, stencil, depth, fragmentPolicy, alphaTest,
                                diagnostics);
      return;
    }

    engine::dispatchTileTasks(tilePlan, threadPool, tasks,
                              [&, sampleOffset, stencil, depth, fragmentPolicy, alphaTest,
                               diagnostics](const Recti& rect, std::size_t tileIndex) {
                                rasterizeTileWithPolicies(triangleSet, rect, tileIndex, colorBuffer,
                                                          clipRect, sampleOffset, cancelled,
                                                          stencil, depth, fragmentPolicy, alphaTest,
                                                          diagnostics);
                              });
  }

  template<class Stencil, class Depth>
  void rasterizeDepthOnlyTriangleSetWithPolicies(
    const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan, QThreadPool& threadPool,
    std::list<std::shared_ptr<engine::TileRenderTask>>& tasks, const std::atomic<bool>& cancelled,
    const Vector2d& sampleOffset, Stencil stencil, Depth depth) {
    if (tilePlan.isSingleTile()) {
      rasterizeDepthOnlyTileWithPolicies(triangleSet, tilePlan.fullRect(), 0, sampleOffset,
                                         cancelled, stencil, depth);
      return;
    }

    engine::dispatchTileTasks(
      tilePlan, threadPool, tasks,
      [&, sampleOffset, stencil, depth](const Recti& rect, std::size_t tileIndex) {
        rasterizeDepthOnlyTileWithPolicies(triangleSet, rect, tileIndex, sampleOffset, cancelled,
                                           stencil, depth);
      });
  }

  template<class DepthBuffer, class Stencil, class Fragment, class RenderFn>
  void withPreparedTriangleDepthPolicy(const Rasterizer& rasterizer, DepthBuffer zBuffer,
                                       Stencil stencil, Fragment fragmentPolicy,
                                       RenderFn&& render) {
    const DepthState depthState{rasterizer.depthFunc(), rasterizer.depthBias()};
    if (rasterizer.depthWriteEnabled()) {
      render(stencil, DepthWritePolicy<DepthBuffer>{zBuffer, depthState}, fragmentPolicy);
    } else {
      render(stencil, DepthReadOnlyPolicy<DepthBuffer>{zBuffer, depthState}, fragmentPolicy);
    }
  }

  template<class DepthBuffer, class StencilBuffer, class RenderFn>
  void withPreparedTrianglePolicies(const render::Scene* scene, const Rasterizer& rasterizer,
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
        withPreparedTriangleDepthPolicy(
          rasterizer, zBuffer, stencil,
          BuiltInFragmentPolicy{
            MaterialEvaluator(scene, rasterizer.shadowMapsEnabled() ? &shadowMaps : nullptr,
                              rasterizer.camera().get())},
          render);
      }
    } else if (useFragmentShader) {
      withPreparedTriangleDepthPolicy(rasterizer, zBuffer, NoStencilPolicy{},
                                      ShaderFragmentPolicy{rasterizer}, render);
    } else {
      withPreparedTriangleDepthPolicy(
        rasterizer, zBuffer, NoStencilPolicy{},
        BuiltInFragmentPolicy{
          MaterialEvaluator(scene, rasterizer.shadowMapsEnabled() ? &shadowMaps : nullptr,
                            rasterizer.camera().get())},
        render);
    }
  }

}
