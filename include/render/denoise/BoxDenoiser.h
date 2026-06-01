#pragma once

#include "render/denoise/Denoiser.h"

namespace render {
  class BoxDenoiser : public Denoiser {
  public:
    explicit BoxDenoiser(int radius = 1);

    std::unique_ptr<Denoiser> clone() const override;
    const char* diagnosticName() const override;
    DenoiserDiagnostics diagnostics() const override;
    void denoiseFrame(DenoiserFrame& frame) const override;

    void setRadius(int radius);
    int radius() const;

  private:
    int m_radius;
  };
}
