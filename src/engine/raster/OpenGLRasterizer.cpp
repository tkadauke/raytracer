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
      OpenGLRasterDrawPass(OpenGLOffscreenContext& context, int width, int height)
          : m_context(context),
            m_width(width),
            m_height(height) {
      }

      void render(const detail::OpenGLRasterMesh& mesh, const Colord& background,
                  Buffer<Colord>& target) {
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
        functions->glViewport(0, 0, m_width, m_height);
        functions->glEnable(GL_DEPTH_TEST);
        functions->glDepthFunc(GL_LESS);
        functions->glClearColor(static_cast<GLfloat>(std::clamp(background.r(), 0.0, 1.0)),
                                static_cast<GLfloat>(std::clamp(background.g(), 0.0, 1.0)),
                                static_cast<GLfloat>(std::clamp(background.b(), 0.0, 1.0)), 1.0f);
        functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        if (mesh.empty()) {
          functions->glFlush();
          return;
        }

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

        program.disableAttributeArray(colorLocation);
        program.disableAttributeArray(positionLocation);
        indexBuffer.release();
        vertexBuffer.release();
        program.release();
      }

      OpenGLOffscreenContext& m_context;
      int m_width;
      int m_height;
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
    if (m_cancelled.load()) {
      clone->cancel();
    }
    return clone;
  }

  void OpenGLRasterizer::render(Buffer<unsigned int>& buffer) {
    RenderEngine::render(buffer);
  }

  void OpenGLRasterizer::render(Buffer<Colord>& buffer) {
    renderOpenGL(buffer);
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

  void OpenGLRasterizer::renderOpenGL(Buffer<Colord>& buffer) const {
    OpenGLOffscreenContext context;
    if (!context.create(buffer.width(), buffer.height())) {
      throw std::runtime_error(context.errorMessage());
    }

    const detail::OpenGLRasterMesh mesh =
      detail::OpenGLRasterMeshBuilder(scene().get(), camera(), m_lod,
                                      Recti(buffer.width(), buffer.height()), m_cancelled)
        .build();

    OpenGLRasterDrawPass(context, buffer.width(), buffer.height())
      .render(mesh, backgroundColor(), buffer);
  }
}
