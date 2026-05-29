#include "engine/raster/OpenGLRasterizer.h"

#include "core/Buffer.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/detail/OpenGLRasterMesh.h"
#include "engine/raster/detail/OpenGLRasterResourceCache.h"
#include "engine/raster/detail/OpenGLRasterShaderSources.h"
#include "engine/raster/detail/OpenGLShadowSamplingPlan.h"
#include "engine/raster/detail/OpenGLShadowTextureData.h"
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
#include <limits>
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
          pixels.push_back(static_cast<GLfloat>(color.r()));
          pixels.push_back(static_cast<GLfloat>(color.g()));
          pixels.push_back(static_cast<GLfloat>(color.b()));
          pixels.push_back(1.0f);
        }
        return pixels;
      }

      QOpenGLFunctions* m_functions;
      std::unordered_map<const render::ImageTexture*, GLuint> m_textures;
    };

    class OpenGLFallbackTexture {
    public:
      explicit OpenGLFallbackTexture(QOpenGLFunctions* functions)
          : m_functions(functions) {
        static constexpr GLfloat pixels[] = {1.0f, 1.0f, 1.0f, 1.0f};

        m_functions->glGenTextures(1, &m_texture);
        if (m_texture == 0) {
          throw std::runtime_error("OpenGL raster backend could not allocate a fallback texture");
        }

        m_functions->glBindTexture(GL_TEXTURE_2D, m_texture);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        m_functions->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, pixels);
        m_functions->glBindTexture(GL_TEXTURE_2D, 0);
      }

      ~OpenGLFallbackTexture() {
        if (m_texture != 0) {
          m_functions->glDeleteTextures(1, &m_texture);
        }
      }

      void bind(int textureUnit) const {
        m_functions->glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
        m_functions->glBindTexture(GL_TEXTURE_2D, m_texture);
      }

      void release(int textureUnit) const {
        m_functions->glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
        m_functions->glBindTexture(GL_TEXTURE_2D, 0);
      }

    private:
      QOpenGLFunctions* m_functions;
      GLuint m_texture{0};
    };

    class OpenGLShadowTexture {
    public:
      OpenGLShadowTexture(QOpenGLFunctions* functions, const detail::OpenGLShadowTextureData& data)
          : m_functions(functions) {
        if (!data.enabled()) {
          return;
        }

        m_functions->glGenTextures(1, &m_texture);
        if (m_texture == 0) {
          throw std::runtime_error("OpenGL raster backend could not allocate a shadow texture");
        }

        m_functions->glBindTexture(GL_TEXTURE_2D, m_texture);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        m_functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        m_functions->glTexImage2D(GL_TEXTURE_2D, 0, internalFormat(), data.width(), data.height(),
                                  0, GL_RGBA, GL_FLOAT, data.rgbaPixels().data());
        m_functions->glBindTexture(GL_TEXTURE_2D, 0);
      }

      ~OpenGLShadowTexture() {
        if (m_texture != 0) {
          m_functions->glDeleteTextures(1, &m_texture);
        }
      }

      bool enabled() const {
        return m_texture != 0;
      }

      void bind(int textureUnit) const {
        m_functions->glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
        m_functions->glBindTexture(GL_TEXTURE_2D, m_texture);
      }

      void release(int textureUnit) const {
        m_functions->glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
        m_functions->glBindTexture(GL_TEXTURE_2D, 0);
      }

    private:
      static GLint internalFormat() {
#if defined(GL_RGBA32F)
        return GL_RGBA32F;
#else
        return GL_RGBA;
#endif
      }

      QOpenGLFunctions* m_functions;
      GLuint m_texture{0};
    };

    struct OpenGLRasterRenderTimings {
      std::chrono::nanoseconds drawElapsed{0};
      std::chrono::nanoseconds readbackElapsed{0};
    };

    class OpenGLRasterDrawPass {
    public:
      OpenGLRasterDrawPass(
        detail::OpenGLRasterResourceCache& resources, int height, const Recti& viewportRect,
        bool scissorEnabled, const Recti& scissorRect, Rasterizer::AttachmentLoadOp colorLoadOp,
        Rasterizer::AttachmentStoreOp colorStoreOp, std::uint8_t colorWriteMask,
        bool blendingEnabled, Rasterizer::BlendFactor sourceBlendFactor,
        Rasterizer::BlendFactor destinationBlendFactor, Rasterizer::BlendOp blendOp,
        const Colord& blendConstantColor, double blendConstantAlpha, bool alphaTestEnabled,
        Rasterizer::AlphaFunc alphaFunc, double alphaReference, Rasterizer::DepthFunc depthFunc,
        double depthClearValue, Rasterizer::AttachmentLoadOp depthLoadOp,
        Rasterizer::AttachmentStoreOp depthStoreOp, bool depthWriteEnabled, bool stencilTestEnabled,
        Rasterizer::StencilFunc stencilFunc, std::uint8_t stencilReference,
        std::uint8_t stencilMask, std::uint8_t stencilClearValue,
        Rasterizer::AttachmentLoadOp stencilLoadOp, Rasterizer::AttachmentStoreOp stencilStoreOp,
        std::uint8_t stencilWriteMask, Rasterizer::StencilOp stencilFailOp,
        Rasterizer::StencilOp stencilDepthFailOp, Rasterizer::StencilOp stencilPassOp,
        detail::OpenGLShadowTextureData shadowTextureData, const Vector3d& cameraPosition,
        Rasterizer::CullMode cullMode, bool hasCullModeOverride, const std::atomic<bool>& cancelled)
          : m_resources(resources),
            m_height(height),
            m_viewportRect(viewportRect),
            m_scissorEnabled(scissorEnabled),
            m_scissorRect(scissorRect),
            m_colorLoadOp(colorLoadOp),
            m_colorStoreOp(colorStoreOp),
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
            m_depthFunc(depthFunc),
            m_depthClearValue(depthClearValue),
            m_depthLoadOp(depthLoadOp),
            m_depthStoreOp(depthStoreOp),
            m_depthWriteEnabled(depthWriteEnabled),
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
            m_stencilPassOp(stencilPassOp),
            m_shadowTextureData(std::move(shadowTextureData)),
            m_cameraPosition(cameraPosition),
            m_cullMode(cullMode),
            m_hasCullModeOverride(hasCullModeOverride),
            m_cancelled(cancelled) {
      }

      OpenGLRasterRenderTimings render(const detail::OpenGLRasterMesh& mesh,
                                       const Colord& background, Buffer<Colord>* target,
                                       Buffer<double>* depthTarget,
                                       Buffer<std::uint8_t>* stencilTarget) {
        if (!m_resources.context.makeCurrent()) {
          throw std::runtime_error(m_resources.context.errorMessage());
        }
        if (!m_resources.context.bindFramebuffer()) {
          m_resources.context.doneCurrent();
          throw std::runtime_error(m_resources.context.errorMessage());
        }

        try {
          const auto drawStarted = std::chrono::steady_clock::now();
          draw(mesh, background);
          const auto drawElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - drawStarted);
          const auto readbackStarted = std::chrono::steady_clock::now();
          if (target && m_colorStoreOp == Rasterizer::AttachmentStoreOp::Store) {
            m_resources.context.copyColorTo(*target);
          }
          if (depthTarget && m_depthStoreOp == Rasterizer::AttachmentStoreOp::Store) {
            m_resources.context.copyDepthTo(*depthTarget);
          }
          if (stencilTarget && m_stencilStoreOp == Rasterizer::AttachmentStoreOp::Store) {
            m_resources.context.copyStencilTo(*stencilTarget);
          }
          const auto readbackElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - readbackStarted);
          m_resources.context.releaseFramebuffer();
          m_resources.context.doneCurrent();
          return {drawElapsed, readbackElapsed};
        } catch (...) {
          m_resources.context.releaseFramebuffer();
          m_resources.context.doneCurrent();
          throw;
        }
      }

    private:
      void draw(const detail::OpenGLRasterMesh& mesh, const Colord& background) {
        if (m_colorLoadOp == Rasterizer::AttachmentLoadOp::Load) {
          throw std::runtime_error(
            "OpenGL raster backend does not support color attachment load yet");
        }

        QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
        functions->glViewport(m_viewportRect.left(), openGLY(m_viewportRect),
                              m_viewportRect.width(), m_viewportRect.height());
        functions->glDisable(GL_SCISSOR_TEST);
        functions->glEnable(GL_DEPTH_TEST);
        functions->glDepthMask(GL_TRUE);
        functions->glClearColor(static_cast<GLfloat>(std::clamp(background.r(), 0.0, 1.0)),
                                static_cast<GLfloat>(std::clamp(background.g(), 0.0, 1.0)),
                                static_cast<GLfloat>(std::clamp(background.b(), 0.0, 1.0)), 1.0f);
        functions->glClearDepthf(normalizedDepthClearValue(m_depthClearValue));
        functions->glStencilMask(0xff);
        functions->glClearStencil(m_stencilClearValue);
        functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        applyDepth(functions);
        applyColorWriteMask(functions);
        applyBlending(functions);
        applyStencil(functions);
        applyCullMode(functions);
        OpenGLShadowTexture shadowTexture(functions, m_shadowTextureData);

        if (mesh.empty()) {
          functions->glFlush();
          resetFixedFunctionState(functions);
          return;
        }

        applyScissor(functions);

        OpenGLFallbackTexture fallbackTexture(functions);
        m_resources.ensureProgram();
        QOpenGLShaderProgram& program = *m_resources.program;
        if (!program.bind()) {
          throw std::runtime_error("OpenGL raster backend could not bind shader program");
        }
        program.setUniformValue("alphaTestEnabled", m_alphaTestEnabled);
        program.setUniformValue("alphaFunc", static_cast<int>(m_alphaFunc));
        program.setUniformValue("alphaReference",
                                static_cast<GLfloat>(std::clamp(m_alphaReference, 0.0, 1.0)));
        program.setUniformValue("imageTexture", 0);
        setShadowUniforms(program, shadowTexture.enabled());
        setLightingUniforms(program, mesh);
        fallbackTexture.bind(0);
        if (shadowTexture.enabled()) {
          shadowTexture.bind(1);
        } else {
          fallbackTexture.bind(1);
        }

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

        const int positionLocation = m_resources.locations.position;
        const int worldPositionLocation = m_resources.locations.worldPosition;
        const int normalLocation = m_resources.locations.normal;
        const int colorLocation = m_resources.locations.color;
        const int uvLocation = m_resources.locations.uv;
        const int alphaScaleLocation = m_resources.locations.alphaScale;
        const int materialDiffuseLocation = m_resources.locations.materialDiffuse;
        const int materialSpecularColorLocation = m_resources.locations.materialSpecularColor;
        const int materialSpecularCoefficientLocation =
          m_resources.locations.materialSpecularCoefficient;
        const int materialSpecularExponentLocation = m_resources.locations.materialSpecularExponent;
        const int ambientLightingLocation = m_resources.locations.ambientLighting;
        const int directLightingLocation = m_resources.locations.directLighting;
        const int specularLocation = m_resources.locations.specular;
        const int albedoModeLocation = m_resources.locations.albedoMode;

        program.enableAttributeArray(positionLocation);
        program.setAttributeBuffer(positionLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, x), 4,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(worldPositionLocation);
        program.setAttributeBuffer(worldPositionLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, worldX), 3,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(normalLocation);
        program.setAttributeBuffer(normalLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, normalX), 3,
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
        program.enableAttributeArray(materialDiffuseLocation);
        program.setAttributeBuffer(materialDiffuseLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, materialDiffuse), 1,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(materialSpecularColorLocation);
        program.setAttributeBuffer(materialSpecularColorLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, materialSpecularR), 3,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(materialSpecularCoefficientLocation);
        program.setAttributeBuffer(
          materialSpecularCoefficientLocation, GL_FLOAT,
          offsetof(detail::OpenGLRasterMesh::Vertex, materialSpecularCoefficient), 1,
          sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(materialSpecularExponentLocation);
        program.setAttributeBuffer(
          materialSpecularExponentLocation, GL_FLOAT,
          offsetof(detail::OpenGLRasterMesh::Vertex, materialSpecularExponent), 1,
          sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(ambientLightingLocation);
        program.setAttributeBuffer(ambientLightingLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, ambientR), 3,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(directLightingLocation);
        program.setAttributeBuffer(directLightingLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, directR), 3,
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
          if (m_cancelled.load()) {
            break;
          }
          functions->glActiveTexture(GL_TEXTURE0);
          if (batch.albedo.mode == detail::RasterAlbedoShaderMode::ImageTexture &&
              batch.albedo.image) {
            functions->glBindTexture(GL_TEXTURE_2D, textureCache.textureFor(*batch.albedo.image));
          } else {
            fallbackTexture.bind(0);
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
        if (shadowTexture.enabled()) {
          shadowTexture.release(1);
        } else {
          fallbackTexture.release(1);
        }
        fallbackTexture.release(0);
        functions->glActiveTexture(GL_TEXTURE0);
        functions->glFlush();

        resetFixedFunctionState(functions);

        program.disableAttributeArray(albedoModeLocation);
        program.disableAttributeArray(specularLocation);
        program.disableAttributeArray(directLightingLocation);
        program.disableAttributeArray(ambientLightingLocation);
        program.disableAttributeArray(materialSpecularExponentLocation);
        program.disableAttributeArray(materialSpecularCoefficientLocation);
        program.disableAttributeArray(materialSpecularColorLocation);
        program.disableAttributeArray(materialDiffuseLocation);
        program.disableAttributeArray(alphaScaleLocation);
        program.disableAttributeArray(uvLocation);
        program.disableAttributeArray(colorLocation);
        program.disableAttributeArray(normalLocation);
        program.disableAttributeArray(worldPositionLocation);
        program.disableAttributeArray(positionLocation);
        indexBuffer.release();
        vertexBuffer.release();
        program.release();
      }

      int openGLY(const Recti& rect) const {
        return m_height - rect.bottom();
      }

      void applyDepth(QOpenGLFunctions* functions) const {
        if (m_depthLoadOp == Rasterizer::AttachmentLoadOp::Load) {
          throw std::runtime_error(
            "OpenGL raster backend does not support depth attachment load yet");
        }

        functions->glDepthFunc(glDepthFunc(m_depthFunc));
        functions->glDepthMask(m_depthWriteEnabled ? GL_TRUE : GL_FALSE);
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

      void applyCullMode(QOpenGLFunctions* functions) const {
        if (!m_hasCullModeOverride || m_cullMode == Rasterizer::CullMode::Both) {
          functions->glDisable(GL_CULL_FACE);
          return;
        }
        functions->glEnable(GL_CULL_FACE);
        functions->glFrontFace(GL_CCW);
        functions->glCullFace(m_cullMode == Rasterizer::CullMode::Back ? GL_BACK : GL_FRONT);
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
        functions->glDisable(GL_CULL_FACE);
        functions->glStencilMask(0xff);
        functions->glDepthMask(GL_TRUE);
        functions->glDepthFunc(GL_LESS);
        functions->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }

      void setShadowUniforms(QOpenGLShaderProgram& program, bool enabled) const {
        program.setUniformValue("shadowTextureEnabled", enabled);
        program.setUniformValue("shadowTexture", 1);
        if (!enabled) {
          return;
        }

        setVectorUniform(program, "shadowOrigin", m_shadowTextureData.origin());
        setVectorUniform(program, "shadowRight", m_shadowTextureData.right());
        setVectorUniform(program, "shadowUp", m_shadowTextureData.up());
        setVectorUniform(program, "shadowForward", m_shadowTextureData.forward());
        program.setUniformValue("shadowHalfExtent",
                                static_cast<GLfloat>(m_shadowTextureData.halfExtent()));
        program.setUniformValue("shadowDepthScale",
                                static_cast<GLfloat>(m_shadowTextureData.depthScale()));
        program.setUniformValue("shadowBias", static_cast<GLfloat>(m_shadowTextureData.bias()));
        program.setUniformValue("shadowFilterRadius", m_shadowTextureData.filterRadius());
        program.setUniformValue("shadowTexelSize",
                                static_cast<GLfloat>(1.0 / m_shadowTextureData.width()),
                                static_cast<GLfloat>(1.0 / m_shadowTextureData.height()));
      }

      void setLightingUniforms(QOpenGLShaderProgram& program,
                               const detail::OpenGLRasterMesh& mesh) const {
        setVectorUniform(program, "cameraPosition", m_cameraPosition);

        const auto& directionalLights = mesh.directionalLights();
        const int directionalLightCount = static_cast<int>(std::min<std::size_t>(
          directionalLights.size(), OpenGLRasterizer::maxShaderDirectionalLights()));
        program.setUniformValue("directionalLightCount", directionalLightCount);
        for (int i = 0; i < directionalLightCount; ++i) {
          const auto& light = directionalLights[static_cast<std::size_t>(i)];
          setVectorUniform(program, uniformName("directionalLightDirection", i).c_str(),
                           Vector3d(light.directionX, light.directionY, light.directionZ));
          program.setUniformValue(uniformName("directionalLightRadiance", i).c_str(),
                                  static_cast<GLfloat>(light.radianceR),
                                  static_cast<GLfloat>(light.radianceG),
                                  static_cast<GLfloat>(light.radianceB));
        }

        const auto& pointLights = mesh.pointLights();
        const int pointLightCount = static_cast<int>(
          std::min<std::size_t>(pointLights.size(), OpenGLRasterizer::maxShaderPointLights()));
        program.setUniformValue("pointLightCount", pointLightCount);
        for (int i = 0; i < pointLightCount; ++i) {
          const auto& light = pointLights[static_cast<std::size_t>(i)];
          setVectorUniform(program, uniformName("pointLightPosition", i).c_str(),
                           Vector3d(light.positionX, light.positionY, light.positionZ));
          program.setUniformValue(
            uniformName("pointLightRadiance", i).c_str(), static_cast<GLfloat>(light.radianceR),
            static_cast<GLfloat>(light.radianceG), static_cast<GLfloat>(light.radianceB));
        }
      }

      void setVectorUniform(QOpenGLShaderProgram& program, const char* name,
                            const Vector3d& value) const {
        program.setUniformValue(name, static_cast<GLfloat>(value.x()),
                                static_cast<GLfloat>(value.y()), static_cast<GLfloat>(value.z()));
      }

      std::string uniformName(const char* arrayName, int index) const {
        std::ostringstream name;
        name << arrayName << "[" << index << "]";
        return name.str();
      }

      static GLfloat normalizedDepthClearValue(double depth) {
        if (!std::isfinite(depth)) {
          return depth < 0.0 ? 0.0f : 1.0f;
        }
        const double positiveDepth = std::max(0.0, depth);
        return static_cast<GLfloat>(std::clamp(positiveDepth / (positiveDepth + 1.0), 0.0, 1.0));
      }

      static GLenum glDepthFunc(Rasterizer::DepthFunc func) {
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

      detail::OpenGLRasterResourceCache& m_resources;
      int m_height;
      Recti m_viewportRect;
      bool m_scissorEnabled;
      Recti m_scissorRect;
      Rasterizer::AttachmentLoadOp m_colorLoadOp;
      Rasterizer::AttachmentStoreOp m_colorStoreOp;
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
      Rasterizer::DepthFunc m_depthFunc;
      double m_depthClearValue;
      Rasterizer::AttachmentLoadOp m_depthLoadOp;
      Rasterizer::AttachmentStoreOp m_depthStoreOp;
      bool m_depthWriteEnabled;
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
      detail::OpenGLShadowTextureData m_shadowTextureData;
      Vector3d m_cameraPosition;
      Rasterizer::CullMode m_cullMode;
      bool m_hasCullModeOverride;
      const std::atomic<bool>& m_cancelled;
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
    clone->setMSAAShadingMode(m_msaaShadingMode);
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
    clone->setColorLoadOp(m_colorLoadOp);
    clone->setColorStoreOp(m_colorStoreOp);
    clone->setColorWriteMask(m_colorWriteMask);
    clone->setBlendingEnabled(m_blendingEnabled);
    clone->setBlendFactors(m_sourceBlendFactor, m_destinationBlendFactor);
    clone->setBlendOp(m_blendOp);
    clone->setBlendConstant(m_blendConstantColor, m_blendConstantAlpha);
    clone->setAlphaTestEnabled(m_alphaTestEnabled);
    clone->setAlphaFunc(m_alphaFunc, m_alphaReference);
    clone->setDepthFunc(m_depthFunc);
    clone->setDepthBias(m_depthBias);
    clone->setDepthClearValue(m_depthClearValue);
    clone->setDepthLoadOp(m_depthLoadOp);
    clone->setDepthStoreOp(m_depthStoreOp);
    clone->setDepthWriteEnabled(m_depthWriteEnabled);
    clone->setStencilTestEnabled(m_stencilTestEnabled);
    clone->setStencilFunc(m_stencilFunc, m_stencilReference, m_stencilMask);
    clone->setStencilClearValue(m_stencilClearValue);
    clone->setStencilLoadOp(m_stencilLoadOp);
    clone->setStencilStoreOp(m_stencilStoreOp);
    clone->setStencilWriteMask(m_stencilWriteMask);
    clone->setStencilOps(m_stencilFailOp, m_stencilDepthFailOp, m_stencilPassOp);
    clone->setShadowMapsEnabled(m_shadowMapsEnabled);
    clone->setExternalShadowMaps(m_externalShadowMaps);
    clone->setVisibilitySet(m_visibilitySet);
    if (m_cancelled.load()) {
      clone->cancel();
    }
    return clone;
  }

  void OpenGLRasterizer::render(Buffer<unsigned int>& buffer) {
    RenderEngine::render(buffer);
  }

  void OpenGLRasterizer::render(Buffer<Colord>& buffer) {
    renderOpenGL(buffer.width(), buffer.height(), &buffer, nullptr, nullptr);
  }

  void OpenGLRasterizer::renderDepth(Buffer<double>& buffer) {
    renderOpenGL(buffer.width(), buffer.height(), nullptr, &buffer, nullptr);
  }

  void OpenGLRasterizer::renderStencil(Buffer<std::uint8_t>& buffer) {
    renderOpenGL(buffer.width(), buffer.height(), nullptr, nullptr, &buffer);
  }

  void OpenGLRasterizer::cancel() {
    m_cancelled.store(true);
  }

  void OpenGLRasterizer::uncancel() {
    m_cancelled.store(false);
  }

  void OpenGLRasterizer::appendLightTruncationTrace(std::size_t directionalLightCount,
                                                    std::size_t pointLightCount,
                                                    std::vector<std::string>& traces) {
    const auto maxDirectional = static_cast<std::size_t>(maxShaderDirectionalLights());
    const auto maxPoint = static_cast<std::size_t>(maxShaderPointLights());
    if (directionalLightCount > maxDirectional) {
      std::ostringstream message;
      message << "OpenGL raster shader truncated " << directionalLightCount
              << " directional lights to " << maxDirectional << " supported";
      traces.push_back(message.str());
    }
    if (pointLightCount > maxPoint) {
      std::ostringstream message;
      message << "OpenGL raster shader truncated " << pointLightCount << " point lights to "
              << maxPoint << " supported";
      traces.push_back(message.str());
    }
  }

  std::string OpenGLRasterizer::statusMessage() {
    return "OpenGL raster backend renders the initial lit mesh path when Qt can create an "
           "offscreen context; unsupported hosts report an OpenGL capability error when the "
           "backend is selected";
  }

  const std::string& OpenGLRasterizer::readbackTraceMessage() const {
    return m_lastReadbackTraceMessage;
  }

  const std::vector<std::string>& OpenGLRasterizer::traceMessages() const {
    return m_lastTraceMessages;
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

  Rasterizer::MSAAShadingMode OpenGLRasterizer::msaaShadingMode() const {
    return m_msaaShadingMode;
  }

  void OpenGLRasterizer::setMSAAShadingMode(Rasterizer::MSAAShadingMode mode) {
    m_msaaShadingMode = mode;
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

  Rasterizer::AttachmentLoadOp OpenGLRasterizer::colorLoadOp() const {
    return m_colorLoadOp;
  }

  void OpenGLRasterizer::setColorLoadOp(Rasterizer::AttachmentLoadOp op) {
    m_colorLoadOp = op;
  }

  Rasterizer::AttachmentStoreOp OpenGLRasterizer::colorStoreOp() const {
    return m_colorStoreOp;
  }

  void OpenGLRasterizer::setColorStoreOp(Rasterizer::AttachmentStoreOp op) {
    m_colorStoreOp = op;
  }

  Rasterizer::DepthFunc OpenGLRasterizer::depthFunc() const {
    return m_depthFunc;
  }

  void OpenGLRasterizer::setDepthFunc(Rasterizer::DepthFunc func) {
    m_depthFunc = func;
  }

  double OpenGLRasterizer::depthBias() const {
    return m_depthBias;
  }

  void OpenGLRasterizer::setDepthBias(double bias) {
    m_depthBias = std::isfinite(bias) ? bias : 0.0;
  }

  double OpenGLRasterizer::depthClearValue() const {
    return m_depthClearValue;
  }

  void OpenGLRasterizer::setDepthClearValue(double value) {
    m_depthClearValue = value;
  }

  Rasterizer::AttachmentLoadOp OpenGLRasterizer::depthLoadOp() const {
    return m_depthLoadOp;
  }

  void OpenGLRasterizer::setDepthLoadOp(Rasterizer::AttachmentLoadOp op) {
    m_depthLoadOp = op;
  }

  Rasterizer::AttachmentStoreOp OpenGLRasterizer::depthStoreOp() const {
    return m_depthStoreOp;
  }

  void OpenGLRasterizer::setDepthStoreOp(Rasterizer::AttachmentStoreOp op) {
    m_depthStoreOp = op;
  }

  bool OpenGLRasterizer::depthWriteEnabled() const {
    return m_depthWriteEnabled;
  }

  void OpenGLRasterizer::setDepthWriteEnabled(bool enabled) {
    m_depthWriteEnabled = enabled;
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

  void
  OpenGLRasterizer::setVisibilitySet(std::shared_ptr<const RasterVisibilitySet> visibilitySet) {
    m_visibilitySet = std::move(visibilitySet);
  }

  void OpenGLRasterizer::clearVisibilitySet() {
    setVisibilitySet(nullptr);
  }

  std::shared_ptr<const RasterVisibilitySet> OpenGLRasterizer::visibilitySet() const {
    return m_visibilitySet;
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
                                                     bool copiedColor, bool copiedDepth,
                                                     bool copiedStencil) const {
    std::vector<std::string> attachments;
    if (copiedColor) {
      attachments.push_back("color");
    }
    if (copiedDepth) {
      attachments.push_back("depth");
    }
    if (copiedStencil) {
      attachments.push_back("stencil");
    }

    std::ostringstream message;
    if (attachments.empty()) {
      message << "OpenGL raster readback skipped CPU attachment copies in " << std::fixed
              << std::setprecision(3) << elapsed.count() / 1000000.0 << " ms";
      return message.str();
    }

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

  std::string OpenGLRasterizer::drawTraceMessage(std::chrono::nanoseconds elapsed,
                                                 std::size_t triangleCount,
                                                 std::size_t vertexBufferBytes,
                                                 std::size_t indexBufferBytes,
                                                 std::size_t imageTextureCount,
                                                 std::size_t imageTextureBytes) const {
    std::ostringstream message;
    message << "OpenGL raster draw uploaded " << vertexBufferBytes << " vertex bytes and "
            << indexBufferBytes << " index bytes";
    if (imageTextureCount != 0) {
      message << " plus " << imageTextureBytes << " texture bytes across " << imageTextureCount
              << " image texture";
      if (imageTextureCount != 1) {
        message << "s";
      }
    }
    message << ", prepared GPU state, and submitted " << triangleCount << " triangle";
    if (triangleCount != 1) {
      message << "s";
    }
    message << " in " << std::fixed << std::setprecision(3) << elapsed.count() / 1000000.0 << " ms";
    return message.str();
  }

  std::string OpenGLRasterizer::meshPreparationTraceMessage(std::chrono::nanoseconds elapsed,
                                                            std::size_t triangleCount) const {
    std::ostringstream message;
    message << "OpenGL raster mesh preparation built " << triangleCount << " triangle";
    if (triangleCount != 1) {
      message << "s";
    }
    message << " in " << std::fixed << std::setprecision(3) << elapsed.count() / 1000000.0 << " ms";
    return message.str();
  }

  void OpenGLRasterizer::renderOpenGL(int width, int height, Buffer<Colord>* colorTarget,
                                      Buffer<double>* depthTarget,
                                      Buffer<std::uint8_t>* stencilTarget) const {
    m_lastReadbackTraceMessage.clear();
    m_lastTraceMessages.clear();

    if (!m_resources) {
      m_resources = std::make_unique<detail::OpenGLRasterResourceCache>();
    }
    if (!m_resources->context.create(width, height, m_msaaSamples)) {
      throw std::runtime_error(m_resources->context.errorMessage());
    }

    const Recti viewport = viewportRectFor(width, height);
    const auto* shadowMaps = m_shadowMapsEnabled ? m_externalShadowMaps.get() : nullptr;
    const detail::OpenGLShadowSamplingPlan shadowSamplingPlan =
      detail::OpenGLShadowSamplingPlan::from(shadowMaps);
    const bool useShaderShadowSampling =
      shadowSamplingPlan.canShadeSceneDirectLighting(scene().get());
    detail::OpenGLShadowTextureData shadowTextureData =
      useShaderShadowSampling ? detail::OpenGLShadowTextureData::from(shadowSamplingPlan)
                              : detail::OpenGLShadowTextureData();
    const std::string shadowTextureTrace = shadowTextureData.traceMessage();
    const auto* meshShadowMaps = useShaderShadowSampling ? nullptr : shadowMaps;
    detail::OpenGLRasterMesh mesh;
    const auto meshPreparationStarted = std::chrono::steady_clock::now();
    if (viewport.width() > 0 && viewport.height() > 0) {
      mesh = detail::OpenGLRasterMeshBuilder(scene().get(), camera(), m_lod, viewport, m_cullMode,
                                             m_hasCullModeOverride, m_cancelled, meshShadowMaps,
                                             m_depthBias, m_visibilitySet)
               .build();
    }
    const auto meshPreparationElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - meshPreparationStarted);

    const auto timings =
      OpenGLRasterDrawPass(
        *m_resources, height, viewport, m_scissorTestEnabled, m_scissorRect, m_colorLoadOp,
        m_colorStoreOp, m_colorWriteMask, m_blendingEnabled, m_sourceBlendFactor,
        m_destinationBlendFactor, m_blendOp, m_blendConstantColor, m_blendConstantAlpha,
        m_alphaTestEnabled, m_alphaFunc, m_alphaReference, m_depthFunc, m_depthClearValue,
        m_depthLoadOp, m_depthStoreOp, m_depthWriteEnabled, m_stencilTestEnabled, m_stencilFunc,
        m_stencilReference, m_stencilMask, m_stencilClearValue, m_stencilLoadOp, m_stencilStoreOp,
        m_stencilWriteMask, m_stencilFailOp, m_stencilDepthFailOp, m_stencilPassOp,
        std::move(shadowTextureData), camera() ? camera()->position() : Vector3d::null, m_cullMode,
        m_hasCullModeOverride, m_cancelled)
        .render(mesh, backgroundColor(), colorTarget, depthTarget, stencilTarget);
    m_lastReadbackTraceMessage = readbackTraceMessage(
      timings.readbackElapsed,
      colorTarget != nullptr && m_colorStoreOp == Rasterizer::AttachmentStoreOp::Store,
      depthTarget != nullptr && m_depthStoreOp == Rasterizer::AttachmentStoreOp::Store,
      stencilTarget != nullptr && m_stencilStoreOp == Rasterizer::AttachmentStoreOp::Store);
    m_lastTraceMessages.push_back(
      meshPreparationTraceMessage(meshPreparationElapsed, mesh.triangleCount()));
    appendLightTruncationTrace(mesh.directionalLights().size(), mesh.pointLights().size(),
                               m_lastTraceMessages);
    m_lastTraceMessages.push_back(drawTraceMessage(
      timings.drawElapsed, mesh.triangleCount(), mesh.vertexBufferByteSize(),
      mesh.indexBufferByteSize(), mesh.imageTextureCount(), mesh.imageTextureUploadByteSize()));
    if (m_shadowMapsEnabled && m_externalShadowMaps) {
      m_lastTraceMessages.push_back(shadowSamplingPlan.traceMessage(scene().get()));
    }
    if (!shadowTextureTrace.empty()) {
      m_lastTraceMessages.push_back(shadowTextureTrace);
    }
    m_lastTraceMessages.push_back(m_lastReadbackTraceMessage);
  }
}
