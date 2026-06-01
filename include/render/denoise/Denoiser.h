#pragma once

#include "core/Color.h"

#include <memory>
#include <string>
#include <vector>

template<class T>
class Buffer;

namespace render {
  struct DenoiserDiagnostics {
    struct NumericParameter {
      std::string name;
      double value = 0.0;
    };

    std::string name;
    std::vector<NumericParameter> numericParameters;
  };

  class Denoiser {
  public:
    virtual ~Denoiser() = default;

    virtual std::unique_ptr<Denoiser> clone() const = 0;
    virtual const char* diagnosticName() const = 0;
    virtual DenoiserDiagnostics diagnostics() const {
      return DenoiserDiagnostics{diagnosticName(), {}};
    }
    virtual void denoise(Buffer<Colord>& buffer) const = 0;
  };
}
