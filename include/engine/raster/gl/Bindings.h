#pragma once

// Pulls in the platform's OpenGL headers so the engine::raster::gl::
// wrappers (Buffer, ShaderProgram, Framebuffer) can call raw `gl*`
// symbols without going through Qt's QOpenGLFunctions adapter. The
// engine library's link line already pulls in OpenGL.framework on
// macOS (for CGLContext) and libGL through Qt::OpenGL on Linux; this
// header just chooses the right include per platform so callers don't
// open-code the same `#if defined(__APPLE__)` block in every wrapper.

#if defined(__APPLE__)
// macOS deprecated everything in OpenGL/gl.h in 10.14; we silence
// the warnings the same way CGLContext.cpp does. The project targets
// GL 2.1, which is well within the deprecated-but-still-shipping
// surface.
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#elif defined(__linux__)
// Mesa's <GL/gl.h> declares OpenGL 1.2 entry points unconditionally
// but gates 2.0+ symbols (glUniformMatrix4fv, glGenBuffers,
// glCompileShader, …) on `GL_GLEXT_PROTOTYPES`. The Mesa libGL.so
// exports all of these symbols, so defining the macro before including
// the headers gives us the entire desktop GL surface without needing
// a function-pointer loader.
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#else
#error "gl::Bindings has no OpenGL header mapping for this platform"
#endif
