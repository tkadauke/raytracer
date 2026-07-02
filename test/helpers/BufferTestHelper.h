#ifndef BUFFER_TEST_HELPER_H
#define BUFFER_TEST_HELPER_H

#include "core/Buffer.h"
#include "core/Color.h"

namespace test {
  namespace helpers {
    template<class T>
    int countPixels(const Buffer<T>& buffer, const T& value) {
      int count = 0;
      for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
          if (buffer[y][x] == value) {
            ++count;
          }
        }
      }
      return count;
    }

    template<class T>
    int countPixelsNotEqualTo(const Buffer<T>& buffer, const T& value) {
      return buffer.width() * buffer.height() - countPixels(buffer, value);
    }

    inline int countIntermediatePixels(const Buffer<Colord>& buffer) {
      int count = 0;
      for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
          const double r = buffer[y][x].r();
          if (r > 0.0 && r < 1.0)
            ++count;
        }
      }
      return count;
    }
  }
}

#endif
