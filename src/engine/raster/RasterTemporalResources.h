#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Vector.h"

#include <cmath>
#include <string>
#include <vector>

namespace engine::raster::detail {

  struct TemporalJitter {
    double x = 0.0;
    double y = 0.0;
  };

  enum class TemporalResetCondition {
    None,
    FirstFrame,
    CameraCut,
    SceneDiscontinuity,
    ResourceResize,
    HistoryInvalidated
  };

  struct TemporalResourceContract {
    const Buffer<Colord>* historyColor = nullptr;
    Buffer<Colord>* nextHistoryColor = nullptr;
    const Buffer<double>* currentDepth = nullptr;
    const Buffer<double>* historyDepth = nullptr;
    const Buffer<Vector2d>* motionVectors = nullptr;
    TemporalJitter currentJitter;
    TemporalJitter previousJitter;
    TemporalResetCondition resetCondition = TemporalResetCondition::None;
  };

  struct TemporalResourceValidation {
    bool complete = false;
    bool canAccumulate = false;
    std::vector<std::string> errors;
  };

  template<class T>
  bool temporalBufferMatches(const Buffer<T>* buffer, int width, int height) {
    return buffer && buffer->width() == width && buffer->height() == height;
  }

  inline bool temporalJitterIsFinite(const TemporalJitter& jitter) {
    return std::isfinite(jitter.x) && std::isfinite(jitter.y);
  }

  template<class T>
  void validateTemporalBuffer(const char* name, const Buffer<T>* buffer, int width, int height,
                              std::vector<std::string>& errors) {
    if (!buffer) {
      errors.push_back(std::string(name) + " buffer is required");
    } else if (buffer->width() != width || buffer->height() != height) {
      errors.push_back(std::string(name) + " buffer dimensions must match the render target");
    }
  }

  inline TemporalResourceValidation
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
