#pragma once

#include "render/RenderEngine.h"

#include <atomic>
#include <memory>
#include <string>

namespace engine::raster {
  /**
    * OpenGL-backed raster executor shell.
    *
    * The class is wired into graph backend selection and owns the first
    * offscreen context/FBO capability path plus the first reusable
    * mesh-preparation path. Selecting it gives callers one deterministic
    * backend/capability error until GPU buffer upload, shader execution, and
    * readback exist.
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

    static std::string statusMessage();

    int lod() const;
    void setLod(int lod);

    bool isAvailable() const;
    std::string availabilityDetail() const;
    std::string availabilityError() const;

  private:
    [[noreturn]] void throwRenderUnavailable(const Buffer<Colord>* buffer) const;
    [[noreturn]] void throwRenderUnavailable(const Buffer<unsigned int>* buffer) const;
    [[noreturn]] void throwRenderUnavailable(int width, int height) const;

    std::atomic<bool> m_cancelled{false};
    int m_lod{0};
  };
}
