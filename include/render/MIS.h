#pragma once

#include "core/Color.h"

#include <algorithm>

namespace render::mis {
  enum class Heuristic { Balance, Power };

  [[nodiscard]] inline double positivePdf(double pdf) {
    return std::max(0.0, pdf);
  }

  [[nodiscard]] inline double balanceHeuristic(int sampledCount, double sampledPdf, int otherCount,
                                               double otherPdf) {
    const double sampled = std::max(0, sampledCount) * positivePdf(sampledPdf);
    const double other = std::max(0, otherCount) * positivePdf(otherPdf);
    const double denominator = sampled + other;
    if (denominator == 0.0)
      return 0.0;

    return sampled / denominator;
  }

  [[nodiscard]] inline double balanceHeuristic(double sampledPdf, double otherPdf) {
    return balanceHeuristic(1, sampledPdf, 1, otherPdf);
  }

  [[nodiscard]] inline double powerHeuristic(int sampledCount, double sampledPdf, int otherCount,
                                             double otherPdf) {
    const double sampled = std::max(0, sampledCount) * positivePdf(sampledPdf);
    const double other = std::max(0, otherCount) * positivePdf(otherPdf);
    const double sampledSquared = sampled * sampled;
    const double otherSquared = other * other;
    const double denominator = sampledSquared + otherSquared;
    if (denominator == 0.0)
      return 0.0;

    return sampledSquared / denominator;
  }

  [[nodiscard]] inline double powerHeuristic(double sampledPdf, double otherPdf) {
    return powerHeuristic(1, sampledPdf, 1, otherPdf);
  }

  [[nodiscard]] inline double weight(Heuristic heuristic, double sampledPdf, double otherPdf) {
    switch (heuristic) {
    case Heuristic::Balance:
      return balanceHeuristic(sampledPdf, otherPdf);
    case Heuristic::Power:
      return powerHeuristic(sampledPdf, otherPdf);
    }

    return 0.0;
  }

  [[nodiscard]] inline Colord directLightingEstimate(const Colord& bsdfValue,
                                                     const Colord& lightRadiance,
                                                     double normalDotLight, double sampledPdf,
                                                     double otherPdf, bool sampledDelta = false,
                                                     Heuristic heuristic = Heuristic::Power) {
    if (normalDotLight <= 0.0)
      return Colord::black();

    const double pdf = positivePdf(sampledPdf);
    if (pdf == 0.0)
      return Colord::black();

    const double misWeight = sampledDelta ? 1.0 : weight(heuristic, pdf, otherPdf);
    return bsdfValue * lightRadiance * (normalDotLight * misWeight / pdf);
  }

  [[nodiscard]] inline Colord estimateDirectLightingFromLightSample(
    const Colord& bsdfValue, const Colord& lightRadiance, double normalDotLight, double lightPdf,
    double bsdfPdf, bool lightIsDelta = false, Heuristic heuristic = Heuristic::Power) {
    return directLightingEstimate(bsdfValue, lightRadiance, normalDotLight, lightPdf, bsdfPdf,
                                  lightIsDelta, heuristic);
  }

  [[nodiscard]] inline Colord estimateDirectLightingFromBsdfSample(
    const Colord& bsdfValue, const Colord& lightRadiance, double normalDotLight, double bsdfPdf,
    double lightPdf, bool bsdfIsDelta = false, Heuristic heuristic = Heuristic::Power) {
    return directLightingEstimate(bsdfValue, lightRadiance, normalDotLight, bsdfPdf, lightPdf,
                                  bsdfIsDelta, heuristic);
  }
}
