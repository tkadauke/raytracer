#include <gtest/gtest.h>

#include "engine/raster/detail/OpenGLRasterResource.h"
#include "engine/raster/gl/Context.h"
#include "engine/raster/gl/createContext.h"

#include <memory>
#include <string>

namespace engine::raster::detail::tests {
  namespace {
    class FailingContext : public gl::Context {
    public:
      bool create(int = 1) override {
        return true;
      }

      bool isValid() const override {
        return true;
      }

      bool migrateToCurrentThread() override {
        ++migrateCalls;
        return true;
      }

      void detachFromCurrentThread() override {
      }

      bool makeCurrent() override {
        ++makeCurrentCalls;
        m_errorMessage = "fake context refused makeCurrent";
        return false;
      }

      void doneCurrent() override {
        ++doneCurrentCalls;
      }

      const std::string& errorMessage() const override {
        return m_errorMessage;
      }

      std::string detailText() const override {
        return "failing test context";
      }

      int migrateCalls{0};
      int makeCurrentCalls{0};
      int doneCurrentCalls{0};

    private:
      std::string m_errorMessage;
    };
  }

  TEST(OpenGLRasterResource, AbandonsHandleWithDiagnosticWhenContextCannotBeMadeCurrent) {
    auto context = std::make_shared<FailingContext>();
    OpenGLRasterResource resource(engine::graph::RenderResourceType::Color,
                                  OpenGLRasterResource::HandleKind::Texture, 17, 4, 3, 1, context);

    EXPECT_FALSE(resource.release());

    EXPECT_FALSE(resource.valid());
    EXPECT_EQ(1, context->migrateCalls);
    EXPECT_EQ(1, context->makeCurrentCalls);
    EXPECT_EQ(0, context->doneCurrentCalls);
    EXPECT_NE(std::string::npos, resource.releaseDiagnostic().find("fake context refused"));
  }

  TEST(OpenGLRasterResource, DeletesRenderbufferWithOwningContextCurrent) {
    auto contextOwner = gl::createOffscreenContext();
    std::shared_ptr<gl::Context> context(std::move(contextOwner));
    if (!context->create()) {
      GTEST_SKIP() << "GL context unavailable on this host: " << context->errorMessage();
    }
    ASSERT_TRUE(context->makeCurrent()) << context->errorMessage();

    GLuint renderbuffer = 0;
    glGenRenderbuffers(1, &renderbuffer);
    ASSERT_NE(0u, renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
    EXPECT_TRUE(glIsRenderbuffer(renderbuffer));
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    context->doneCurrent();

    OpenGLRasterResource resource(engine::graph::RenderResourceType::Color,
                                  OpenGLRasterResource::HandleKind::Renderbuffer, renderbuffer, 1,
                                  1, 1, context);

    EXPECT_TRUE(resource.release());
    EXPECT_FALSE(resource.valid());
    EXPECT_TRUE(resource.releaseDiagnostic().empty());

    ASSERT_TRUE(context->makeCurrent()) << context->errorMessage();
    EXPECT_FALSE(glIsRenderbuffer(renderbuffer));
    context->doneCurrent();
  }
}
