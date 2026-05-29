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
  namespace detail {
    class ShadowMaps;
  }

  /**
    * OpenGL-backed raster executor shell.
    *
    * The class is wired into graph backend selection and owns the first
    * offscreen context/FBO capability path plus the first reusable
    * mesh-preparation and draw path. The first visible implementation renders
    * clipped triangles with material albedo into an offscreen color/depth
    * framebuffer before readback.
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

    bool isAvailable() const;
    std::string availabilityDetail() const;
    std::string availabilityError() const;
    const std::string& readbackTraceMessage() const;
    const std::vector<std::string>& traceMessages() const;

  private:
    Recti viewportRectFor(int width, int height) const;
    void renderOpenGL(Buffer<Colord>& buffer, Buffer<double>* depthTarget,
                      Buffer<std::uint8_t>* stencilTarget) const;
    std::string readbackTraceMessage(std::chrono::nanoseconds elapsed, bool copiedColor,
                                     bool copiedDepth, bool copiedStencil) const;
    std::string drawTraceMessage(std::chrono::nanoseconds elapsed, std::size_t triangleCount) const;
    std::string meshPreparationTraceMessage(std::chrono::nanoseconds elapsed,
                                            std::size_t triangleCount) const;

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
    mutable std::string m_lastReadbackTraceMessage;
    mutable std::vector<std::string> m_lastTraceMessages;
  };
}
