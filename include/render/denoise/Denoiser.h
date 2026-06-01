#pragma once

#include "core/Color.h"

#include <memory>

template<class T>
class Buffer;

namespace render {
  class Denoiser {
  public:
    virtual ~Denoiser() = default;

    virtual std::unique_ptr<Denoiser> clone() const = 0;
    virtual const char* diagnosticName() const = 0;
    virtual void denoise(Buffer<Colord>& buffer) const = 0;
  };
}
