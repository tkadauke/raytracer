#pragma once

#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/OpenGLRasterDrawState.h"
#include "engine/raster/gl/Bindings.h"

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
    * The helpers call raw `gl*` symbols (via `gl/Bindings.h`); the
    * caller must have a GL context current when invoking them, but
    * the context can be any backend — Qt-backed or native CGL.
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

  inline void applyDepth(const OpenGLRasterDrawState& state) {
    glDepthFunc(toGLDepthFunc(state.depthFunc));
    glDepthMask(state.depthWriteEnabled ? GL_TRUE : GL_FALSE);
  }

  inline void applyScissor(const OpenGLRasterDrawState& state, int framebufferHeight) {
    if (!state.scissorEnabled) {
      glDisable(GL_SCISSOR_TEST);
      return;
    }
    glEnable(GL_SCISSOR_TEST);
    glScissor(state.scissorRect.left(), framebufferHeight - state.scissorRect.bottom(),
              state.scissorRect.width(), state.scissorRect.height());
  }

  inline void applyColorWriteMask(const OpenGLRasterDrawState& state) {
    glColorMask((state.colorWriteMask & Rasterizer::ColorWriteRed) != 0,
                (state.colorWriteMask & Rasterizer::ColorWriteGreen) != 0,
                (state.colorWriteMask & Rasterizer::ColorWriteBlue) != 0, GL_TRUE);
  }

  inline void applyBlending(const OpenGLRasterDrawState& state) {
    if (!state.blendingEnabled) {
      glDisable(GL_BLEND);
      return;
    }
    glEnable(GL_BLEND);
    const Colord clampedBlendColor = state.blendConstantColor.clamped();
    glBlendColor(static_cast<GLclampf>(clampedBlendColor.r()),
                 static_cast<GLclampf>(clampedBlendColor.g()),
                 static_cast<GLclampf>(clampedBlendColor.b()),
                 static_cast<GLclampf>(std::clamp(state.blendConstantAlpha, 0.0, 1.0)));
    glBlendFunc(toGLBlendFactor(state.sourceBlendFactor),
                toGLBlendFactor(state.destinationBlendFactor));
    glBlendEquation(toGLBlendOp(state.blendOp));
  }

  inline void applyCullMode(const OpenGLRasterDrawState& state) {
    if (!state.hasCullModeOverride || state.cullMode == Rasterizer::CullMode::Both) {
      glDisable(GL_CULL_FACE);
      return;
    }
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(state.cullMode == Rasterizer::CullMode::Back ? GL_BACK : GL_FRONT);
  }

  inline void applyStencil(const OpenGLRasterDrawState& state) {
    if (!state.stencilTestEnabled) {
      glDisable(GL_STENCIL_TEST);
      glStencilMask(0xff);
      return;
    }
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(toGLStencilFunc(state.stencilFunc), state.stencilReference, state.stencilMask);
    glStencilMask(state.stencilWriteMask);
    glStencilOp(toGLStencilOp(state.stencilFailOp), toGLStencilOp(state.stencilDepthFailOp),
                toGLStencilOp(state.stencilPassOp));
  }

  /**
    * Returns the fixed-function GL state to the conservative defaults
    * the draw pass and tests expect at scope exit. Mirrors what every
    * `apply*` above might have changed.
    */
  inline void resetFixedFunctionState() {
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glStencilMask(0xff);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }
}
