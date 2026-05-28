#pragma once

#include <memory>
#include <string>

namespace render {
  class Camera;
  class RenderEngine;
  class Scene;
}

namespace engine::raster {
  /**
    * Selects the concrete backend used for graph-backed raster passes.
    *
    * The CPU software rasterizer remains the default and reference
    * implementation. OpenGL is an opt-in backend used by the render graph when
    * the compiled pass state asks for it.
    */
  class RasterBackend {
  public:
    enum class Kind { CPU, OpenGL };

    RasterBackend();

    static RasterBackend cpu();
    static RasterBackend openGL();
    static RasterBackend fromString(std::string value, const std::string& path = "rasterBackend");

    Kind kind() const;
    const char* id() const;
    const char* displayName() const;
    bool isCPU() const;
    bool isOpenGL() const;
    bool usesSoftwareRasterizer() const;

    std::shared_ptr<render::RenderEngine> createEngine(std::shared_ptr<render::Camera> camera,
                                                       std::shared_ptr<render::Scene> scene) const;

    bool operator==(const RasterBackend& other) const;
    bool operator!=(const RasterBackend& other) const;

  private:
    explicit RasterBackend(Kind kind);
    static std::string normalizedName(std::string value);

    Kind m_kind{Kind::CPU};
  };
}
