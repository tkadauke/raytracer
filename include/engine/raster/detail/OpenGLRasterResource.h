#pragma once

#include "engine/graph/RenderGraphTypes.h"
#include "engine/raster/gl/Bindings.h"

#include <memory>
#include <string>

namespace engine::raster::gl {
  class Context;
}

namespace engine::raster::detail {
  /**
    * Graph-visible OpenGL resident raster resource.
    *
    * The resource owns one texture or renderbuffer handle created in
    * `sourceContext()`. Destruction makes that context current before issuing
    * the matching `glDelete*` call; when the context cannot be made current,
    * the handle is abandoned with a deterministic diagnostic instead of
    * deleting against an arbitrary current context.
    */
  class OpenGLRasterResource {
  public:
    enum class HandleKind { Texture, Renderbuffer };

    OpenGLRasterResource(engine::graph::RenderResourceType resourceType, HandleKind handleKind,
                         GLuint handle, int width, int height, int sampleCount,
                         std::shared_ptr<gl::Context> sourceContext);
    ~OpenGLRasterResource();

    OpenGLRasterResource(const OpenGLRasterResource&) = delete;
    OpenGLRasterResource& operator=(const OpenGLRasterResource&) = delete;

    OpenGLRasterResource(OpenGLRasterResource&& other) noexcept;
    OpenGLRasterResource& operator=(OpenGLRasterResource&& other) noexcept;

    engine::graph::RenderResourceType resourceType() const {
      return m_resourceType;
    }

    HandleKind handleKind() const {
      return m_handleKind;
    }

    GLuint handle() const {
      return m_handle;
    }

    int width() const {
      return m_width;
    }

    int height() const {
      return m_height;
    }

    int sampleCount() const {
      return m_sampleCount;
    }

    const std::shared_ptr<gl::Context>& sourceContext() const {
      return m_sourceContext;
    }

    const void* sourceContextIdentity() const {
      return m_sourceContext.get();
    }

    bool valid() const {
      return m_handle != 0;
    }

    std::string description() const;

    /**
      * Releases the GL handle now. Returns false when release had to abandon
      * the handle because the source context could not be made current.
      */
    bool release();

    const std::string& releaseDiagnostic() const {
      return m_releaseDiagnostic;
    }

  private:
    void abandonWithDiagnostic(std::string diagnostic);
    void moveFrom(OpenGLRasterResource&& other) noexcept;

    engine::graph::RenderResourceType m_resourceType{engine::graph::RenderResourceType::Color};
    HandleKind m_handleKind{HandleKind::Texture};
    GLuint m_handle{0};
    int m_width{0};
    int m_height{0};
    int m_sampleCount{1};
    std::shared_ptr<gl::Context> m_sourceContext;
    std::string m_releaseDiagnostic;
  };
}
