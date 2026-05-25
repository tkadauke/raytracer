#pragma once

#include "core/Buffer.h"

#include <cassert>
#include <memory>

namespace core::util {

  template<class T>
  bool bufferDimensionsMatch(const Buffer<T>* buffer, int width, int height) {
    return buffer && buffer->width() == width && buffer->height() == height;
  }

  template<class T>
  bool bufferDimensionsMatch(const std::unique_ptr<Buffer<T>>& buffer, int width, int height) {
    return bufferDimensionsMatch(buffer.get(), width, height);
  }

  template<class T, class U>
  bool bufferDimensionsEqual(const Buffer<T>& left, const Buffer<U>& right) {
    return left.width() == right.width() && left.height() == right.height();
  }

  template<class T>
  void copyBuffer(Buffer<T>& target, const Buffer<T>& source) {
    assert(bufferDimensionsEqual(target, source));
    for (int y = 0; y != source.height(); ++y)
      for (int x = 0; x != source.width(); ++x)
        target[y][x] = source[y][x];
  }

}
