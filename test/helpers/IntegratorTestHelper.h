#pragma once

#include "core/Color.h"
#include "render/Integrator.h"

#include <cstdint>
#include <vector>

namespace test::helpers {

  class RecordingBatchObserver final : public render::IntegratorBatchObserver {
  public:
    render::IntegratorBatchFeedback depthCompleted(std::uint64_t completedDepth,
                                                   const std::vector<Colord>& sampleColors,
                                                   std::uint64_t activeSamples) override {
      completedDepths.push_back(completedDepth);
      snapshots.push_back(sampleColors);
      activeSampleCounts.push_back(activeSamples);
      return feedback;
    }

    render::IntegratorBatchFeedback feedback;
    std::vector<std::uint64_t> completedDepths;
    std::vector<std::vector<Colord>> snapshots;
    std::vector<std::uint64_t> activeSampleCounts;
  };
}
