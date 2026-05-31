#pragma once

#include "core/Color.h"
#include "core/math/Matrix.h"
#include "core/math/Rect.h"
#include "core/math/Vector.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/OpenGLShadowTextureData.h"

#include <cstdint>
#include <optional>

namespace engine::raster::detail {
  /**
    * Per-render configuration for `OpenGLRasterDrawPass`. Populated by
    * `OpenGLRasterizer::renderOpenGL` and passed in as one bundle so
    * adding new draw-time state (attachment-load policy, future
    * attachment-set handles, Phase 3 residency knobs) doesn't require
    * touching the draw-pass constructor signature, every call site,
    * and the member-init list.
    *
    * Lives in a public-detail header so the fixed-function state
    * helpers (`OpenGLFixedFunctionState.h`) can read fields directly
    * without re-introducing per-field accessors.
    */
  struct OpenGLRasterDrawState {
    int width{0};
    int height{0};
    int samples{0};
    Recti viewportRect;
    bool scissorEnabled{false};
    Recti scissorRect;
    Rasterizer::AttachmentLoadOp colorLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    const Buffer<Colord>* loadColorAttachment{nullptr};
    Rasterizer::AttachmentStoreOp colorStoreOp{Rasterizer::AttachmentStoreOp::Store};
    std::uint8_t colorWriteMask{Rasterizer::ColorWriteAll};
    bool blendingEnabled{false};
    Rasterizer::BlendFactor sourceBlendFactor{Rasterizer::BlendFactor::One};
    Rasterizer::BlendFactor destinationBlendFactor{Rasterizer::BlendFactor::Zero};
    Rasterizer::BlendOp blendOp{Rasterizer::BlendOp::Add};
    Colord blendConstantColor{Colord::white()};
    double blendConstantAlpha{1.0};
    bool alphaTestEnabled{false};
    Rasterizer::AlphaFunc alphaFunc{Rasterizer::AlphaFunc::Always};
    double alphaReference{0.0};
    Rasterizer::DepthFunc depthFunc{Rasterizer::DepthFunc::Less};
    double depthClearValue{1.0};
    Rasterizer::AttachmentLoadOp depthLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    Rasterizer::AttachmentStoreOp depthStoreOp{Rasterizer::AttachmentStoreOp::Store};
    bool depthWriteEnabled{true};
    bool stencilTestEnabled{false};
    Rasterizer::StencilFunc stencilFunc{Rasterizer::StencilFunc::Always};
    std::uint8_t stencilReference{0};
    std::uint8_t stencilMask{0xff};
    std::uint8_t stencilClearValue{0};
    Rasterizer::AttachmentLoadOp stencilLoadOp{Rasterizer::AttachmentLoadOp::Clear};
    Rasterizer::AttachmentStoreOp stencilStoreOp{Rasterizer::AttachmentStoreOp::Store};
    std::uint8_t stencilWriteMask{0xff};
    Rasterizer::StencilOp stencilFailOp{Rasterizer::StencilOp::Keep};
    Rasterizer::StencilOp stencilDepthFailOp{Rasterizer::StencilOp::Keep};
    Rasterizer::StencilOp stencilPassOp{Rasterizer::StencilOp::Keep};
    OpenGLShadowTextureData shadowTextureData;
    Vector3d cameraPosition;
    std::optional<Matrix4d> viewProjection;
    Rasterizer::CullMode cullMode{Rasterizer::CullMode::Both};
    bool hasCullModeOverride{false};
    bool skipMeshUpload{false};
  };
}
