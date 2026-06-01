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
    double sampleWeight(const DenoiserFrame& frame, const Buffer<Colord>& source, int centerX,
                        int centerY, int sampleX, int sampleY, double spatialSigma) const;
    double albedoWeight(const DenoiserFrame& frame, int centerX, int centerY, int sampleX,
                        int sampleY) const;
    double normalWeight(const DenoiserFrame& frame, int centerX, int centerY, int sampleX,
                        int sampleY) const;
    double depthWeight(const DenoiserFrame& frame, int centerX, int centerY, int sampleX,
                       int sampleY) const;
    double colorDistanceSquared(const Colord& a, const Colord& b) const;
    double gaussian(double distanceSquared, double sigma) const;
    bool hasFeatureDimensions(const Buffer<Colord>* buffer, const DenoiserFrame& frame) const;
    bool hasFeatureDimensions(const Buffer<Vector3d>* buffer, const DenoiserFrame& frame) const;
    bool hasFeatureDimensions(const Buffer<double>* buffer, const DenoiserFrame& frame) const;

    int m_radius;
    double m_colorSigma;
  };
}
