#pragma once

#include "core/Color.h"
#include "core/math/Rect.h"
#include "engine/raster/Rasterizer.h"
#include "render/RenderEngine.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace engine::raster {
  class RasterVisibilitySet;

  namespace detail {
    class ShadowMaps;
    struct OpenGLRasterResourceCache;
  }

  /**
    * OpenGL-backed raster executor.
    *
    * Implements the same `render::RenderEngine` contract as the CPU
    * `Rasterizer` and is selected by `RasterBackend::openGL()` for graph
    * passes that opt into GPU execution. The pipeline builds an
    * `OpenGLOffscreenContext`, prepares a triangle mesh from the scene
    * through `OpenGLRasterMeshBuilder` (which respects per-material cull
    * defaults, LOD, and an optional visibility set), then issues a single
    * GLSL pass that supports:
    *
    * * Phong-style direct lighting from up to
    *   `maxShaderDirectionalLights()` directional and
    *   `maxShaderPointLights()` point lights; extras are reported in the
    *   trace messages,
    * * material albedo from vertex color, UV, image texture, or checker
    *   patterns with optional tint,
    * * alpha test, color/depth/stencil load/store ops, blending, color
    *   write masking, depth function/bias/write toggles, stencil
    *   func/ops/masks, and explicit cull-mode overrides (Front/Back) via
    *   `GL_CULL_FACE`,
    * * an optional external shadow-texture sampled inside the fragment
    *   shader for directional light shadowing.
    *
    * `render()`, `renderDepth()`, and `renderStencil()` share the same
    * draw path; depth- and stencil-only renders skip color readback.
    * Cancellation is checked before context creation, before the FBO
    * bind, and between batch draws, matching the CPU rasterizer's
    * clean-stop contract.
    *
    * Hosts without a usable offscreen GL context (no `QGuiApplication`,
    * gated Cocoa probes, etc.) report a clear error through
    * `availabilityError()` and throw from `render()` rather than
    * silently producing an empty buffer.
    */
  class OpenGLRasterizer : public render::RenderEngine {
  public:
    explicit OpenGLRasterizer(std::shared_ptr<render::Scene> scene);
    OpenGLRasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene);
    ~OpenGLRasterizer() override;

    std::shared_ptr<render::RenderEngine> cloneForRender() const override;
    void render(Buffer<unsigned int>& buffer) override;
    void render(Buffer<Colord>& buffer) override;
    void renderDepth(Buffer<double>& buffer);
    void renderStencil(Buffer<std::uint8_t>& buffer);
    void cancel() override;
    void uncancel() override;

    static std::string statusMessage();

    /**
      * Returns the process-wide shared GL resource cache. Every
      * `OpenGLRasterizer` instance pulls its context, shader program,
      * image textures, and vertex/index buffers from this cache so that
      * graph-driven frame loops — which construct a fresh
      * `OpenGLRasterizer` per pass through `RasterBackend::createEngine` —
      * still hit warm caches across frames. Callers should not retain the
      * returned pointer beyond their immediate use.
      */
    static std::shared_ptr<detail::OpenGLRasterResourceCache> sharedResources();

    static constexpr int maxShaderDirectionalLights() {
      return 8;
    }
    static constexpr int maxShaderPointLights() {
      return 8;
    }
    static void appendLightTruncationTrace(std::size_t directionalLightCount,
                                           std::size_t pointLightCount,
                                           std::vector<std::string>& traces);

    int lod() const;
    void setLod(int lod);

    int msaaSamples() const;
    void setMSAASamples(int samples);
    Rasterizer::MSAAShadingMode msaaShadingMode() const;
    void setMSAAShadingMode(Rasterizer::MSAAShadingMode mode);

    Rasterizer::CullMode cullMode() const;
    bool hasCullModeOverride() const;
    void setCullMode(Rasterizer::CullMode mode);
    void clearCullModeOverride();

    bool viewportEnabled() const;
    const Recti& viewportRect() const;
    void setViewportRect(const Recti& rect);
    void clearViewportRect();

    bool scissorTestEnabled() const;
    const Recti& scissorRect() const;
    void setScissorRect(const Recti& rect);
    void clearScissorRect();

    Rasterizer::AttachmentLoadOp colorLoadOp() const;
    void setColorLoadOp(Rasterizer::AttachmentLoadOp op);
    Rasterizer::AttachmentStoreOp colorStoreOp() const;
    void setColorStoreOp(Rasterizer::AttachmentStoreOp op);

    Rasterizer::DepthFunc depthFunc() const;
    void setDepthFunc(Rasterizer::DepthFunc func);
    double depthBias() const;
    void setDepthBias(double bias);
    double depthClearValue() const;
    void setDepthClearValue(double value);
    Rasterizer::AttachmentLoadOp depthLoadOp() const;
    void setDepthLoadOp(Rasterizer::AttachmentLoadOp op);
    Rasterizer::AttachmentStoreOp depthStoreOp() const;
    void setDepthStoreOp(Rasterizer::AttachmentStoreOp op);
    bool depthWriteEnabled() const;
    void setDepthWriteEnabled(bool enabled);

    std::uint8_t colorWriteMask() const;
    void setColorWriteMask(std::uint8_t mask);

    bool blendingEnabled() const;
    void setBlendingEnabled(bool enabled);
    Rasterizer::BlendFactor sourceBlendFactor() const;
    Rasterizer::BlendFactor destinationBlendFactor() const;
    void setBlendFactors(Rasterizer::BlendFactor source, Rasterizer::BlendFactor destination);
    Rasterizer::BlendOp blendOp() const;
    void setBlendOp(Rasterizer::BlendOp op);
    Colord blendConstantColor() const;
    double blendConstantAlpha() const;
    void setBlendConstant(const Colord& color, double alpha);

    bool alphaTestEnabled() const;
    void setAlphaTestEnabled(bool enabled);
    Rasterizer::AlphaFunc alphaFunc() const;
    double alphaReference() const;
    void setAlphaFunc(Rasterizer::AlphaFunc func, double reference);

    bool stencilTestEnabled() const;
    void setStencilTestEnabled(bool enabled);
    Rasterizer::StencilFunc stencilFunc() const;
    std::uint8_t stencilReference() const;
    std::uint8_t stencilMask() const;
    void setStencilFunc(Rasterizer::StencilFunc func, std::uint8_t reference,
                        std::uint8_t mask = 0xff);
    std::uint8_t stencilClearValue() const;
    void setStencilClearValue(std::uint8_t value);
    Rasterizer::AttachmentLoadOp stencilLoadOp() const;
    void setStencilLoadOp(Rasterizer::AttachmentLoadOp op);
    Rasterizer::AttachmentStoreOp stencilStoreOp() const;
    void setStencilStoreOp(Rasterizer::AttachmentStoreOp op);
    std::uint8_t stencilWriteMask() const;
    void setStencilWriteMask(std::uint8_t mask);
    Rasterizer::StencilOp stencilFailOp() const;
    Rasterizer::StencilOp stencilDepthFailOp() const;
    Rasterizer::StencilOp stencilPassOp() const;
    void setStencilOps(Rasterizer::StencilOp stencilFail, Rasterizer::StencilOp depthFail,
                       Rasterizer::StencilOp pass);

    bool shadowMapsEnabled() const;
    void setShadowMapsEnabled(bool enabled);
    void setExternalShadowMaps(std::shared_ptr<const detail::ShadowMaps> shadowMaps);
    void clearExternalShadowMaps();
    void setVisibilitySet(std::shared_ptr<const RasterVisibilitySet> visibilitySet);
    void clearVisibilitySet();
    std::shared_ptr<const RasterVisibilitySet> visibilitySet() const;

    bool isAvailable() const;
    std::string availabilityDetail() const;
    std::string availabilityError() const;
    const std::string& readbackTraceMessage() const;
    const std::vector<std::string>& traceMessages() const;

  private:
    Recti viewportRectFor(int width, int height) const;
    void renderOpenGL(int width, int height, Buffer<Colord>* colorTarget,
                      Buffer<double>* depthTarget, Buffer<std::uint8_t>* stencilTarget) const;
    std::string readbackTraceMessage(std::chrono::nanoseconds elapsed, bool copiedColor,
                                     bool copiedDepth, bool copiedStencil) const;
    std::string drawTraceMessage(std::chrono::nanoseconds elapsed, std::size_t triangleCount,
                                 std::size_t vertexBufferBytes, std::size_t indexBufferBytes,
                                 std::size_t imageTextureCount,
                                 std::size_t imageTextureBytes) const;
    std::string meshPreparationTraceMessage(std::chrono::nanoseconds elapsed,
                                            std::size_t triangleCount, bool cacheHit) const;
    std::string latencyBreakdownTraceMessage(std::chrono::nanoseconds makeCurrentElapsed,
                                             std::chrono::nanoseconds glFinishElapsed,
                                             std::chrono::nanoseconds doneCurrentElapsed) const;

    std::atomic<bool> m_cancelled{false};
    int m_lod{0};
    int m_msaaSamples{1};
    Rasterizer::MSAAShadingMode m_msaaShadingMode{Rasterizer::MSAAShadingMode::PerFragment};
    Rasterizer::CullMode m_cullMode{Rasterizer::CullMode::Both};
    bool m_hasCullModeOverride{false};
    bool m_viewportEnabled{false};
    Recti m_viewportRect;
    bool m_scissorTestEnabled{false};
    Recti m_scissorRect;
    Rasterizer::AttachmentLoadOp m_colorLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    Rasterizer::AttachmentStoreOp m_colorStoreOp{Rasterizer::AttachmentStoreOp::Store};
    Rasterizer::DepthFunc m_depthFunc{Rasterizer::DepthFunc::Less};
    double m_depthBias{0.0};
    double m_depthClearValue{std::numeric_limits<double>::infinity()};
    Rasterizer::AttachmentLoadOp m_depthLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    Rasterizer::AttachmentStoreOp m_depthStoreOp{Rasterizer::AttachmentStoreOp::Store};
    bool m_depthWriteEnabled{true};
    std::uint8_t m_colorWriteMask{Rasterizer::ColorWriteAll};
    bool m_blendingEnabled{false};
    Rasterizer::BlendFactor m_sourceBlendFactor{Rasterizer::BlendFactor::One};
    Rasterizer::BlendFactor m_destinationBlendFactor{Rasterizer::BlendFactor::Zero};
    Rasterizer::BlendOp m_blendOp{Rasterizer::BlendOp::Add};
    Colord m_blendConstantColor{Colord::white()};
    double m_blendConstantAlpha{1.0};
    bool m_alphaTestEnabled{false};
    Rasterizer::AlphaFunc m_alphaFunc{Rasterizer::AlphaFunc::Always};
    double m_alphaReference{0.0};
    bool m_stencilTestEnabled{false};
    Rasterizer::StencilFunc m_stencilFunc{Rasterizer::StencilFunc::Always};
    std::uint8_t m_stencilReference{0};
    std::uint8_t m_stencilMask{0xff};
    std::uint8_t m_stencilClearValue{0};
    Rasterizer::AttachmentLoadOp m_stencilLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    Rasterizer::AttachmentStoreOp m_stencilStoreOp{Rasterizer::AttachmentStoreOp::Store};
    std::uint8_t m_stencilWriteMask{0xff};
    Rasterizer::StencilOp m_stencilFailOp{Rasterizer::StencilOp::Keep};
    Rasterizer::StencilOp m_stencilDepthFailOp{Rasterizer::StencilOp::Keep};
    Rasterizer::StencilOp m_stencilPassOp{Rasterizer::StencilOp::Keep};
    bool m_shadowMapsEnabled{false};
    std::shared_ptr<const detail::ShadowMaps> m_externalShadowMaps;
    std::shared_ptr<const RasterVisibilitySet> m_visibilitySet;
    mutable std::string m_lastReadbackTraceMessage;
    mutable std::vector<std::string> m_lastTraceMessages;
    mutable std::shared_ptr<detail::OpenGLRasterResourceCache> m_resources;
  };
}
