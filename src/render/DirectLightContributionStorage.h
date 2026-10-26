#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/Color.h"
#include "render/Integrator.h"

namespace render::detail {
  /// Per-selection direct-light contribution storage shared by the batch
  /// integrators. Each integrator derives its own `DirectLightContributionBatch`
  /// from this and adds the mutators that are specific to its algorithm.
  class DirectLightContributionStorage {
  public:
    explicit DirectLightContributionStorage(std::size_t count)
        : m_contributions(count, Colord::black()) {
    }

    DirectLightContributionStorage(std::size_t count, int depth, IntegratorBatchMetrics* metrics)
        : DirectLightContributionStorage(count) {
      recordHostBytes(depth, metrics);
    }

    [[nodiscard]] std::size_t size() const {
      return m_contributions.size();
    }

    [[nodiscard]] bool empty() const {
      return m_contributions.empty();
    }

    [[nodiscard]] std::uint64_t hostBytes() const {
      return static_cast<std::uint64_t>(m_contributions.size()) * sizeof(Colord);
    }

    [[nodiscard]] Colord at(std::size_t index) const {
      return index < m_contributions.size() ? m_contributions[index] : Colord::black();
    }

  protected:
    void recordHostBytes(int depth, IntegratorBatchMetrics* metrics) const {
      if (!metrics) {
        return;
      }
      metrics->recordDirectLightContributionHostBytes(
        static_cast<std::uint64_t>(std::max(0, depth)), hostBytes());
    }

    std::vector<Colord> m_contributions;
  };
}
