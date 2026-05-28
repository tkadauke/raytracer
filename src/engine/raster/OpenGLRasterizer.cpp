#include "engine/raster/OpenGLRasterizer.h"

#include "core/Buffer.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/detail/OpenGLRasterMesh.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace engine::raster {
  namespace {
    class OpenGLRasterDrawPass {
    public:
      OpenGLRasterDrawPass(OpenGLOffscreenContext& context, int height, const Recti& viewportRect,
                           bool scissorEnabled, const Recti& scissorRect,
                           std::uint8_t colorWriteMask)
          : m_context(context),
            m_height(height),
            m_viewportRect(viewportRect),
            m_scissorEnabled(scissorEnabled),
            m_scissorRect(scissorRect),
            m_colorWriteMask(colorWriteMask) {
      }

      void render(const detail::OpenGLRasterMesh& mesh, const Colord& background,
                  Buffer<Colord>& target, Buffer<double>* depthTarget) {
        if (!m_context.makeCurrent()) {
          throw std::runtime_error(m_context.errorMessage());
        }
        if (!m_context.bindFramebuffer()) {
          m_context.doneCurrent();
          throw std::runtime_error(m_context.errorMessage());
        }

        try {
          draw(mesh, background);
          m_context.copyColorTo(target);
          if (depthTarget) {
            m_context.copyDepthTo(*depthTarget);
          }
          m_context.releaseFramebuffer();
          m_context.doneCurrent();
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
        functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        applyColorWriteMask(functions);

        if (mesh.empty()) {
          functions->glFlush();
          functions->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
          return;
        }

        applyScissor(functions);

        QOpenGLShaderProgram program;
        if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                             "attribute vec3 position;\n"
                                             "attribute vec3 color;\n"
                                             "varying vec3 vertexColor;\n"
                                             "void main() {\n"
                                             "  gl_Position = vec4(position, 1.0);\n"
                                             "  vertexColor = color;\n"
                                             "}\n")) {
          throw std::runtime_error("OpenGL raster backend could not compile vertex shader: " +
                                   program.log().toStdString());
        }
        if (!program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                             "varying vec3 vertexColor;\n"
                                             "void main() {\n"
                                             "  gl_FragColor = vec4(vertexColor, 1.0);\n"
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
        if (positionLocation < 0 || colorLocation < 0) {
          indexBuffer.release();
          vertexBuffer.release();
          program.release();
          throw std::runtime_error("OpenGL raster backend shader attributes are unavailable");
        }

        program.enableAttributeArray(positionLocation);
        program.setAttributeBuffer(positionLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, x), 3,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));
        program.enableAttributeArray(colorLocation);
        program.setAttributeBuffer(colorLocation, GL_FLOAT,
                                   offsetof(detail::OpenGLRasterMesh::Vertex, r), 3,
                                   sizeof(detail::OpenGLRasterMesh::Vertex));

        functions->glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices().size()),
                                  GL_UNSIGNED_INT, nullptr);
        functions->glFlush();

        functions->glDisable(GL_SCISSOR_TEST);
        functions->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

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

      OpenGLOffscreenContext& m_context;
      int m_height;
      Recti m_viewportRect;
      bool m_scissorEnabled;
      Recti m_scissorRect;
      std::uint8_t m_colorWriteMask;
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
    if (m_cancelled.load()) {
      clone->cancel();
    }
    return clone;
  }

  void OpenGLRasterizer::render(Buffer<unsigned int>& buffer) {
    RenderEngine::render(buffer);
  }

  void OpenGLRasterizer::render(Buffer<Colord>& buffer) {
    renderOpenGL(buffer, nullptr);
  }

  void OpenGLRasterizer::renderDepth(Buffer<double>& buffer) {
    Buffer<Colord> scratch(buffer.width(), buffer.height());
    renderOpenGL(scratch, &buffer);
  }

  void OpenGLRasterizer::cancel() {
    m_cancelled.store(true);
  }

  void OpenGLRasterizer::uncancel() {
    m_cancelled.store(false);
  }

  std::string OpenGLRasterizer::statusMessage() {
    return "OpenGL raster backend renders the initial material-albedo mesh path when Qt can "
           "create an offscreen context; unsupported hosts report an OpenGL capability error "
           "when the backend is selected";
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
           availability.detail() + ") and can render the initial material-albedo mesh path";
  }

  Recti OpenGLRasterizer::viewportRectFor(int width, int height) const {
    if (m_viewportEnabled) {
      return m_viewportRect;
    }
    return Recti(width, height);
  }

  void OpenGLRasterizer::renderOpenGL(Buffer<Colord>& buffer, Buffer<double>* depthTarget) const {
    OpenGLOffscreenContext context;
    if (!context.create(buffer.width(), buffer.height(), m_msaaSamples)) {
      throw std::runtime_error(context.errorMessage());
    }

    const Recti viewport = viewportRectFor(buffer.width(), buffer.height());
    detail::OpenGLRasterMesh mesh;
    if (viewport.width() > 0 && viewport.height() > 0) {
      mesh = detail::OpenGLRasterMeshBuilder(scene().get(), camera(), m_lod, viewport, m_cullMode,
                                             m_hasCullModeOverride, m_cancelled)
               .build();
    }

    OpenGLRasterDrawPass(context, buffer.height(), viewport, m_scissorTestEnabled, m_scissorRect,
                         m_colorWriteMask)
      .render(mesh, backgroundColor(), buffer, depthTarget);
  }
}
