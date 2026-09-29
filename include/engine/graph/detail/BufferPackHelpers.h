#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/util/BufferUtils.h"
#include "render/tonemap/Tonemap.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace engine::graph::detail {

  // Shared by GraphRenderEngine.cpp and RenderPassPayloads.cpp: throws with
  // @p message if the two buffers don't have matching dimensions.
  template<class Source, class Destination>
  inline void requireMatchingBufferSize(const Buffer<Source>& source, const Buffer<Destination>& destination,
                                        const std::string& message) {
    if (!core::util::bufferDimensionsEqual(source, destination))
      throw std::runtime_error(message);
  }

  // Shared by GraphRenderEngine.cpp and RenderPassPayloads.cpp: copies a
  // Colord source buffer into a packed-pixel destination buffer, applying
  // @p tonemap per pixel when present. Throws @p sizeMismatchMessage if the
  // buffers don't have matching dimensions.
  template<class Destination>
  inline void packColorBuffer(const Buffer<Colord>& source, Buffer<Destination>& destination,
                              const std::shared_ptr<render::Tonemap>& tonemap,
                              const std::string& sizeMismatchMessage) {
    requireMatchingBufferSize(source, destination, sizeMismatchMessage);
    for (int y = 0; y != source.height(); ++y) {
      for (int x = 0; x != source.width(); ++x) {
        destination[y][x] = (tonemap ? tonemap->apply(source[y][x]) : source[y][x]).rgb();
      }
    }
  }

}
