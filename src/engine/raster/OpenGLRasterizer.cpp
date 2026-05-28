#include "engine/raster/OpenGLRasterizer.h"

#include "core/Buffer.h"

#include <stdexcept>
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
    if (m_cancelled.load()) {
      clone->cancel();
    }
    return clone;
  }

  void OpenGLRasterizer::render(Buffer<unsigned int>&) {
    throwUnavailable();
  }

  void OpenGLRasterizer::render(Buffer<Colord>&) {
    throwUnavailable();
  }

  void OpenGLRasterizer::cancel() {
    m_cancelled.store(true);
  }

  void OpenGLRasterizer::uncancel() {
    m_cancelled.store(false);
  }

  bool OpenGLRasterizer::isAvailable() const {
    return false;
  }

  std::string OpenGLRasterizer::availabilityError() const {
    return "OpenGL raster backend is selected, but the OpenGL executor shell does not yet "
           "create a context, framebuffer, shader program, or readback path; use "
           "--raster_backend cpu until the first GPU raster pass lands";
  }

  void OpenGLRasterizer::throwUnavailable() const {
    throw std::runtime_error(availabilityError());
  }
}
