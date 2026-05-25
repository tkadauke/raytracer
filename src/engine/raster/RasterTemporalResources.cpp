#include "engine/raster/detail/RasterTemporalResources.h"

#include <cmath>

namespace engine::raster::detail {

  bool temporalJitterIsFinite(const TemporalJitter& jitter) {
    return std::isfinite(jitter.x) && std::isfinite(jitter.y);
  }

  TemporalResourceValidation
  validateTemporalResourceContract(const TemporalResourceContract& contract, int width,
                                   int height) {
    TemporalResourceValidation result;

    if (width <= 0 || height <= 0) {
      result.errors.push_back("render target dimensions must be positive");
    } else {
      validateTemporalBuffer("history color", contract.historyColor, width, height, result.errors);
      validateTemporalBuffer("next history color", contract.nextHistoryColor, width, height,
                             result.errors);
      validateTemporalBuffer("current depth", contract.currentDepth, width, height, result.errors);
      validateTemporalBuffer("history depth", contract.historyDepth, width, height, result.errors);
      validateTemporalBuffer("motion vector", contract.motionVectors, width, height, result.errors);
    }

    if (!temporalJitterIsFinite(contract.currentJitter))
      result.errors.push_back("current jitter must be finite");
    if (!temporalJitterIsFinite(contract.previousJitter))
      result.errors.push_back("previous jitter must be finite");

    result.complete = result.errors.empty();
    result.canAccumulate =
      result.complete && contract.resetCondition == TemporalResetCondition::None;
    return result;
  }

}
