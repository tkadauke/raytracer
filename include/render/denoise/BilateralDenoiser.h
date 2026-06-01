#pragma once

#include "render/denoise/Denoiser.h"

namespace render {
  class BilateralDenoiser : public Denoiser {
  public:
    explicit BilateralDenoiser(int radius = 2, double colorSigma = 0.1);

    std::unique_ptr<Denoiser> clone() const override;
    const char* diagnosticName() const override;
    DenoiserDiagnostics diagnostics() const override;
    void denoiseFrame(DenoiserFrame& frame) const override;

    void setRadius(int radius);
    int radius() const;

    void setColorSigma(double sigma);
    double colorSigma() const;

  private:
    static double colorDistanceSquared(const Colord& a, const Colord& b);
    static double gaussian(double distanceSquared, double sigma);

    int m_radius;
    double m_colorSigma;
  };
}
