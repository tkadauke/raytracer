#include "render/samplers/GridSampleAwareSampler.h"

using namespace render;

Vector2d GridSampleAwareSampler::sampleForDimension(int sampleIndex, uint64_t pixelHash,
                                                    uint64_t dimension) const {
  return scrambledPathAwareSample(sampleIndex, pixelHash, dimension);
}
