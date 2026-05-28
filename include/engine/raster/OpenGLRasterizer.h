#pragma once

#include "render/RenderEngine.h"

#include <atomic>
#include <memory>
#include <string>

namespace engine::raster {
  /**
    * OpenGL-backed raster executor shell.
    *
    * The class is wired into graph backend selection before the real GL context,
    * FBO, shader, and readback path exist. Selecting it gives callers one
    * deterministic backend/capability error instead of silently falling back to
    * the CPU rasterizer or bypassing the render graph.
    */
  class OpenGLRasterizer : public render::RenderEngine {
  public:
    explicit OpenGLRasterizer(std::shared_ptr<render::Scene> scene);
    OpenGLRasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene);
    ~OpenGLRasterizer() override;

    std::shared_ptr<render::RenderEngine> cloneForRender() const override;
    void render(Buffer<unsigned int>& buffer) override;
    void render(Buffer<Colord>& buffer) override;
    void cancel() override;
    void uncancel() override;

    bool isAvailable() const;
    std::string availabilityError() const;

  private:
    [[noreturn]] void throwUnavailable() const;

    std::atomic<bool> m_cancelled{false};
  };
}
