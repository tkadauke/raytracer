#ifndef SHAPE_RECOGNITION_H
#define SHAPE_RECOGNITION_H

#include <vector>
#include "core/Buffer.h"

namespace testing {
  class ShapeRecognition {
  public:
    bool recognizeRect(const Buffer<unsigned int>& buffer) const;
    bool recognizeCircle(const Buffer<unsigned int>& buffer) const;
  
  private:
    std::vector<int> lines(const Buffer<unsigned int>& buffer) const;
  };
}

#endif
