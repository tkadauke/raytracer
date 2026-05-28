#include "engine/raster/OpenGLRasterizer.h"

#include "core/Buffer.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/detail/OpenGLRasterMesh.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace engine::raster {
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
    throwRenderUnavailable(&buffer);
  }

  void OpenGLRasterizer::render(Buffer<Colord>& buffer) {
    throwRenderUnavailable(&buffer);
  }

  void OpenGLRasterizer::cancel() {
    m_cancelled.store(true);
  }

  void OpenGLRasterizer::uncancel() {
    m_cancelled.store(false);
  }

  std::string OpenGLRasterizer::statusMessage() {
    const OpenGLAvailability availability = OpenGLOffscreenContext::probe();
    if (!availability.available()) {
      return availability.error();
    }
    return "OpenGL raster backend is selected and an offscreen context is available (" +
           availability.detail() +
           "), but GPU buffer upload, shader execution, and readback are not implemented yet; use "
           "--raster_backend cpu until the first GPU raster pass lands";
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
    return statusMessage();
  }

  void OpenGLRasterizer::throwRenderUnavailable(const Buffer<Colord>* buffer) const {
    throwRenderUnavailable(buffer ? buffer->width() : 1, buffer ? buffer->height() : 1);
  }

  void OpenGLRasterizer::throwRenderUnavailable(const Buffer<unsigned int>* buffer) const {
    throwRenderUnavailable(buffer ? buffer->width() : 1, buffer ? buffer->height() : 1);
  }

  void OpenGLRasterizer::throwRenderUnavailable(int width, int height) const {
    OpenGLOffscreenContext context;
    if (!context.create(width, height)) {
      throw std::runtime_error(context.errorMessage());
    }

    const detail::OpenGLRasterMesh mesh =
      detail::OpenGLRasterMeshBuilder(scene().get(), camera(), m_lod, Recti(width, height),
                                      m_cancelled)
        .build();

    throw std::runtime_error(
      "OpenGL raster backend is selected and created an offscreen " + context.detailText() +
      " context/framebuffer, and prepared " + std::to_string(mesh.triangleCount()) +
      " screen-space triangles, but GPU buffer upload, shader execution, and readback are not "
      "implemented yet; use --raster_backend cpu until the first GPU raster pass lands");
  }
}
