#include "engine/raster/RasterBackend.h"

#include "engine/raster/OpenGLRasterizer.h"
#include "engine/raster/Rasterizer.h"

#include "core/util/StringUtil.h"

#include <stdexcept>
#include <utility>

namespace engine::raster {
  RasterBackend::RasterBackend() = default;

  RasterBackend::RasterBackend(Kind kind)
      : m_kind(kind) {
  }

  RasterBackend RasterBackend::cpu() {
    return RasterBackend(Kind::CPU);
  }

  RasterBackend RasterBackend::openGL() {
    return RasterBackend(Kind::OpenGL);
  }

  RasterBackend RasterBackend::fromString(std::string value, const std::string& path) {
    value = normalizedName(std::move(value));
    if (value == "cpu" || value == "software") {
      return cpu();
    }
    if (value == "opengl" || value == "gl" || value == "gpu") {
      return openGL();
    }
    throw std::runtime_error("Invalid raster backend at " + path +
                             ": expected cpu, opengl, or gpu");
  }

  RasterBackend::Kind RasterBackend::kind() const {
    return m_kind;
  }

  const char* RasterBackend::id() const {
    switch (m_kind) {
    case Kind::CPU:
      return "cpu";
    case Kind::OpenGL:
      return "opengl";
    }
    return "cpu";
  }

  const char* RasterBackend::displayName() const {
    switch (m_kind) {
    case Kind::CPU:
      return "CPU";
    case Kind::OpenGL:
      return "OpenGL";
    }
    return "CPU";
  }

  bool RasterBackend::isCPU() const {
    return m_kind == Kind::CPU;
  }

  bool RasterBackend::isOpenGL() const {
    return m_kind == Kind::OpenGL;
  }

  bool RasterBackend::usesSoftwareRasterizer() const {
    return isCPU();
  }

  std::shared_ptr<render::RenderEngine>
  RasterBackend::createEngine(std::shared_ptr<render::Camera> camera,
                              std::shared_ptr<render::Scene> scene) const {
    switch (m_kind) {
    case Kind::CPU:
      return std::make_shared<Rasterizer>(std::move(camera), std::move(scene));
    case Kind::OpenGL:
      return std::make_shared<OpenGLRasterizer>(std::move(camera), std::move(scene));
    }
    return std::make_shared<Rasterizer>(std::move(camera), std::move(scene));
  }

  bool RasterBackend::operator==(const RasterBackend& other) const {
    return m_kind == other.m_kind;
  }

  bool RasterBackend::operator!=(const RasterBackend& other) const {
    return !(*this == other);
  }

  std::string RasterBackend::normalizedName(std::string value) {
    return core::util::lowercase(std::move(value));
  }
}
