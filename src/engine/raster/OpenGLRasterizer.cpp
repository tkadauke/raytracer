#include "engine/raster/OpenGLRasterizer.h"

#include "core/Buffer.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/detail/OpenGLRasterMesh.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "render/textures/ImageTexture.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::raster {
  namespace {
    class OpenGLTextureCache {
    public:
      explicit OpenGLTextureCache(QOpenGLFunctions* functions)
          : m_functions(functions) {
      }

      ~OpenGLTextureCache() {
        for (const auto& entry : m_textures) {
          GLuint texture = entry.second;
          m_functions->glDeleteTextures(1, &texture);
        }
      }

      GLuint textureFor(const render::ImageTexture& image) {
        const auto cached = m_textures.find(&image);
        if (cached != m_textures.end()) {
          return cached->second;
        }

        GLuint texture = 0;
        m_functions->glGenTextures(1, &texture);
        if (texture == 0) {
          throw std::runtime_error("OpenGL raster backend could not allocate an image texture");
        }

        m_functions->glBindTexture(GL_TEXTURE_2D, texture);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter(image));
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter(image));
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode(image));
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode(image));
        m_functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (int level = 0; level != image.mipLevelCount(); ++level) {
          const std::vector<GLfloat> pixels = texturePixels(image.pixels(level));
          m_functions->glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, image.width(level),
                                    image.height(level), 0, GL_RGBA, GL_FLOAT, pixels.data());
        }
        m_functions->glBindTexture(GL_TEXTURE_2D, 0);
        m_textures.emplace(&image, texture);
        return texture;
      }

    private:
      static GLint minFilter(const render::ImageTexture& image) {
        switch (image.filter()) {
        case render::ImageTextureFilter::Nearest:
          return GL_NEAREST;
        case render::ImageTextureFilter::Bilinear:
          return GL_LINEAR;
        case render::ImageTextureFilter::Mipmap:
          return GL_LINEAR_MIPMAP_LINEAR;
        }
        return GL_LINEAR;
      }

      static GLint magFilter(const render::ImageTexture& image) {
        return image.filter() == render::ImageTextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
      }

      static GLint wrapMode(const render::ImageTexture& image) {
        return image.wrap() == render::ImageTextureWrap::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
      }

      static std::vector<GLfloat> texturePixels(const std::vector<Colord>& colors) {
        std::vector<GLfloat> pixels;
        pixels.reserve(colors.size() * 4);
        for (const Colord& color : colors) {
          pixels.push_back(static_cast<GLfloat>(std::clamp(color.r(), 0.0, 1.0)));
          pixels.push_back(static_cast<GLfloat>(std::clamp(color.g(), 0.0, 1.0)));
          pixels.push_back(static_cast<GLfloat>(std::clamp(color.b(), 0.0, 1.0)));
          pixels.push_back(1.0f);
        }
        return pixels;
      }

      QOpenGLFunctions* m_functions;
      std::unordered_map<const render::ImageTexture*, GLuint> m_textures;
    };

    class OpenGLRasterDrawPass {
    public:
      OpenGLRasterDrawPass(
        OpenGLOffscreenContext& context, int height, const Recti& viewportRect, bool scissorEnabled,
        const Recti& scissorRect, std::uint8_t colorWriteMask, bool blendingEnabled,
        Rasterizer::BlendFactor sourceBlendFactor, Rasterizer::BlendFactor destinationBlendFactor,
        Rasterizer::BlendOp blendOp, const Colord& blendConstantColor, double blendConstantAlpha,
        bool alphaTestEnabled, Rasterizer::AlphaFunc alphaFunc, double alphaReference,
        bool stencilTestEnabled, Rasterizer::StencilFunc stencilFunc, std::uint8_t stencilReference,
        std::uint8_t stencilMask, std::uint8_t stencilClearValue,
        Rasterizer::AttachmentLoadOp stencilLoadOp, Rasterizer::AttachmentStoreOp stencilStoreOp,
        std::uint8_t stencilWriteMask, Rasterizer::StencilOp stencilFailOp,
        Rasterizer::StencilOp stencilDepthFailOp, Rasterizer::StencilOp stencilPassOp)
          : m_context(context),
            m_height(height),
            m_viewportRect(viewportRect),
            m_scissorEnabled(scissorEnabled),
            m_scissorRect(scissorRect),
            m_colorWriteMask(colorWriteMask),
            m_blendingEnabled(blendingEnabled),
            m_sourceBlendFactor(sourceBlendFactor),
            m_destinationBlendFactor(destinationBlendFactor),
            m_blendOp(blendOp),
            m_blendConstantColor(blendConstantColor),
            m_blendConstantAlpha(blendConstantAlpha),
            m_alphaTestEnabled(alphaTestEnabled),
            m_alphaFunc(alphaFunc),
            m_alphaReference(alphaReference),
            m_stencilTestEnabled(stencilTestEnabled),
            m_stencilFunc(stencilFunc),
            m_stencilReference(stencilReference),
            m_stencilMask(stencilMask),
            m_stencilClearValue(stencilClearValue),
            m_stencilLoadOp(stencilLoadOp),
            m_stencilStoreOp(stencilStoreOp),
            m_stencilWriteMask(stencilWriteMask),
            m_stencilFailOp(stencilFailOp),
            m_stencilDepthFailOp(stencilDepthFailOp),
            m_stencilPassOp(stencilPassOp) {
      }

      std::chrono::nanoseconds render(const detail::OpenGLRasterMesh& mesh,
                                      const Colord& background, Buffer<Colord>& target,
                                      Buffer<double>* depthTarget,
                                      Buffer<std::uint8_t>* stencilTarget) {
        if (!m_context.makeCurrent()) {
          throw std::runtime_error(m_context.errorMessage());
        }
        if (!m_context.bindFramebuffer()) {
          m_context.doneCurrent();
          throw std::runtime_error(m_context.errorMessage());
        }

        try {
          draw(mesh, background);
          const auto readbackStarted = std::chrono::steady_clock::now();
          m_context.copyColorTo(target);
          if (depthTarget) {
            m_context.copyDepthTo(*depthTarget);
          }
          if (stencilTarget && m_stencilStoreOp == Rasterizer::AttachmentStoreOp::Store) {
            m_context.copyStencilTo(*stencilTarget);
          }
          const auto readbackElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - readbackStarted);
          m_context.releaseFramebuffer();
          m_context.doneCurrent();
          return readbackElapsed;
        } catch (...) {
          m_context.releaseFramebuffer();
          m_context.doneCurrent();
          throw;
        }
      }

    private:
      void draw(const detail::OpenGLRasterMesh& mesh, const Colord& background) {
        QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
        functions->glViewport(m_viewportRect.left(), openGLY(m_viewportRect),
                              m_viewportRect.width(), m_viewportRect.height());
        functions->glDisable(GL_SCISSOR_TEST);
        functions->glEnable(GL_DEPTH_TEST);
        functions->glDepthFunc(GL_LESS);
        functions->glClearColor(static_cast<GLfloat>(std::clamp(background.r(), 0.0, 1.0)),
                                static_cast<GLfloat>(std::clamp(background.g(), 0.0, 1.0)),
                                static_cast<GLfloat>(std::clamp(background.b(), 0.0, 1.0)), 1.0f);
        functions->glStencilMask(0xff);
        functions->glClearStencil(m_stencilClearValue);
        functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        applyColorWriteMask(functions);
        applyBlending(functions);
        applyStencil(functions);

        if (mesh.empty()) {
          functions->glFlush();
          resetFixedFunctionState(functions);
          return;
        }

        applyScissor(functions);

        QOpenGLShaderProgram program;
        if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                             "attribute vec4 position;\n"
                                             "attribute vec4 color;\n"
                                             "attribute vec2 uv;\n"
                                             "attribute float alphaScale;\n"
                                             "attribute vec3 lighting;\n"
                                             "attribute vec3 specular;\n"
                                             "attribute float albedoMode;\n"
                                             "varying vec4 vertexColor;\n"
                                             "varying vec2 vertexUV;\n"
                                             "varying float fragmentAlphaScale;\n"
                                             "varying vec3 fragmentLighting;\n"
                                             "varying vec3 fragmentSpecular;\n"
                                             "varying float fragmentAlbedoMode;\n"
                                             "void main() {\n"
                                             "  gl_Position = vec4(position.xyz * position.w, "
                                             "position.w);\n"
                                             "  vertexColor = color;\n"
                                             "  vertexUV = uv;\n"
                                             "  fragmentAlphaScale = alphaScale;\n"
                                             "  fragmentLighting = lighting;\n"
                                             "  fragmentSpecular = specular;\n"
                                             "  fragmentAlbedoMode = albedoMode;\n"
                                             "}\n")) {
          throw std::runtime_error("OpenGL raster backend could not compile vertex shader: " +
                                   program.log().toStdString());
        }
        if (!program.addShaderFromSourceCode(
              QOpenGLShader::Fragment, "varying vec4 vertexColor;\n"
                                       "varying vec2 vertexUV;\n"
                                       "varying float fragmentAlphaScale;\n"
                                       "varying vec3 fragmentLighting;\n"
                                       "varying vec3 fragmentSpecular;\n"
                                       "varying float fragmentAlbedoMode;\n"
                                       "uniform bool alphaTestEnabled;\n"
                                       "uniform int alphaFunc;\n"
                                       "uniform float alphaReference;\n"
                                       "uniform sampler2D imageTexture;\n"
                                       "uniform vec2 imageUVScale;\n"
                                       "uniform vec3 albedoTint;\n"
                                       "uniform vec3 checkerBright;\n"
                                       "uniform vec3 checkerDark;\n"
                                       "bool alphaPass(float alpha) {\n"
                                       "  if (!alphaTestEnabled) return true;\n"
                                       "  if (alphaFunc == 0) return false;\n"
                                       "  if (alphaFunc == 1) return alpha < alphaReference;\n"
                                       "  if (alphaFunc == 2) return alpha == alphaReference;\n"
                                       "  if (alphaFunc == 3) return alpha <= alphaReference;\n"
                                       "  if (alphaFunc == 4) return alpha > alphaReference;\n"
                                       "  if (alphaFunc == 5) return alpha >= alphaReference;\n"
                                       "  if (alphaFunc == 6) return alpha != alphaReference;\n"
                                       "  return true;\n"
                                       "}\n"
                                       "void main() {\n"
                                       "  vec4 sourceColor = vertexColor;\n"
                                       "  if (fragmentAlbedoMode > 0.5 && "
                                       "fragmentAlbedoMode < 1.5) {\n"
                                       "    sourceColor = vec4(vertexUV.x, vertexUV.y, 0.0, "
                                       "1.0);\n"
                                       "    sourceColor.a = max(max(sourceColor.r, "
                                       "sourceColor.g), sourceColor.b) * "
                                       "fragmentAlphaScale;\n"
                                       "  } else if (fragmentAlbedoMode > 1.5 && "
                                       "fragmentAlbedoMode < 2.5) {\n"
                                       "    sourceColor = texture2D(imageTexture, vertexUV * "
                                       "imageUVScale);\n"
                                       "    sourceColor.a = max(max(sourceColor.r, "
                                       "sourceColor.g), sourceColor.b) * "
                                       "fragmentAlphaScale;\n"
                                       "  } else if (fragmentAlbedoMode > 2.5 && "
                                       "fragmentAlbedoMode < 3.5) {\n"
                                       "    vec2 scaledUV = vertexUV * imageUVScale;\n"
                                       "    float checker = mod(floor(scaledUV.x) + "
                                       "floor(scaledUV.y), 2.0);\n"
                                       "    vec3 checkerColor = mix(checkerBright, checkerDark, "
                                       "step(0.5, checker));\n"
                                       "    sourceColor = vec4(checkerColor, 1.0);\n"
                                       "    sourceColor.a = max(max(sourceColor.r, "
                                       "sourceColor.g), sourceColor.b) * "
                                       "fragmentAlphaScale;\n"
                                       "  }\n"
                                       "  sourceColor.rgb *= albedoTint;\n"
                                       "  if (fragmentAlbedoMode > 0.5) {\n"
                                       "    sourceColor.a = max(max(sourceColor.r, "
                                       "sourceColor.g), sourceColor.b) * "
                                       "fragmentAlphaScale;\n"
                                       "  }\n"
                                       "  sourceColor.rgb = sourceColor.rgb * fragmentLighting + "
                                       "fragmentSpecular;\n"
                                       "  if (!alphaPass(sourceColor.a)) discard;\n"
                                       "  gl_FragColor = sourceColor;\n"
                                       "}\n")) {
          throw std::runtime_error("OpenGL raster backend could not compile fragment shader: " +
                                   program.log().toStdString());
        }
        if (!program.link()) {
          throw std::runtime_error("OpenGL raster backend could not link shader program: " +
                                   program.log().toStdString());
        }
        if (!program.bind()) {
          throw std::runtime_error("OpenGL raster backend could not bind shader program");
        }
        program.setUniformValue("alphaTestEnabled", m_alphaTestEnabled);
        program.setUniformValue("alphaFunc", static_cast<int>(m_alphaFunc));
        program.setUniformValue("alphaReference",
                                static_cast<GLfloat>(std::clamp(m_alphaReference, 0.0, 1.0)));
        program.setUniformValue("imageTexture", 0);

        QOpenGLBuffer vertexBuffer(QOpenGLBuffer::VertexBuffer);
        QOpenGLBuffer indexBuffer(QOpenGLBuffer::IndexBuffer);
        if (!vertexBuffer.create() || !indexBuffer.create()) {
          program.release();
          throw std::runtime_error("OpenGL raster backend could not create GPU buffers");
        }

        vertexBuffer.bind();
        vertexBuffer.allocate(
          mesh.vertices().data(),
          static_cast<int>(mesh.vertices().size() * sizeof(detail::OpenGLRasterMesh::Vertex)));
        indexBuffer.bind();
        indexBuffer.allocate(mesh.indices().data(),
                             static_cast<int>(mesh.indices().size() * sizeof(std::uint32_t)));

        const int positionLocation = program.attributeLocation("position");
        const int colorLocation = program.attributeLocation("color");
        const int uvLocation = program.attributeLocation("uv");
        const int alphaScaleLocation = program.attributeLocation("alphaScale");
        const int lightingLocation = program.attributeLocation("lighting");
        const int specularLocation = program.attributeLocation("specular");
        const int albedoModeLocation = program.attributeLocation("albedoMode");
        if (positionLocation < 0 || colorLocation < 0 || uvLocation < 0 || alphaScaleLocation < 0 ||
            lightingLocation < 0 || specularLocation < 0 || albedoModeLocation < 0) {
          indexBuffer.release();
          vertexBuffer.release();
          program.release();
          throw std::runtime_error("OpenGL raster backend shader attributes are unavailable");
        }

        program.enableAttributeArray(positionLocation);
        program.setAttributeBuffer(positionLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, x), 4,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(colorLocation);
        program.setAttributeBuffer(colorLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, r), 4,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(uvLocation);
        program.setAttributeBuffer(uvLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, u), 2,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(alphaScaleLocation);
        program.setAttributeBuffer(alphaScaleLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, alphaScale), 1,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(lightingLocation);
        program.setAttributeBuffer(lightingLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, lightR), 3,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(specularLocation);
        program.setAttributeBuffer(specularLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, specularR), 3,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(albedoModeLocation);
        program.setAttributeBuffer(albedoModeLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, albedoMode), 1,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));

        OpenGLTextureCache textureCache(functions);
        for (const auto& batch : mesh.batches()) {
          functions->glActiveTexture(GL_TEXTURE0);
          if (batch.albedo.mode == detail::RasterAlbedoShaderMode::ImageTexture &&
              batch.albedo.image) {
            functions->glBindTexture(GL_TEXTURE_2D, textureCache.textureFor(*batch.albedo.image));
          } else {
            functions->glBindTexture(GL_TEXTURE_2D, 0);
          }
          program.setUniformValue("imageUVScale", static_cast<GLfloat>(batch.albedo.uScale),
                                  static_cast<GLfloat>(batch.albedo.vScale));
          program.setUniformValue("albedoTint", static_cast<GLfloat>(batch.albedo.tint.r()),
                                  static_cast<GLfloat>(batch.albedo.tint.g()),
                                  static_cast<GLfloat>(batch.albedo.tint.b()));
          program.setUniformValue("checkerBright",
                                  static_cast<GLfloat>(batch.albedo.checkerBright.r()),
                                  static_cast<GLfloat>(batch.albedo.checkerBright.g()),
                                  static_cast<GLfloat>(batch.albedo.checkerBright.b()));
          program.setUniformValue("checkerDark", static_cast<GLfloat>(batch.albedo.checkerDark.r()),
                                  static_cast<GLfloat>(batch.albedo.checkerDark.g()),
                                  static_cast<GLfloat>(batch.albedo.checkerDark.b()));
          const auto byteOffset = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(batch.indexOffset * sizeof(std::uint32_t)));
          functions->glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.indexCount),
                                    GL_UNSIGNED_INT, byteOffset);
        }
        functions->glBindTexture(GL_TEXTURE_2D, 0);
        functions->glFlush();

        resetFixedFunctionState(functions);

        program.disableAttributeArray(albedoModeLocation);
        program.disableAttributeArray(specularLocation);
        program.disableAttributeArray(lightingLocation);
        program.disableAttributeArray(alphaScaleLocation);
        program.disableAttributeArray(uvLocation);
        program.disableAttributeArray(colorLocation);
        program.disableAttributeArray(positionLocation);
        indexBuffer.release();
        vertexBuffer.release();
        program.release();
      }

      int openGLY(const Recti& rect) const {
        return m_height - rect.bottom();
      }

      void applyScissor(QOpenGLFunctions* functions) const {
        if (!m_scissorEnabled) {
          functions->glDisable(GL_SCISSOR_TEST);
          return;
        }

        functions->glEnable(GL_SCISSOR_TEST);
        functions->glScissor(m_scissorRect.left(), openGLY(m_scissorRect), m_scissorRect.width(),
                             m_scissorRect.height());
      }

      void applyColorWriteMask(QOpenGLFunctions* functions) const {
        functions->glColorMask((m_colorWriteMask & Rasterizer::ColorWriteRed) != 0,
                               (m_colorWriteMask & Rasterizer::ColorWriteGreen) != 0,
                               (m_colorWriteMask & Rasterizer::ColorWriteBlue) != 0, GL_TRUE);
      }

      void applyBlending(QOpenGLFunctions* functions) const {
        if (!m_blendingEnabled) {
          functions->glDisable(GL_BLEND);
          return;
        }

        functions->glEnable(GL_BLEND);
        functions->glBlendColor(
          static_cast<GLclampf>(std::clamp(m_blendConstantColor.r(), 0.0, 1.0)),
          static_cast<GLclampf>(std::clamp(m_blendConstantColor.g(), 0.0, 1.0)),
          static_cast<GLclampf>(std::clamp(m_blendConstantColor.b(), 0.0, 1.0)),
          static_cast<GLclampf>(std::clamp(m_blendConstantAlpha, 0.0, 1.0)));
        functions->glBlendFunc(glBlendFactor(m_sourceBlendFactor),
                               glBlendFactor(m_destinationBlendFactor));
        functions->glBlendEquation(glBlendOp(m_blendOp));
      }

      void applyStencil(QOpenGLFunctions* functions) const {
        if (!m_stencilTestEnabled) {
          functions->glDisable(GL_STENCIL_TEST);
          functions->glStencilMask(0xff);
          return;
        }

        if (m_stencilLoadOp == Rasterizer::AttachmentLoadOp::Load) {
          throw std::runtime_error(
            "OpenGL raster backend does not support stencil attachment load yet");
        }

        functions->glEnable(GL_STENCIL_TEST);
        functions->glStencilFunc(glStencilFunc(m_stencilFunc), m_stencilReference, m_stencilMask);
        functions->glStencilMask(m_stencilWriteMask);
        functions->glStencilOp(glStencilOp(m_stencilFailOp), glStencilOp(m_stencilDepthFailOp),
                               glStencilOp(m_stencilPassOp));
      }

      void resetFixedFunctionState(QOpenGLFunctions* functions) const {
        functions->glDisable(GL_SCISSOR_TEST);
        functions->glDisable(GL_BLEND);
        functions->glDisable(GL_STENCIL_TEST);
        functions->glStencilMask(0xff);
        functions->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }

      static GLenum glBlendFactor(Rasterizer::BlendFactor factor) {
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

      static GLenum glBlendOp(Rasterizer::BlendOp op) {
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

      static GLenum glStencilFunc(Rasterizer::StencilFunc func) {
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

      static GLenum glStencilOp(Rasterizer::StencilOp op) {
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

      OpenGLOffscreenContext& m_context;
      int m_height;
      Recti m_viewportRect;
      bool m_scissorEnabled;
      Recti m_scissorRect;
      std::uint8_t m_colorWriteMask;
      bool m_blendingEnabled;
      Rasterizer::BlendFactor m_sourceBlendFactor;
      Rasterizer::BlendFactor m_destinationBlendFactor;
      Rasterizer::BlendOp m_blendOp;
      Colord m_blendConstantColor;
      double m_blendConstantAlpha;
      bool m_alphaTestEnabled;
      Rasterizer::AlphaFunc m_alphaFunc;
      double m_alphaReference;
      bool m_stencilTestEnabled;
      Rasterizer::StencilFunc m_stencilFunc;
      std::uint8_t m_stencilReference;
      std::uint8_t m_stencilMask;
      std::uint8_t m_stencilClearValue;
      Rasterizer::AttachmentLoadOp m_stencilLoadOp;
      Rasterizer::AttachmentStoreOp m_stencilStoreOp;
      std::uint8_t m_stencilWriteMask;
      Rasterizer::StencilOp m_stencilFailOp;
      Rasterizer::StencilOp m_stencilDepthFailOp;
      Rasterizer::StencilOp m_stencilPassOp;
    };
  }

  OpenGLRasterizer::OpenGLRasterizer(std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(scene)) {
  }

  OpenGLRasterizer::OpenGLRasterizer(std::shared_ptr<render::Camera> camera,
                                     std::shared_ptr<render::Scene> scene)
      : RenderEngine(std::move(camera), std::move(scene)) {
  }

  OpenGLRasterizer::~OpenGLRasterizer() = default;

  std::shared_ptr<render::RenderEngine> OpenGLRasterizer::cloneForRender() const {
    auto clone = std::make_shared<OpenGLRasterizer>(camera(), scene());
    if (hasBackgroundColorOverride()) {
      clone->setBackgroundColor(backgroundColor());
    }
    clone->setTonemap(tonemap());
    clone->setLod(m_lod);
    clone->setMSAASamples(m_msaaSamples);
    if (m_hasCullModeOverride) {
      clone->setCullMode(m_cullMode);
    } else {
      clone->clearCullModeOverride();
    }
    if (m_viewportEnabled) {
      clone->setViewportRect(m_viewportRect);
    } else {
      clone->clearViewportRect();
    }
    if (m_scissorTestEnabled) {
      clone->setScissorRect(m_scissorRect);
    } else {
      clone->clearScissorRect();
    }
    clone->setColorWriteMask(m_colorWriteMask);
    clone->setBlendingEnabled(m_blendingEnabled);
    clone->setBlendFactors(m_sourceBlendFactor, m_destinationBlendFactor);
    clone->setBlendOp(m_blendOp);
    clone->setBlendConstant(m_blendConstantColor, m_blendConstantAlpha);
    clone->setAlphaTestEnabled(m_alphaTestEnabled);
    clone->setAlphaFunc(m_alphaFunc, m_alphaReference);
    clone->setStencilTestEnabled(m_stencilTestEnabled);
    clone->setStencilFunc(m_stencilFunc, m_stencilReference, m_stencilMask);
    clone->setStencilClearValue(m_stencilClearValue);
    clone->setStencilLoadOp(m_stencilLoadOp);
    clone->setStencilStoreOp(m_stencilStoreOp);
    clone->setStencilWriteMask(m_stencilWriteMask);
    clone->setStencilOps(m_stencilFailOp, m_stencilDepthFailOp, m_stencilPassOp);
    clone->setShadowMapsEnabled(m_shadowMapsEnabled);
    clone->setExternalShadowMaps(m_externalShadowMaps);
    if (m_cancelled.load()) {
      clone->cancel();
    }
    return clone;
  }

  void OpenGLRasterizer::render(Buffer<unsigned int>& buffer) {
    RenderEngine::render(buffer);
  }

  void OpenGLRasterizer::render(Buffer<Colord>& buffer) {
    renderOpenGL(buffer, nullptr, nullptr);
  }

  void OpenGLRasterizer::renderDepth(Buffer<double>& buffer) {
    Buffer<Colord> scratch(buffer.width(), buffer.height());
    renderOpenGL(scratch, &buffer, nullptr);
  }

  void OpenGLRasterizer::renderStencil(Buffer<std::uint8_t>& buffer) {
    Buffer<Colord> scratch(buffer.width(), buffer.height());
    renderOpenGL(scratch, nullptr, &buffer);
  }

  void OpenGLRasterizer::cancel() {
    m_cancelled.store(true);
  }

  void OpenGLRasterizer::uncancel() {
    m_cancelled.store(false);
  }

  std::string OpenGLRasterizer::statusMessage() {
    return "OpenGL raster backend renders the initial lit mesh path when Qt can create an "
           "offscreen context; unsupported hosts report an OpenGL capability error when the "
           "backend is selected";
  }

  const std::string& OpenGLRasterizer::readbackTraceMessage() const {
    return m_lastReadbackTraceMessage;
  }

  int OpenGLRasterizer::lod() const {
    return m_lod;
  }

  void OpenGLRasterizer::setLod(int lod) {
    m_lod = lod;
  }

  int OpenGLRasterizer::msaaSamples() const {
    return m_msaaSamples;
  }

  void OpenGLRasterizer::setMSAASamples(int samples) {
    if (samples <= 1) {
      m_msaaSamples = 1;
    } else if (samples <= 2) {
      m_msaaSamples = 2;
    } else if (samples <= 4) {
      m_msaaSamples = 4;
    } else {
      m_msaaSamples = 8;
    }
  }

  Rasterizer::CullMode OpenGLRasterizer::cullMode() const {
    return m_cullMode;
  }

  bool OpenGLRasterizer::hasCullModeOverride() const {
    return m_hasCullModeOverride;
  }

  void OpenGLRasterizer::setCullMode(Rasterizer::CullMode mode) {
    m_cullMode = mode;
    m_hasCullModeOverride = true;
  }

  void OpenGLRasterizer::clearCullModeOverride() {
    m_hasCullModeOverride = false;
  }

  bool OpenGLRasterizer::viewportEnabled() const {
    return m_viewportEnabled;
  }

  const Recti& OpenGLRasterizer::viewportRect() const {
    return m_viewportRect;
  }

  void OpenGLRasterizer::setViewportRect(const Recti& rect) {
    m_viewportRect =
      Recti(rect.left(), rect.top(), std::max(0, rect.width()), std::max(0, rect.height()));
    m_viewportEnabled = true;
  }

  void OpenGLRasterizer::clearViewportRect() {
    m_viewportRect = Recti();
    m_viewportEnabled = false;
  }

  bool OpenGLRasterizer::scissorTestEnabled() const {
    return m_scissorTestEnabled;
  }

  const Recti& OpenGLRasterizer::scissorRect() const {
    return m_scissorRect;
  }

  void OpenGLRasterizer::setScissorRect(const Recti& rect) {
    m_scissorRect =
      Recti(rect.left(), rect.top(), std::max(0, rect.width()), std::max(0, rect.height()));
    m_scissorTestEnabled = true;
  }

  void OpenGLRasterizer::clearScissorRect() {
    m_scissorRect = Recti();
    m_scissorTestEnabled = false;
  }

  std::uint8_t OpenGLRasterizer::colorWriteMask() const {
    return m_colorWriteMask;
  }

  void OpenGLRasterizer::setColorWriteMask(std::uint8_t mask) {
    m_colorWriteMask = mask & Rasterizer::ColorWriteAll;
  }

  bool OpenGLRasterizer::blendingEnabled() const {
    return m_blendingEnabled;
  }

  void OpenGLRasterizer::setBlendingEnabled(bool enabled) {
    m_blendingEnabled = enabled;
  }

  Rasterizer::BlendFactor OpenGLRasterizer::sourceBlendFactor() const {
    return m_sourceBlendFactor;
  }

  Rasterizer::BlendFactor OpenGLRasterizer::destinationBlendFactor() const {
    return m_destinationBlendFactor;
  }

  void OpenGLRasterizer::setBlendFactors(Rasterizer::BlendFactor source,
                                         Rasterizer::BlendFactor destination) {
    m_sourceBlendFactor = source;
    m_destinationBlendFactor = destination;
  }

  Rasterizer::BlendOp OpenGLRasterizer::blendOp() const {
    return m_blendOp;
  }

  void OpenGLRasterizer::setBlendOp(Rasterizer::BlendOp op) {
    m_blendOp = op;
  }

  Colord OpenGLRasterizer::blendConstantColor() const {
    return m_blendConstantColor;
  }

  double OpenGLRasterizer::blendConstantAlpha() const {
    return m_blendConstantAlpha;
  }

  void OpenGLRasterizer::setBlendConstant(const Colord& color, double alpha) {
    m_blendConstantColor = color;
    m_blendConstantAlpha = std::isfinite(alpha) ? std::clamp(alpha, 0.0, 1.0) : 1.0;
  }

  bool OpenGLRasterizer::alphaTestEnabled() const {
    return m_alphaTestEnabled;
  }

  void OpenGLRasterizer::setAlphaTestEnabled(bool enabled) {
    m_alphaTestEnabled = enabled;
  }

  Rasterizer::AlphaFunc OpenGLRasterizer::alphaFunc() const {
    return m_alphaFunc;
  }

  double OpenGLRasterizer::alphaReference() const {
    return m_alphaReference;
  }

  void OpenGLRasterizer::setAlphaFunc(Rasterizer::AlphaFunc func, double reference) {
    m_alphaFunc = func;
    m_alphaReference = std::isfinite(reference) ? std::clamp(reference, 0.0, 1.0) : 0.0;
  }

  bool OpenGLRasterizer::stencilTestEnabled() const {
    return m_stencilTestEnabled;
  }

  void OpenGLRasterizer::setStencilTestEnabled(bool enabled) {
    m_stencilTestEnabled = enabled;
  }

  Rasterizer::StencilFunc OpenGLRasterizer::stencilFunc() const {
    return m_stencilFunc;
  }

  std::uint8_t OpenGLRasterizer::stencilReference() const {
    return m_stencilReference;
  }

  std::uint8_t OpenGLRasterizer::stencilMask() const {
    return m_stencilMask;
  }

  void OpenGLRasterizer::setStencilFunc(Rasterizer::StencilFunc func, std::uint8_t reference,
                                        std::uint8_t mask) {
    m_stencilFunc = func;
    m_stencilReference = reference;
    m_stencilMask = mask;
  }

  std::uint8_t OpenGLRasterizer::stencilClearValue() const {
    return m_stencilClearValue;
  }

  void OpenGLRasterizer::setStencilClearValue(std::uint8_t value) {
    m_stencilClearValue = value;
  }

  Rasterizer::AttachmentLoadOp OpenGLRasterizer::stencilLoadOp() const {
    return m_stencilLoadOp;
  }

  void OpenGLRasterizer::setStencilLoadOp(Rasterizer::AttachmentLoadOp op) {
    m_stencilLoadOp = op;
  }

  Rasterizer::AttachmentStoreOp OpenGLRasterizer::stencilStoreOp() const {
    return m_stencilStoreOp;
  }

  void OpenGLRasterizer::setStencilStoreOp(Rasterizer::AttachmentStoreOp op) {
    m_stencilStoreOp = op;
  }

  std::uint8_t OpenGLRasterizer::stencilWriteMask() const {
    return m_stencilWriteMask;
  }

  void OpenGLRasterizer::setStencilWriteMask(std::uint8_t mask) {
    m_stencilWriteMask = mask;
  }

  Rasterizer::StencilOp OpenGLRasterizer::stencilFailOp() const {
    return m_stencilFailOp;
  }

  Rasterizer::StencilOp OpenGLRasterizer::stencilDepthFailOp() const {
    return m_stencilDepthFailOp;
  }

  Rasterizer::StencilOp OpenGLRasterizer::stencilPassOp() const {
    return m_stencilPassOp;
  }

  void OpenGLRasterizer::setStencilOps(Rasterizer::StencilOp stencilFail,
                                       Rasterizer::StencilOp depthFail,
                                       Rasterizer::StencilOp pass) {
    m_stencilFailOp = stencilFail;
    m_stencilDepthFailOp = depthFail;
    m_stencilPassOp = pass;
  }

  bool OpenGLRasterizer::shadowMapsEnabled() const {
    return m_shadowMapsEnabled;
  }

  void OpenGLRasterizer::setShadowMapsEnabled(bool enabled) {
    m_shadowMapsEnabled = enabled;
  }

  void
  OpenGLRasterizer::setExternalShadowMaps(std::shared_ptr<const detail::ShadowMaps> shadowMaps) {
    m_externalShadowMaps = std::move(shadowMaps);
  }

  void OpenGLRasterizer::clearExternalShadowMaps() {
    setExternalShadowMaps(nullptr);
  }

  bool OpenGLRasterizer::isAvailable() const {
    return OpenGLOffscreenContext::probe().available();
  }

  std::string OpenGLRasterizer::availabilityDetail() const {
    return OpenGLOffscreenContext::probe().detail();
  }

  std::string OpenGLRasterizer::availabilityError() const {
    const OpenGLAvailability availability = OpenGLOffscreenContext::probe();
    if (!availability.available()) {
      return availability.error();
    }
    return "OpenGL raster backend is selected and an offscreen context is available (" +
           availability.detail() + ") and can render the initial lit mesh path";
  }

  Recti OpenGLRasterizer::viewportRectFor(int width, int height) const {
    if (m_viewportEnabled) {
      return m_viewportRect;
    }
    return Recti(width, height);
  }

  std::string OpenGLRasterizer::readbackTraceMessage(std::chrono::nanoseconds elapsed,
                                                     bool copiedDepth, bool copiedStencil) const {
    std::vector<std::string> attachments{"color"};
    if (copiedDepth) {
      attachments.push_back("depth");
    }
    if (copiedStencil) {
      attachments.push_back("stencil");
    }

    std::ostringstream message;
    message << "OpenGL raster readback copied ";
    for (std::size_t i = 0; i != attachments.size(); ++i) {
      if (i != 0) {
        message << (i + 1 == attachments.size() ? " and " : ", ");
      }
      message << attachments[i];
    }
    message << " attachment";
    if (attachments.size() != 1) {
      message << "s";
    }
    message << " to CPU buffers in " << std::fixed << std::setprecision(3)
            << elapsed.count() / 1000000.0 << " ms";
    return message.str();
  }

  void OpenGLRasterizer::renderOpenGL(Buffer<Colord>& buffer, Buffer<double>* depthTarget,
                                      Buffer<std::uint8_t>* stencilTarget) const {
    OpenGLOffscreenContext context;
    if (!context.create(buffer.width(), buffer.height(), m_msaaSamples)) {
      throw std::runtime_error(context.errorMessage());
    }

    const Recti viewport = viewportRectFor(buffer.width(), buffer.height());
    detail::OpenGLRasterMesh mesh;
    if (viewport.width() > 0 && viewport.height() > 0) {
      mesh = detail::OpenGLRasterMeshBuilder(
               scene().get(), camera(), m_lod, viewport, m_cullMode, m_hasCullModeOverride,
               m_cancelled, m_shadowMapsEnabled ? m_externalShadowMaps.get() : nullptr)
               .build();
    }

    const auto readbackElapsed =
      OpenGLRasterDrawPass(
        context, buffer.height(), viewport, m_scissorTestEnabled, m_scissorRect, m_colorWriteMask,
        m_blendingEnabled, m_sourceBlendFactor, m_destinationBlendFactor, m_blendOp,
        m_blendConstantColor, m_blendConstantAlpha, m_alphaTestEnabled, m_alphaFunc,
        m_alphaReference, m_stencilTestEnabled, m_stencilFunc, m_stencilReference, m_stencilMask,
        m_stencilClearValue, m_stencilLoadOp, m_stencilStoreOp, m_stencilWriteMask, m_stencilFailOp,
        m_stencilDepthFailOp, m_stencilPassOp)
        .render(mesh, backgroundColor(), buffer, depthTarget, stencilTarget);
    m_lastReadbackTraceMessage = readbackTraceMessage(
      readbackElapsed, depthTarget != nullptr,
      stencilTarget != nullptr && m_stencilStoreOp == Rasterizer::AttachmentStoreOp::Store);
  }
}
