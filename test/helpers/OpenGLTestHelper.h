#ifndef OPENGL_TEST_HELPER_H
#define OPENGL_TEST_HELPER_H

#include <gtest/gtest.h>

#include "engine/raster/OpenGLOffscreenContext.h"

// Skips the calling test when no OpenGL offscreen context is available on
// this host. Must be invoked directly in a TEST body (GTEST_SKIP() performs
// a `return`), so this is a macro rather than a helper function.
#define SKIP_IF_NO_OPENGL()                                                     \
  do {                                                                          \
    if (!engine::raster::OpenGLOffscreenContext::probe().available()) {        \
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";      \
    }                                                                           \
  } while (0)

#endif
