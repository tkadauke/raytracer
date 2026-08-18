#include "engine/raster/OpenGLRasterizer.h"

#include "core/Buffer.h"
#include "engine/raster/OpenGLOffscreenContext.h"

#include <mutex>
#include "engine/raster/detail/OpenGLRasterAttributeBinder.h"
#include "engine/raster/detail/OpenGLRasterMesh.h"
#include "engine/raster/detail/OpenGLRasterResourceCache.h"
#include "engine/raster/detail/OpenGLRasterShaderSources.h"
#include "engine/raster/detail/OpenGLShadowSamplingPlan.h"
#include "engine/raster/detail/OpenGLFixedFunctionState.h"
#include "engine/raster/detail/OpenGLRasterDrawState.h"
#include "engine/raster/detail/OpenGLRasterTextures.h"
#include "engine/raster/detail/OpenGLShadowTextureData.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "engine/raster/gl/Bindings.h"
#include "render/cameras/Camera.h"
#include "render/textures/ImageTexture.h"
#include "render/viewplanes/ViewPlane.h"

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
    struct OpenGLRasterRenderTimings {
      std::chrono::nanoseconds makeCurrentElapsed{0};
      std::chrono::nanoseconds drawElapsed{0};
      std::chrono::nanoseconds glFinishElapsed{0};
      std::chrono::nanoseconds readbackElapsed{0};
      std::chrono::nanoseconds doneCurrentElapsed{0};
    };

    class OpenGLRasterDrawPass {
    public:
      OpenGLRasterDrawPass(detail::OpenGLRasterResourceCache& resources,
                           detail::OpenGLRasterDrawState state, const std::atomic<bool>& cancelled)
          : m_resources(resources),
            m_width(state.width),
            m_height(state.height),
            m_samples(state.samples),
            m_viewportRect(state.viewportRect),
            m_scissorEnabled(state.scissorEnabled),
            m_scissorRect(state.scissorRect),
            m_colorLoadOp(state.colorLoadOp),
            m_loadColorAttachment(state.loadColorAttachment),
            m_colorStoreOp(state.colorStoreOp),
            m_colorWriteMask(state.colorWriteMask),
            m_blendingEnabled(state.blendingEnabled),
            m_sourceBlendFactor(state.sourceBlendFactor),
            m_destinationBlendFactor(state.destinationBlendFactor),
            m_blendOp(state.blendOp),
            m_blendConstantColor(state.blendConstantColor),
            m_blendConstantAlpha(state.blendConstantAlpha),
            m_alphaTestEnabled(state.alphaTestEnabled),
            m_alphaFunc(state.alphaFunc),
            m_alphaReference(state.alphaReference),
            m_depthFunc(state.depthFunc),
            m_depthClearValue(state.depthClearValue),
            m_depthStoreOp(state.depthStoreOp),
            m_depthWriteEnabled(state.depthWriteEnabled),
            m_stencilTestEnabled(state.stencilTestEnabled),
            m_stencilFunc(state.stencilFunc),
            m_stencilReference(state.stencilReference),
            m_stencilMask(state.stencilMask),
            m_stencilClearValue(state.stencilClearValue),
            m_stencilStoreOp(state.stencilStoreOp),
            m_stencilWriteMask(state.stencilWriteMask),
            m_stencilFailOp(state.stencilFailOp),
            m_stencilDepthFailOp(state.stencilDepthFailOp),
            m_stencilPassOp(state.stencilPassOp),
            m_shadowTextureData(std::move(state.shadowTextureData)),
            m_cameraPosition(state.cameraPosition),
            m_viewProjection(std::move(state.viewProjection)),
            m_cullMode(state.cullMode),
            m_hasCullModeOverride(state.hasCullModeOverride),
            m_skipMeshUpload(state.skipMeshUpload),
            m_cancelled(cancelled) {
      }

      OpenGLRasterRenderTimings render(const detail::OpenGLRasterMesh& mesh,
                                       const Colord& background, Buffer<Colord>* target,
                                       Buffer<double>* depthTarget,
                                       Buffer<std::uint8_t>* stencilTarget) {
        OpenGLRasterRenderTimings timings;
        const auto makeCurrentStarted = std::chrono::steady_clock::now();
        if (!m_resources.context->makeCurrent()) {
          throw std::runtime_error(m_resources.context->errorMessage());
        }
        gl::AttachmentSet* attachmentSet =
          m_resources.acquireAttachmentSet(m_width, m_height, m_samples);
        if (!attachmentSet) {
          const std::string error = "OpenGL raster backend could not allocate attachment set (" +
                                    std::to_string(m_width) + "x" + std::to_string(m_height) +
                                    " samples=" + std::to_string(m_samples) + ")";
          m_resources.context->doneCurrent();
          m_resources.context->detachFromCurrentThread();
          throw std::runtime_error(error);
        }
        attachmentSet->bind();
        timings.makeCurrentElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - makeCurrentStarted);

        try {
          const auto drawStarted = std::chrono::steady_clock::now();
          draw(mesh, background);
          timings.drawElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - drawStarted);
          const auto glFinishStarted = std::chrono::steady_clock::now();
          glFinish();
          timings.glFinishElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - glFinishStarted);
          const auto readbackStarted = std::chrono::steady_clock::now();
          if (target && m_colorStoreOp == Rasterizer::AttachmentStoreOp::Store) {
            attachmentSet->copyColorTo(*target);
          }
          if (depthTarget && m_depthStoreOp == Rasterizer::AttachmentStoreOp::Store) {
            attachmentSet->copyDepthTo(*depthTarget);
          }
          if (stencilTarget && m_stencilStoreOp == Rasterizer::AttachmentStoreOp::Store) {
            attachmentSet->copyStencilTo(*stencilTarget);
          }
          timings.readbackElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - readbackStarted);
          const auto doneCurrentStarted = std::chrono::steady_clock::now();
          attachmentSet->release();
          m_resources.context->doneCurrent();
          m_resources.context->detachFromCurrentThread();
          timings.doneCurrentElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - doneCurrentStarted);
          return timings;
        } catch (...) {
          attachmentSet->release();
          m_resources.context->doneCurrent();
          m_resources.context->detachFromCurrentThread();
          throw;
        }
      }

    private:
      void draw(const detail::OpenGLRasterMesh& mesh, const Colord& background) {
        // Attachment-load support is checked in
        // `OpenGLRasterizer::renderOpenGL` before the context is bound;
        // by the time we reach `draw()`, color/depth/stencil load ops
        // are all guaranteed to be `Clear` or `DontCare`.
        glViewport(m_viewportRect.left(), openGLY(m_viewportRect), m_viewportRect.width(),
                   m_viewportRect.height());
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearColor(static_cast<GLfloat>(std::clamp(background.r(), 0.0, 1.0)),
                     static_cast<GLfloat>(std::clamp(background.g(), 0.0, 1.0)),
                     static_cast<GLfloat>(std::clamp(background.b(), 0.0, 1.0)), 1.0f);
        glClearDepth(detail::normalizedDepthClearValue(m_depthClearValue));
        glStencilMask(0xff);
        glClearStencil(m_stencilClearValue);
        GLbitfield clearMask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        if (m_colorLoadOp == Rasterizer::AttachmentLoadOp::Load && m_loadColorAttachment) {
          uploadLoadColor(*m_loadColorAttachment);
          clearMask &= ~static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT);
        }
        glClear(clearMask);
        const detail::OpenGLRasterDrawState fixedState = fixedFunctionStateSlice();
        detail::applyDepth(fixedState);
        detail::applyColorWriteMask(fixedState);
        detail::applyBlending(fixedState);
        detail::applyStencil(fixedState);
        detail::applyCullMode(fixedState);
        detail::OpenGLShadowTexture shadowTexture(m_shadowTextureData);

        if (mesh.empty()) {
          glFlush();
          detail::resetFixedFunctionState();
          return;
        }

        detail::applyScissor(fixedState, m_height);

        detail::OpenGLFallbackTexture fallbackTexture;
        m_resources.ensureProgram();
        gl::ShaderProgram& program = *m_resources.program;
        if (!program.bind()) {
          throw std::runtime_error("OpenGL raster backend could not bind shader program");
        }
        program.setUniformValue("useMatrixProjection", m_viewProjection.has_value());
        if (m_viewProjection) {
          const Matrix4f gpu(*m_viewProjection);
          const int loc = program.uniformLocation("viewProjection");
          if (loc >= 0) {
            glUniformMatrix4fv(loc, 1, GL_TRUE, gpu.data());
          }
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

        m_resources.ensureMeshBuffers();
        gl::Buffer& vertexBuffer = *m_resources.vertexBuffer;
        gl::Buffer& indexBuffer = *m_resources.indexBuffer;

        vertexBuffer.bind();
        indexBuffer.bind();
        if (!m_skipMeshUpload) {
          vertexBuffer.allocate(
            mesh.vertices().data(),
            static_cast<int>(mesh.vertices().size() * sizeof(detail::OpenGLRasterMesh::Vertex)));
          indexBuffer.allocate(mesh.indices().data(),
                               static_cast<int>(mesh.indices().size() * sizeof(std::uint32_t)));
        }

        detail::bindVertexAttributes(program, m_resources.locations);

        if (!m_resources.imageTextures) {
          m_resources.imageTextures = std::make_unique<detail::OpenGLRasterImageTextureCache>();
        }
        for (const auto& batch : mesh.batches()) {
          if (m_cancelled.load()) {
            break;
          }
          glActiveTexture(GL_TEXTURE0);
          if (batch.albedo.mode == detail::RasterAlbedoShaderMode::ImageTexture &&
              batch.albedo.image) {
            glBindTexture(GL_TEXTURE_2D,
                          m_resources.imageTextures->textureFor(*batch.albedo.image));
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
          glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.indexCount), GL_UNSIGNED_INT,
                         byteOffset);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        if (shadowTexture.enabled()) {
          shadowTexture.release(1);
        } else {
          fallbackTexture.release(1);
        }
        fallbackTexture.release(0);
        glActiveTexture(GL_TEXTURE0);
        glFlush();

        detail::resetFixedFunctionState();

        detail::unbindVertexAttributes(program, m_resources.locations);
        indexBuffer.release();
        vertexBuffer.release();
        program.release();
      }

      int openGLY(const Recti& rect) const {
        return m_height - rect.bottom();
      }

      /**
        * Upload a `Buffer<Colord>` into the currently-bound color
        * attachment via a temporary `GL_RGBA32F` texture + FBO blit.
        * Called by the draw pass when `colorLoadOp == Load` to
        * preserve the previous pass's color contents before drawing.
        * The source buffer is read row-flipped (the AttachmentSet
        * stores its color attachment with row-0 at the GL bottom; the
        * caller's `Buffer<Colord>` has row-0 at the visible top).
        */
      void uploadLoadColor(const Buffer<Colord>& source) {
        const int width = std::min(source.width(), m_width);
        const int height = std::min(source.height(), m_height);
        if (width <= 0 || height <= 0) {
          return;
        }

        std::vector<GLfloat> pixels(static_cast<std::size_t>(width * height * 4), 0.0f);
        for (int y = 0; y != height; ++y) {
          const int sourceY = height - 1 - y;
          for (int x = 0; x != width; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 4);
            const Colord& c = source[sourceY][x];
            pixels[offset + 0] = static_cast<GLfloat>(c.r());
            pixels[offset + 1] = static_cast<GLfloat>(c.g());
            pixels[offset + 2] = static_cast<GLfloat>(c.b());
            pixels[offset + 3] = 1.0f;
          }
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        // GL_RGBA32F is GL 3.0+ / EXT_color_buffer_float; fall back to
        // GL_RGBA8 on legacy contexts. The blit downstream resolves to
        // GL_RGBA8 on the attachment renderbuffer anyway, so the
        // precision drop is invisible in the final color attachment.
#if defined(GL_RGBA32F)
        const GLint internalFormat = GL_RGBA32F;
#else
        const GLint internalFormat = GL_RGBA8;
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_FLOAT,
                     pixels.data());

        GLuint sourceFbo = 0;
        glGenFramebuffers(1, &sourceFbo);
        // Cache the currently-bound draw FBO so we can restore it after
        // the blit; AttachmentSet::bind() bound it before draw() entry.
        GLint drawFbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &drawFbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT,
                          GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
        glDeleteFramebuffers(1, &sourceFbo);
        glDeleteTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, 0);
      }

      // Fixed-function GL state translation lives in
      // engine/raster/detail/OpenGLFixedFunctionState.h. The thin
      // wrappers here adapt the draw-pass member view to the free
      // functions until the rest of the draw pass migrates to a state
      // struct as well.
      detail::OpenGLRasterDrawState fixedFunctionStateSlice() const {
        detail::OpenGLRasterDrawState slice;
        slice.scissorEnabled = m_scissorEnabled;
        slice.scissorRect = m_scissorRect;
        slice.colorWriteMask = m_colorWriteMask;
        slice.blendingEnabled = m_blendingEnabled;
        slice.sourceBlendFactor = m_sourceBlendFactor;
        slice.destinationBlendFactor = m_destinationBlendFactor;
        slice.blendOp = m_blendOp;
        slice.blendConstantColor = m_blendConstantColor;
        slice.blendConstantAlpha = m_blendConstantAlpha;
        slice.depthFunc = m_depthFunc;
        slice.depthWriteEnabled = m_depthWriteEnabled;
        slice.stencilTestEnabled = m_stencilTestEnabled;
        slice.stencilFunc = m_stencilFunc;
        slice.stencilReference = m_stencilReference;
        slice.stencilMask = m_stencilMask;
        slice.stencilWriteMask = m_stencilWriteMask;
        slice.stencilFailOp = m_stencilFailOp;
        slice.stencilDepthFailOp = m_stencilDepthFailOp;
        slice.stencilPassOp = m_stencilPassOp;
        slice.cullMode = m_cullMode;
        slice.hasCullModeOverride = m_hasCullModeOverride;
        return slice;
      }

      void setShadowUniforms(gl::ShaderProgram& program, bool enabled) const {
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

      void setLightingUniforms(gl::ShaderProgram& program,
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

      void setVectorUniform(gl::ShaderProgram& program, const char* name,
                            const Vector3d& value) const {
        program.setUniformValue(name, static_cast<GLfloat>(value.x()),
                                static_cast<GLfloat>(value.y()), static_cast<GLfloat>(value.z()));
      }

      std::string uniformName(const char* arrayName, int index) const {
        std::ostringstream name;
        name << arrayName << "[" << index << "]";
        return name.str();
      }

      detail::OpenGLRasterResourceCache& m_resources;
      int m_width;
      int m_height;
      int m_samples;
      Recti m_viewportRect;
      bool m_scissorEnabled;
      Recti m_scissorRect;
      Rasterizer::AttachmentLoadOp m_colorLoadOp;
      const Buffer<Colord>* m_loadColorAttachment;
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
      Rasterizer::AttachmentStoreOp m_depthStoreOp;
      bool m_depthWriteEnabled;
      bool m_stencilTestEnabled;
      Rasterizer::StencilFunc m_stencilFunc;
      std::uint8_t m_stencilReference;
      std::uint8_t m_stencilMask;
      std::uint8_t m_stencilClearValue;
      Rasterizer::AttachmentStoreOp m_stencilStoreOp;
      std::uint8_t m_stencilWriteMask;
      Rasterizer::StencilOp m_stencilFailOp;
      Rasterizer::StencilOp m_stencilDepthFailOp;
      Rasterizer::StencilOp m_stencilPassOp;
      detail::OpenGLShadowTextureData m_shadowTextureData;
      Vector3d m_cameraPosition;
      std::optional<Matrix4d> m_viewProjection;
      Rasterizer::CullMode m_cullMode;
      bool m_hasCullModeOverride;
      bool m_skipMeshUpload;
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
    copyRenderEngineStateTo(*clone);
    clone->setLod(m_lod);
    clone->setTessellationQuality(m_tessellationQuality);
    if (std::isfinite(m_maximumScreenSpaceError)) {
      clone->setMaximumScreenSpaceError(m_maximumScreenSpaceError);
    }
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
    clone->setColorLoadSource(m_loadColorAttachment);
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
    clone->setDepthLoadSource(m_loadDepthAttachment);
    clone->setDepthStoreOp(m_depthStoreOp);
    clone->setDepthWriteEnabled(m_depthWriteEnabled);
    clone->setStencilTestEnabled(m_stencilTestEnabled);
    clone->setStencilFunc(m_stencilFunc, m_stencilReference, m_stencilMask);
    clone->setStencilClearValue(m_stencilClearValue);
    clone->setStencilLoadOp(m_stencilLoadOp);
    clone->setStencilLoadSource(m_loadStencilAttachment);
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

  std::shared_ptr<detail::OpenGLRasterResourceCache> OpenGLRasterizer::sharedResources() {
    static std::mutex mutex;
    // Heap-allocated and intentionally leaked: at process exit, Qt's
    // own static state (the mutex inside `qt_gl_functions_resource`)
    // has already been torn down by `__cxa_finalize_ranges`. Running
    // `~OpenGLRasterResourceCache` then — even with the context current
    // — crashes inside `QOpenGLBuffer::destroy` →
    // `QOpenGLContextGroupPrivate::deletePendingResources` →
    // `QOpenGLMultiGroupSharedResource::value` when it tries to lock
    // the destroyed mutex. Keeping a strong ref through a never-freed
    // heap pointer means the cache destructor never runs; the OS
    // reclaims the GL handles when the process exits.
    static std::shared_ptr<detail::OpenGLRasterResourceCache>* cache = nullptr;
    std::lock_guard<std::mutex> lock(mutex);
    if (!cache) {
      cache = new std::shared_ptr<detail::OpenGLRasterResourceCache>(
        std::make_shared<detail::OpenGLRasterResourceCache>());
    }
    return *cache;
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
    m_lod = atLeast(0, lod);
  }

  Rasterizer::TessellationQuality OpenGLRasterizer::tessellationQuality() const {
    return m_tessellationQuality;
  }

  void OpenGLRasterizer::setTessellationQuality(Rasterizer::TessellationQuality quality) {
    m_tessellationQuality = quality;
  }

  double OpenGLRasterizer::maximumScreenSpaceError() const {
    return std::isfinite(m_maximumScreenSpaceError)
             ? m_maximumScreenSpaceError
             : Rasterizer::presetScreenSpaceError(m_tessellationQuality);
  }

  void OpenGLRasterizer::setMaximumScreenSpaceError(double pixels) {
    m_maximumScreenSpaceError = finiteAtLeast(0.0, pixels);
  }

  void OpenGLRasterizer::clearMaximumScreenSpaceErrorOverride() {
    m_maximumScreenSpaceError = std::numeric_limits<double>::quiet_NaN();
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

  const Buffer<Colord>* OpenGLRasterizer::colorLoadSource() const {
    return m_loadColorAttachment;
  }

  void OpenGLRasterizer::setColorLoadSource(const Buffer<Colord>* buffer) {
    m_loadColorAttachment = buffer;
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

  const Buffer<double>* OpenGLRasterizer::depthLoadSource() const {
    return m_loadDepthAttachment;
  }

  void OpenGLRasterizer::setDepthLoadSource(const Buffer<double>* buffer) {
    m_loadDepthAttachment = buffer;
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

  const Buffer<std::uint8_t>* OpenGLRasterizer::stencilLoadSource() const {
    return m_loadStencilAttachment;
  }

  void OpenGLRasterizer::setStencilLoadSource(const Buffer<std::uint8_t>* buffer) {
    m_loadStencilAttachment = buffer;
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

  std::string
  OpenGLRasterizer::drawTraceMessage(std::chrono::nanoseconds elapsed, std::size_t triangleCount,
                                     std::size_t vertexBufferBytes, std::size_t indexBufferBytes,
                                     std::size_t imageTextureCount, std::size_t imageTextureBytes,
                                     bool uploadedMesh) const {
    std::ostringstream message;
    message << "OpenGL raster draw ";
    if (uploadedMesh) {
      message << "uploaded " << vertexBufferBytes << " vertex bytes and " << indexBufferBytes
              << " index bytes";
    } else {
      message << "reused " << vertexBufferBytes << " vertex bytes and " << indexBufferBytes
              << " index bytes from cache";
    }
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

  std::string OpenGLRasterizer::latencyBreakdownTraceMessage(
    std::chrono::nanoseconds makeCurrentElapsed, std::chrono::nanoseconds glFinishElapsed,
    std::chrono::nanoseconds doneCurrentElapsed) const {
    std::ostringstream message;
    message << std::fixed << std::setprecision(3)
            << "OpenGL raster per-frame overhead: makeCurrent+bindFBO "
            << makeCurrentElapsed.count() / 1000000.0 << " ms, glFinish (GPU wait) "
            << glFinishElapsed.count() / 1000000.0 << " ms, releaseFBO+doneCurrent "
            << doneCurrentElapsed.count() / 1000000.0 << " ms";
    return message.str();
  }

  std::string OpenGLRasterizer::meshPreparationTraceMessage(std::chrono::nanoseconds elapsed,
                                                            std::size_t triangleCount,
                                                            bool cacheHit) const {
    std::ostringstream message;
    message << "OpenGL raster mesh preparation " << (cacheHit ? "reused " : "built ")
            << triangleCount << " triangle";
    if (triangleCount != 1) {
      message << "s";
    }
    if (cacheHit) {
      message << " from cache";
    }
    message << " in " << std::fixed << std::setprecision(3) << elapsed.count() / 1000000.0 << " ms";
    return message.str();
  }

  void OpenGLRasterizer::renderOpenGL(int width, int height, Buffer<Colord>* colorTarget,
                                      Buffer<double>* depthTarget,
                                      Buffer<std::uint8_t>* stencilTarget) const {
    m_lastReadbackTraceMessage.clear();
    m_lastTraceMessages.clear();

    // `AttachmentLoadOp::Load` requires the caller to provide a
    // matching CPU source buffer via `setLoadColorAttachment` /
    // `setLoadDepthAttachment` / `setLoadStencilAttachment`. The
    // backend uploads that buffer to the attachment at pass start
    // instead of clearing. The CPU round-trip here is the consumer
    // side of `opengl-gpu-residency.md` Phase 2 — the producer side
    // (publishing the FBO contents as a `Buffer<...>` on the graph
    // storage) already exists via the readback path. Full GPU
    // residency (no round-trip when both producer and consumer are
    // GPU-domain) is Phase 0-1 follow-up work.
    //
    // Throwing pre-bind keeps the failure observable as a render-graph
    // diagnostic rather than as a half-bound GL frame.
    const auto requireLoadSource = [](const char* attachment, Rasterizer::AttachmentLoadOp op,
                                      const void* source) {
      if (op == Rasterizer::AttachmentLoadOp::Load && source == nullptr) {
        std::string message = "OpenGL raster backend ";
        message += attachment;
        message += " attachment Load requires a source buffer set via setLoadColorAttachment / "
                   "setLoadDepthAttachment / setLoadStencilAttachment";
        throw std::runtime_error(message);
      }
    };
    requireLoadSource("color", m_colorLoadOp, m_loadColorAttachment);
    requireLoadSource("depth", m_depthLoadOp, m_loadDepthAttachment);
    requireLoadSource("stencil", m_stencilLoadOp, m_loadStencilAttachment);

    // Depth and stencil Load are part of the same residency arc but
    // need their own upload helpers (format conversions, blit masks);
    // shipping them is opengl-gpu-residency.md Phase 2 follow-up.
    // Throw with a narrow, named error rather than a generic
    // "unsupported" message so the caller knows exactly which slice is
    // missing.
    if (m_depthLoadOp == Rasterizer::AttachmentLoadOp::Load) {
      throw std::runtime_error(
        "OpenGL raster backend depth attachment Load is not yet implemented; "
        "tracked under opengl-gpu-residency.md Phase 2 follow-up");
    }
    if (m_stencilLoadOp == Rasterizer::AttachmentLoadOp::Load) {
      throw std::runtime_error(
        "OpenGL raster backend stencil attachment Load is not yet implemented; "
        "tracked under opengl-gpu-residency.md Phase 2 follow-up");
    }

    if (!m_resources) {
      m_resources = sharedResources();
    }
    if (!m_resources->context->migrateToCurrentThread()) {
      // The cache's offscreen context was bound to a thread that has
      // since exited. Replace it locally so this render and future ones
      // start clean instead of retrying the failed migration every
      // frame.
      m_resources = std::make_shared<detail::OpenGLRasterResourceCache>();
    }
    // The context only manages lifecycle now; attachment dimensions
    // and sample counts live on the per-render attachment set (see
    // OpenGLRasterDrawPass::render -> acquireAttachmentSet).
    if (!m_resources->context->create()) {
      throw std::runtime_error(m_resources->context->errorMessage());
    }

    const Recti viewport = viewportRectFor(width, height);
    const std::shared_ptr<const detail::ShadowMaps> shadowMapsPtr =
      m_shadowMapsEnabled ? m_externalShadowMaps : nullptr;
    const detail::OpenGLShadowSamplingPlan shadowSamplingPlan =
      detail::OpenGLShadowSamplingPlan::from(shadowMapsPtr.get());
    const bool useShaderShadowSampling =
      shadowSamplingPlan.canShadeSceneDirectLighting(scene().get());
    detail::OpenGLShadowTextureData shadowTextureData =
      useShaderShadowSampling ? detail::OpenGLShadowTextureData::from(shadowSamplingPlan)
                              : detail::OpenGLShadowTextureData();
    const std::string shadowTextureTrace = shadowTextureData.traceMessage();
    const std::shared_ptr<const detail::ShadowMaps> meshShadowMapsPtr =
      useShaderShadowSampling ? nullptr : shadowMapsPtr;
    const auto* meshShadowMaps = meshShadowMapsPtr.get();
    const auto meshPreparationStarted = std::chrono::steady_clock::now();
    bool meshCacheHit = false;
    detail::OpenGLRasterMesh* activeMesh = nullptr;
    std::ptrdiff_t activeMeshSlot = -1;
    if (viewport.width() > 0 && viewport.height() > 0) {
      const detail::OpenGLRasterMeshBuilder builder(
        scene().get(), camera(), m_lod, m_tessellationQuality, maximumScreenSpaceError(), viewport,
        m_cullMode, m_hasCullModeOverride, m_cancelled, meshShadowMaps, m_depthBias,
        m_visibilitySet);
      detail::OpenGLMeshCacheKey key;
      key.scene = scene();
      key.visibilitySet = m_visibilitySet;
      key.shadowMaps = meshShadowMapsPtr;
      key.lod = m_lod;
      key.tessellationQuality = m_tessellationQuality;
      key.maximumScreenSpaceError = maximumScreenSpaceError();
      key.viewportWidth = viewport.width();
      key.viewportHeight = viewport.height();
      key.cullMode = m_cullMode;
      key.hasCullModeOverride = m_hasCullModeOverride;
      key.depthBias = m_depthBias;
      // The mesh build path is camera-dependent unless every light is
      // handled in the fragment shader AND the camera supplies a GPU
      // projection matrix. With both gates true, the builder skips
      // per-vertex projection, per-primitive frustum cull, and
      // triangle clipping (`isCameraIndependentBuildAvailable()`);
      // the resulting mesh is reusable across camera moves and the
      // cache key drops the camera pose. Otherwise the camera pose is
      // load-bearing — frustum culling and CPU specular bake would
      // leave holes / stale highlights if the cache hit across a move.
      key.cameraDependent = !builder.isCameraIndependentBuildAvailable();
      if (key.cameraDependent && camera()) {
        key.cameraPosition = camera()->position();
        key.cameraTarget = camera()->target();
      }
      const auto slotResult = m_resources->acquireMeshSlot(key);
      activeMeshSlot = slotResult.slot;
      activeMesh = &slotResult.entry->mesh;
      if (slotResult.hit) {
        meshCacheHit = true;
        // Mesh build calls `viewPlane()->setup` as a side effect; on a
        // cache hit the side effect is skipped. The view plane is shared
        // with other render passes that may have left it set up for a
        // different viewport (shadow maps, picking, separate post-
        // process passes), and `worldToClipMatrix` below reads
        // `viewPlane()->hSpan()/vSpan()` to compose the projection
        // matrix. Re-run setup so the matrix matches this pass's
        // viewport. Without this, cache hits intermittently render with
        // another pass's projection — the symptom is a scene squished
        // along the buffer axis whose ratio differs from the other
        // pass's viewport.
        if (camera() && camera()->viewPlane()) {
          camera()->viewPlane()->setup(camera()->matrix(), viewport);
        }
      } else {
        slotResult.entry->mesh = builder.build();
        slotResult.entry->key = key;
        // The slot we just rebuilt may be the same one whose GL bytes
        // are currently in the VBO; if so, the GPU buffer is now stale.
        if (m_resources->uploadedMeshSlot == slotResult.slot) {
          m_resources->uploadedMeshSlot = -1;
        }
      }
    } else {
      activeMesh = &m_resources->meshCache[0].mesh;
      m_resources->meshCache[0].mesh = detail::OpenGLRasterMesh();
      m_resources->meshCache[0].key.reset();
      m_resources->meshCache[0].lastUsed = 0;
      activeMeshSlot = 0;
      if (m_resources->uploadedMeshSlot == 0) {
        m_resources->uploadedMeshSlot = -1;
      }
    }
    detail::OpenGLRasterMesh& mesh = *activeMesh;
    const auto meshPreparationElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - meshPreparationStarted);

    // Pull the camera's GL view-projection matrix once per render after
    // the mesh builder has set up the view plane. PinholeCamera returns
    // a value; other camera types (thin-lens, fish-eye, …) return
    // nullopt and the shader falls back to the legacy per-vertex baked
    // clip-space positions.
    const std::optional<Matrix4d> viewProjection =
      camera() ? camera()->worldToClipMatrix() : std::optional<Matrix4d>{};

    // FitExact aspect mode confines rendered content to the view plane's
    // inner rect (letterbox/pillarbox bars elsewhere). The CPU rasterizer
    // gets this for free because `projectPoint` maps world coords through
    // `inner.width()/inner.height()`. The GL viewport has to mirror that
    // — otherwise NDC stretches the projection across the full buffer
    // and content appears squished along the buffer's wider axis. The
    // clear pass still hits the full framebuffer (glClear ignores the
    // viewport), so bars naturally take the background color.
    Recti drawViewport = viewport;
    if (!m_viewportEnabled && camera() && camera()->viewPlane() &&
        camera()->viewPlane()->aspectMode() == render::AspectMode::FitExact) {
      drawViewport = camera()->viewPlane()->innerRect();
    }

    detail::OpenGLRasterDrawState drawState;
    drawState.width = width;
    drawState.height = height;
    drawState.samples = m_msaaSamples;
    drawState.viewportRect = drawViewport;
    drawState.scissorEnabled = m_scissorTestEnabled;
    drawState.scissorRect = m_scissorRect;
    drawState.colorLoadOp = m_colorLoadOp;
    drawState.loadColorAttachment = m_loadColorAttachment;
    drawState.colorStoreOp = m_colorStoreOp;
    drawState.colorWriteMask = m_colorWriteMask;
    drawState.blendingEnabled = m_blendingEnabled;
    drawState.sourceBlendFactor = m_sourceBlendFactor;
    drawState.destinationBlendFactor = m_destinationBlendFactor;
    drawState.blendOp = m_blendOp;
    drawState.blendConstantColor = m_blendConstantColor;
    drawState.blendConstantAlpha = m_blendConstantAlpha;
    drawState.alphaTestEnabled = m_alphaTestEnabled;
    drawState.alphaFunc = m_alphaFunc;
    drawState.alphaReference = m_alphaReference;
    drawState.depthFunc = m_depthFunc;
    drawState.depthClearValue = m_depthClearValue;
    drawState.depthLoadOp = m_depthLoadOp;
    drawState.depthStoreOp = m_depthStoreOp;
    drawState.depthWriteEnabled = m_depthWriteEnabled;
    drawState.stencilTestEnabled = m_stencilTestEnabled;
    drawState.stencilFunc = m_stencilFunc;
    drawState.stencilReference = m_stencilReference;
    drawState.stencilMask = m_stencilMask;
    drawState.stencilClearValue = m_stencilClearValue;
    drawState.stencilLoadOp = m_stencilLoadOp;
    drawState.stencilStoreOp = m_stencilStoreOp;
    drawState.stencilWriteMask = m_stencilWriteMask;
    drawState.stencilFailOp = m_stencilFailOp;
    drawState.stencilDepthFailOp = m_stencilDepthFailOp;
    drawState.stencilPassOp = m_stencilPassOp;
    drawState.shadowTextureData = std::move(shadowTextureData);
    drawState.cameraPosition = camera() ? camera()->position() : Vector3d::null;
    drawState.viewProjection = viewProjection;
    drawState.cullMode = m_cullMode;
    drawState.hasCullModeOverride = m_hasCullModeOverride;

    // Upload skip: the GPU vertex/index buffer holds bytes for whichever
    // mesh-cache slot last wrote them (tracked in `uploadedMeshSlot`).
    // When the active slot already matches, the draw pass skips the
    // re-upload — saves ~50 MB/frame for the sloth on cache hits and
    // keeps the LRU effective when several passes rotate through
    // different cached meshes.
    const bool uploadedFreshMesh = m_resources->uploadedMeshSlot != activeMeshSlot;
    drawState.skipMeshUpload = !uploadedFreshMesh;
    const auto timings =
      OpenGLRasterDrawPass(*m_resources, std::move(drawState), m_cancelled)
        .render(mesh, backgroundColor(), colorTarget, depthTarget, stencilTarget);
    if (uploadedFreshMesh) {
      m_resources->uploadedMeshSlot = activeMeshSlot;
    }
    m_lastReadbackTraceMessage = readbackTraceMessage(
      timings.readbackElapsed,
      colorTarget != nullptr && m_colorStoreOp == Rasterizer::AttachmentStoreOp::Store,
      depthTarget != nullptr && m_depthStoreOp == Rasterizer::AttachmentStoreOp::Store,
      stencilTarget != nullptr && m_stencilStoreOp == Rasterizer::AttachmentStoreOp::Store);
    m_lastTraceMessages.push_back(
      meshPreparationTraceMessage(meshPreparationElapsed, mesh.triangleCount(), meshCacheHit));
    appendLightTruncationTrace(mesh.directionalLights().size(), mesh.pointLights().size(),
                               m_lastTraceMessages);
    m_lastTraceMessages.push_back(
      drawTraceMessage(timings.drawElapsed, mesh.triangleCount(), mesh.vertexBufferByteSize(),
                       mesh.indexBufferByteSize(), mesh.imageTextureCount(),
                       mesh.imageTextureUploadByteSize(), uploadedFreshMesh));
    m_lastTraceMessages.push_back(latencyBreakdownTraceMessage(
      timings.makeCurrentElapsed, timings.glFinishElapsed, timings.doneCurrentElapsed));
    if (m_shadowMapsEnabled && m_externalShadowMaps) {
      m_lastTraceMessages.push_back(shadowSamplingPlan.traceMessage(scene().get()));
    }
    if (!shadowTextureTrace.empty()) {
      m_lastTraceMessages.push_back(shadowTextureTrace);
    }
    m_lastTraceMessages.push_back(m_lastReadbackTraceMessage);
  }
}
