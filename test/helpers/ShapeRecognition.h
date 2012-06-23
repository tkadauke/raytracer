#ifndef SHAPE_RECOGNITION_H
#define SHAPE_RECOGNITION_H

#include <vector>
#include "Buffer.h"

namespace testing {
  class ShapeRecognition {
  public:
    bool recognizeRect(const Buffer<unsigned int>& buffer);
    bool recognizeCircle(const Buffer<unsigned int>& buffer);
  
  private:
    std::vector<int> lines(const Buffer<unsigned int>& buffer);
  };
}

#endif
