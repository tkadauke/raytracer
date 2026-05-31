#pragma once

#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/OpenGLRasterDrawState.h"

#include <QOpenGLFunctions>

#include <algorithm>
#include <cmath>

namespace engine::raster::detail {
  /**
    * Free-function helpers that translate `OpenGLRasterDrawState`'s
    * fixed-function fields into raw GL state calls. Lifted out of
    * `OpenGLRasterDrawPass` so the per-pass class doesn't grow every
    * time we add a state knob and so the residency-substrate work can
    * reuse them across a per-attachment-set draw scaffold.
    *
    * All entry points take a `QOpenGLFunctions*` so the caller controls
    * which function loader they're issued against; that becomes a
    * cleaner seam when the Qt → native-GL decoupling lands.
    */

  inline GLenum toGLDepthFunc(Rasterizer::DepthFunc func) {
    switch (func) {
    case Rasterizer::DepthFunc::Never:
      return GL_NEVER;
    case Rasterizer::DepthFunc::Less:
      return GL_LESS;
    case Rasterizer::DepthFunc::Equal:
      return GL_EQUAL;
    case Rasterizer::DepthFunc::LessEqual:
      return GL_LEQUAL;
    case Rasterizer::DepthFunc::Greater:
      return GL_GREATER;
    case Rasterizer::DepthFunc::GreaterEqual:
      return GL_GEQUAL;
    case Rasterizer::DepthFunc::NotEqual:
      return GL_NOTEQUAL;
    case Rasterizer::DepthFunc::Always:
      return GL_ALWAYS;
    }
    return GL_LESS;
  }

  inline GLenum toGLBlendFactor(Rasterizer::BlendFactor factor) {
    switch (factor) {
    case Rasterizer::BlendFactor::Zero:
      return GL_ZERO;
    case Rasterizer::BlendFactor::One:
      return GL_ONE;
    case Rasterizer::BlendFactor::SourceColor:
      return GL_SRC_COLOR;
    case Rasterizer::BlendFactor::OneMinusSourceColor:
      return GL_ONE_MINUS_SRC_COLOR;
    case Rasterizer::BlendFactor::SourceAlpha:
      return GL_SRC_ALPHA;
    case Rasterizer::BlendFactor::OneMinusSourceAlpha:
      return GL_ONE_MINUS_SRC_ALPHA;
    case Rasterizer::BlendFactor::DestinationColor:
      return GL_DST_COLOR;
    case Rasterizer::BlendFactor::OneMinusDestinationColor:
      return GL_ONE_MINUS_DST_COLOR;
    case Rasterizer::BlendFactor::ConstantColor:
      return GL_CONSTANT_COLOR;
    case Rasterizer::BlendFactor::OneMinusConstantColor:
      return GL_ONE_MINUS_CONSTANT_COLOR;
    case Rasterizer::BlendFactor::ConstantAlpha:
      return GL_CONSTANT_ALPHA;
    case Rasterizer::BlendFactor::OneMinusConstantAlpha:
      return GL_ONE_MINUS_CONSTANT_ALPHA;
    }
    return GL_ONE;
  }

  inline GLenum toGLBlendOp(Rasterizer::BlendOp op) {
    switch (op) {
    case Rasterizer::BlendOp::Add:
      return GL_FUNC_ADD;
    case Rasterizer::BlendOp::Subtract:
      return GL_FUNC_SUBTRACT;
    case Rasterizer::BlendOp::ReverseSubtract:
      return GL_FUNC_REVERSE_SUBTRACT;
    case Rasterizer::BlendOp::Min:
      return GL_MIN;
    case Rasterizer::BlendOp::Max:
      return GL_MAX;
    }
    return GL_FUNC_ADD;
  }

  inline GLenum toGLStencilFunc(Rasterizer::StencilFunc func) {
    switch (func) {
    case Rasterizer::StencilFunc::Never:
      return GL_NEVER;
    case Rasterizer::StencilFunc::Less:
      return GL_LESS;
    case Rasterizer::StencilFunc::Equal:
      return GL_EQUAL;
    case Rasterizer::StencilFunc::LessEqual:
      return GL_LEQUAL;
    case Rasterizer::StencilFunc::Greater:
      return GL_GREATER;
    case Rasterizer::StencilFunc::GreaterEqual:
      return GL_GEQUAL;
    case Rasterizer::StencilFunc::NotEqual:
      return GL_NOTEQUAL;
    case Rasterizer::StencilFunc::Always:
      return GL_ALWAYS;
    }
    return GL_ALWAYS;
  }

  inline GLenum toGLStencilOp(Rasterizer::StencilOp op) {
    switch (op) {
    case Rasterizer::StencilOp::Keep:
      return GL_KEEP;
    case Rasterizer::StencilOp::Zero:
      return GL_ZERO;
    case Rasterizer::StencilOp::Replace:
      return GL_REPLACE;
    case Rasterizer::StencilOp::IncrementClamp:
      return GL_INCR;
    case Rasterizer::StencilOp::DecrementClamp:
      return GL_DECR;
    case Rasterizer::StencilOp::Invert:
      return GL_INVERT;
    }
    return GL_KEEP;
  }

  inline GLfloat normalizedDepthClearValue(double depth) {
    if (!std::isfinite(depth)) {
      return depth < 0.0 ? 0.0f : 1.0f;
    }
    const double positiveDepth = std::max(0.0, depth);
    return static_cast<GLfloat>(std::clamp(positiveDepth / (positiveDepth + 1.0), 0.0, 1.0));
  }

  inline void applyDepth(QOpenGLFunctions* functions, const OpenGLRasterDrawState& state) {
    functions->glDepthFunc(toGLDepthFunc(state.depthFunc));
    functions->glDepthMask(state.depthWriteEnabled ? GL_TRUE : GL_FALSE);
  }

  inline void applyScissor(QOpenGLFunctions* functions, const OpenGLRasterDrawState& state,
                           int framebufferHeight) {
    if (!state.scissorEnabled) {
      functions->glDisable(GL_SCISSOR_TEST);
      return;
    }
    functions->glEnable(GL_SCISSOR_TEST);
    functions->glScissor(state.scissorRect.left(), framebufferHeight - state.scissorRect.bottom(),
                         state.scissorRect.width(), state.scissorRect.height());
  }

  inline void applyColorWriteMask(QOpenGLFunctions* functions, const OpenGLRasterDrawState& state) {
    functions->glColorMask((state.colorWriteMask & Rasterizer::ColorWriteRed) != 0,
                           (state.colorWriteMask & Rasterizer::ColorWriteGreen) != 0,
                           (state.colorWriteMask & Rasterizer::ColorWriteBlue) != 0, GL_TRUE);
  }

  inline void applyBlending(QOpenGLFunctions* functions, const OpenGLRasterDrawState& state) {
    if (!state.blendingEnabled) {
      functions->glDisable(GL_BLEND);
      return;
    }
    functions->glEnable(GL_BLEND);
    functions->glBlendColor(
      static_cast<GLclampf>(std::clamp(state.blendConstantColor.r(), 0.0, 1.0)),
      static_cast<GLclampf>(std::clamp(state.blendConstantColor.g(), 0.0, 1.0)),
      static_cast<GLclampf>(std::clamp(state.blendConstantColor.b(), 0.0, 1.0)),
      static_cast<GLclampf>(std::clamp(state.blendConstantAlpha, 0.0, 1.0)));
    functions->glBlendFunc(toGLBlendFactor(state.sourceBlendFactor),
                           toGLBlendFactor(state.destinationBlendFactor));
    functions->glBlendEquation(toGLBlendOp(state.blendOp));
  }

  inline void applyCullMode(QOpenGLFunctions* functions, const OpenGLRasterDrawState& state) {
    if (!state.hasCullModeOverride || state.cullMode == Rasterizer::CullMode::Both) {
      functions->glDisable(GL_CULL_FACE);
      return;
    }
    functions->glEnable(GL_CULL_FACE);
    functions->glFrontFace(GL_CCW);
    functions->glCullFace(state.cullMode == Rasterizer::CullMode::Back ? GL_BACK : GL_FRONT);
  }

  inline void applyStencil(QOpenGLFunctions* functions, const OpenGLRasterDrawState& state) {
    if (!state.stencilTestEnabled) {
      functions->glDisable(GL_STENCIL_TEST);
      functions->glStencilMask(0xff);
      return;
    }
    functions->glEnable(GL_STENCIL_TEST);
    functions->glStencilFunc(toGLStencilFunc(state.stencilFunc), state.stencilReference,
                             state.stencilMask);
    functions->glStencilMask(state.stencilWriteMask);
    functions->glStencilOp(toGLStencilOp(state.stencilFailOp),
                           toGLStencilOp(state.stencilDepthFailOp),
                           toGLStencilOp(state.stencilPassOp));
  }

  /**
    * Returns the fixed-function GL state to the conservative defaults
    * the draw pass and tests expect at scope exit. Mirrors what every
    * `apply*` above might have changed.
    */
  inline void resetFixedFunctionState(QOpenGLFunctions* functions) {
    functions->glDisable(GL_SCISSOR_TEST);
    functions->glDisable(GL_BLEND);
    functions->glDisable(GL_STENCIL_TEST);
    functions->glDisable(GL_CULL_FACE);
    functions->glStencilMask(0xff);
    functions->glDepthMask(GL_TRUE);
    functions->glDepthFunc(GL_LESS);
    functions->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }
}
