#pragma once

#include "engine/raster/detail/OpenGLRasterMesh.h"
#include "engine/raster/detail/OpenGLRasterResourceCache.h"
#include "engine/raster/gl/Bindings.h"
#include "engine/raster/gl/ShaderProgram.h"

#include <array>
#include <cstddef>

namespace engine::raster::detail {
  /**
    * Bind the OpenGL raster vertex attribute streams to the shader's
    * attribute slot locations. The 14 attribute streams (position,
    * worldPosition, normal, color, uv, alphaScale, materialDiffuse,
    * materialSpecularColor, materialSpecularCoefficient,
    * materialSpecularExponent, ambientLighting, directLighting,
    * specular, albedoMode) all share the same shape — enable +
    * `setAttributeBuffer(loc, GL_FLOAT, offset, tupleSize, stride)` —
    * so they get described as a small table rather than 14 inlined
    * blocks of repeated boilerplate.
    *
    * The caller must have:
    *  - bound the vertex buffer with `gl::Buffer::bind()` (so the
    *    attribute setup attaches to the right buffer object);
    *  - linked and bound the shader program;
    *  - resolved `OpenGLRasterAttributeLocations` against the program.
    *
    * Pairs with `unbindVertexAttributes` which disables every attribute
    * the bind enabled.
    */

  /// Bind every vertex attribute stream. `out` is the program, `locs`
  /// supplies the per-attribute integer locations resolved at link
  /// time.
  inline void bindVertexAttributes(gl::ShaderProgram& program,
                                   const OpenGLRasterAttributeLocations& locs) {
    using Vertex = OpenGLRasterMesh::Vertex;
    constexpr int stride = static_cast<int>(sizeof(Vertex));

    struct Binding {
      int location;
      int offset;
      int tupleSize;
    };

    const std::array<Binding, 14> bindings{{
      {locs.position, offsetof(Vertex, x), 4},
      {locs.worldPosition, offsetof(Vertex, worldX), 3},
      {locs.normal, offsetof(Vertex, normalX), 3},
      {locs.color, offsetof(Vertex, r), 4},
      {locs.uv, offsetof(Vertex, u), 2},
      {locs.alphaScale, offsetof(Vertex, alphaScale), 1},
      {locs.materialDiffuse, offsetof(Vertex, materialDiffuse), 1},
      {locs.materialSpecularColor, offsetof(Vertex, materialSpecularR), 3},
      {locs.materialSpecularCoefficient, offsetof(Vertex, materialSpecularCoefficient), 1},
      {locs.materialSpecularExponent, offsetof(Vertex, materialSpecularExponent), 1},
      {locs.ambientLighting, offsetof(Vertex, ambientR), 3},
      {locs.directLighting, offsetof(Vertex, directR), 3},
      {locs.specular, offsetof(Vertex, specularR), 3},
      {locs.albedoMode, offsetof(Vertex, albedoMode), 1},
    }};

    for (const auto& b : bindings) {
      program.enableAttributeArray(b.location);
      program.setAttributeBuffer(b.location, GL_FLOAT, b.offset, b.tupleSize, stride);
    }
  }

  /// Disable every vertex attribute stream that `bindVertexAttributes`
  /// enabled. Order is reversed to mirror the original draw-pass
  /// shape; OpenGL doesn't care about the order, but the symmetry
  /// makes diffs across the function pair easier to read.
  inline void unbindVertexAttributes(gl::ShaderProgram& program,
                                     const OpenGLRasterAttributeLocations& locs) {
    program.disableAttributeArray(locs.albedoMode);
    program.disableAttributeArray(locs.specular);
    program.disableAttributeArray(locs.directLighting);
    program.disableAttributeArray(locs.ambientLighting);
    program.disableAttributeArray(locs.materialSpecularExponent);
    program.disableAttributeArray(locs.materialSpecularCoefficient);
    program.disableAttributeArray(locs.materialSpecularColor);
    program.disableAttributeArray(locs.materialDiffuse);
    program.disableAttributeArray(locs.alphaScale);
    program.disableAttributeArray(locs.uv);
    program.disableAttributeArray(locs.color);
    program.disableAttributeArray(locs.normal);
    program.disableAttributeArray(locs.worldPosition);
    program.disableAttributeArray(locs.position);
  }
}
