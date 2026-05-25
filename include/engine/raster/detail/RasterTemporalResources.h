#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/util/BufferUtils.h"

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

  bool temporalJitterIsFinite(const TemporalJitter& jitter);

  template<class T>
  void validateTemporalBuffer(const char* name, const Buffer<T>* buffer, int width, int height,
                              std::vector<std::string>& errors) {
    if (!buffer) {
      errors.push_back(std::string(name) + " buffer is required");
    } else if (!core::util::bufferDimensionsMatch(buffer, width, height)) {
      errors.push_back(std::string(name) + " buffer dimensions must match the render target");
    }
  }

  TemporalResourceValidation
  validateTemporalResourceContract(const TemporalResourceContract& contract, int width, int height);

}
