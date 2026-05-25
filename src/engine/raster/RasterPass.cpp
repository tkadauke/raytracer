#include "engine/raster/detail/RasterPass.h"

#include <algorithm>

namespace engine::raster::detail {

  PassBuffers::PassBuffers(const Rasterizer& rasterizer, const render::TilePlan& tilePlan,
                           Buffer<Colord>& colorBuffer, bool useExternalAttachments)
      : m_colorBuffer(colorBuffer),
        m_depthBuffer(tilePlan.width(), tilePlan.height()),
        m_depthStoreOp(rasterizer.depthStoreOp()),
        m_stencilStoreOp(rasterizer.stencilStoreOp()) {
    const auto& attachments = rasterizer.attachmentBuffers();
    if (useExternalAttachments &&
        rasterBufferMatches(attachments.depth, tilePlan.width(), tilePlan.height())) {
      m_depthAttachment = attachments.depth;
    }

    if (m_depthAttachment && rasterizer.depthLoadOp() == Rasterizer::AttachmentLoadOp::Load) {
      copyRasterBuffer(m_depthBuffer, *m_depthAttachment);
    } else {
      m_depthBuffer.clear(rasterizer.depthClearValue());
    }

    if (useExternalAttachments &&
        rasterBufferMatches(attachments.stencil, tilePlan.width(), tilePlan.height())) {
      m_stencilAttachment = attachments.stencil;
    }

    if (rasterizer.stencilTestEnabled() || m_stencilAttachment) {
      m_stencilBuffer = std::make_unique<Buffer<std::uint8_t>>(tilePlan.width(), tilePlan.height());
      if (m_stencilAttachment && rasterizer.stencilLoadOp() == Rasterizer::AttachmentLoadOp::Load) {
        copyRasterBuffer(*m_stencilBuffer, *m_stencilAttachment);
      } else {
        m_stencilBuffer->clear(rasterizer.stencilClearValue());
      }
    }
  }

  PassBuffers::~PassBuffers() {
    if (m_depthAttachment && m_depthStoreOp == Rasterizer::AttachmentStoreOp::Store) {
      copyRasterBuffer(*m_depthAttachment, m_depthBuffer);
    }
    if (m_stencilAttachment && m_stencilBuffer &&
        m_stencilStoreOp == Rasterizer::AttachmentStoreOp::Store) {
      copyRasterBuffer(*m_stencilAttachment, *m_stencilBuffer);
    }
  }

  Buffer<Colord>& PassBuffers::color() {
    return m_colorBuffer;
  }

  Buffer<double>& PassBuffers::depth() {
    return m_depthBuffer;
  }

  Buffer<std::uint8_t>* PassBuffers::stencil() {
    return m_stencilBuffer.get();
  }

  double DepthState::biasedDepth(double depth) const {
    return depth + bias;
  }

  bool DepthState::pass(double incoming, double stored) const {
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

  bool StencilState::pass(std::uint8_t stored) const {
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

  std::uint8_t StencilState::apply(Rasterizer::StencilOp op, std::uint8_t current) const {
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

  std::uint8_t StencilState::update(Rasterizer::StencilOp op, std::uint8_t current) const {
    const std::uint8_t updated = apply(op, current);
    return static_cast<std::uint8_t>((current & ~writeMask) | (updated & writeMask));
  }

  bool AlphaTestState::pass(double alpha) const {
    if (!enabled) {
      return true;
    }
    switch (func) {
    case Rasterizer::AlphaFunc::Never:
      return false;
    case Rasterizer::AlphaFunc::Less:
      return alpha < reference;
    case Rasterizer::AlphaFunc::Equal:
      return alpha == reference;
    case Rasterizer::AlphaFunc::LessEqual:
      return alpha <= reference;
    case Rasterizer::AlphaFunc::Greater:
      return alpha > reference;
    case Rasterizer::AlphaFunc::GreaterEqual:
      return alpha >= reference;
    case Rasterizer::AlphaFunc::NotEqual:
      return alpha != reference;
    case Rasterizer::AlphaFunc::Always:
      return true;
    }
    return false;
  }

  bool NoStencilPolicy::pass(int, int) const {
    return true;
  }

  std::uint8_t NoStencilPolicy::value(int, int) const {
    return 0;
  }

  void NoStencilPolicy::onStencilFail(int, int) const {
  }

  void NoStencilPolicy::onDepthFail(int, int) const {
  }

  void NoStencilPolicy::onPass(int, int) const {
  }

  double ColorOutputState::factor(Rasterizer::BlendFactor blendFactor, const RasterFragment& source,
                                  const Colord& destination, int channel) const {
    switch (blendFactor) {
    case Rasterizer::BlendFactor::Zero:
      return 0.0;
    case Rasterizer::BlendFactor::One:
      return 1.0;
    case Rasterizer::BlendFactor::SourceColor:
      return source.color[channel];
    case Rasterizer::BlendFactor::OneMinusSourceColor:
      return 1.0 - source.color[channel];
    case Rasterizer::BlendFactor::SourceAlpha:
      return source.alpha;
    case Rasterizer::BlendFactor::OneMinusSourceAlpha:
      return 1.0 - source.alpha;
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

  Colord ColorOutputState::blend(const RasterFragment& source, const Colord& destination) const {
    if (!blendEnabled) {
      return source.color;
    }

    Colord result;
    for (int i = 0; i != 3; ++i) {
      if (op == Rasterizer::BlendOp::Min) {
        result[i] = std::min(source.color[i], destination[i]);
      } else if (op == Rasterizer::BlendOp::Max) {
        result[i] = std::max(source.color[i], destination[i]);
      } else {
        const double sourceTerm = source.color[i] * factor(sourceFactor, source, destination, i);
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

  Colord ColorOutputState::resolve(const RasterFragment& source, const Colord& destination) const {
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

  RasterFragment BuiltInFragmentPolicy::shade(const RasterTriangle& triangle, int x, int y, double,
                                              double, double,
                                              const InterpolatedFragment& fragment) const {
    return materialEvaluator.shade(triangle, x, y, fragment);
  }

  RasterFragment ShaderFragmentPolicy::shade(const RasterTriangle& triangle, int x, int y,
                                             double w0b, double w1b, double w2b,
                                             const InterpolatedFragment& fragment) const {
    const auto& shader = rasterizer.fragmentShader();
    const Vector3d n = fragment.normal.normalized();
    const Rasterizer::FragmentInput input{
      x, y,           fragment.depth,     Vector3d(w0b, w1b, w2b), fragment.worldPos,
      n, fragment.uv, triangle.primitive, triangle.material.get(), triangle.faceIdx};
    return {shader(input), 1.0};
  }
}
