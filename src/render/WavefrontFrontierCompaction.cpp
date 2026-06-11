#include "render/WavefrontFrontierCompaction.h"

#include "render/Integrator.h"

#include <stdexcept>
#include <utility>

namespace render {
  WavefrontFrontierCompactionRequest::WavefrontFrontierCompactionRequest(std::size_t inputPathCount)
      : m_inputPathCount(inputPathCount) {
  }

  void WavefrontFrontierCompactionRequest::retain(std::size_t pathIndex) {
    if (pathIndex >= m_inputPathCount) {
      throw std::out_of_range("frontier compaction retained path index is out of range");
    }
    m_retainedPathIndices.push_back(pathIndex);
  }

  std::size_t WavefrontFrontierCompactionRequest::inputPathCount() const {
    return m_inputPathCount;
  }

  const std::vector<std::size_t>& WavefrontFrontierCompactionRequest::retainedPathIndices() const {
    return m_retainedPathIndices;
  }

  WavefrontFrontierCompactionResult WavefrontFrontierCompactionResult::hostCompaction(
    const WavefrontFrontierCompactionRequest& request) {
    return fromRetainedPathIndices(request.inputPathCount(), request.retainedPathIndices(), "host");
  }

  WavefrontFrontierCompactionResult WavefrontFrontierCompactionResult::fromRetainedPathIndices(
    std::size_t inputPathCount, std::vector<std::size_t> retainedPathIndices,
    std::string executionPath) {
    validateRetainedPathIndices(inputPathCount, retainedPathIndices);
    const std::size_t movedPathCount = movedPathCountFor(retainedPathIndices);
    return WavefrontFrontierCompactionResult(inputPathCount, std::move(retainedPathIndices),
                                             movedPathCount, std::move(executionPath));
  }

  std::size_t WavefrontFrontierCompactionResult::inputPathCount() const {
    return m_inputPathCount;
  }

  std::size_t WavefrontFrontierCompactionResult::retainedPathCount() const {
    return m_retainedPathIndices.size();
  }

  std::size_t WavefrontFrontierCompactionResult::removedPathCount() const {
    return m_inputPathCount > retainedPathCount() ? m_inputPathCount - retainedPathCount() : 0;
  }

  std::size_t WavefrontFrontierCompactionResult::movedPathCount() const {
    return m_movedPathCount;
  }

  const std::vector<std::size_t>& WavefrontFrontierCompactionResult::retainedPathIndices() const {
    return m_retainedPathIndices;
  }

  const std::string& WavefrontFrontierCompactionResult::executionPath() const {
    return m_executionPath;
  }

  void WavefrontFrontierCompactionResult::record(IntegratorBatchMetrics* metrics) const {
    if (!metrics) {
      return;
    }
    metrics->recordFrontierCompaction(
      static_cast<std::uint64_t>(m_inputPathCount), static_cast<std::uint64_t>(retainedPathCount()),
      static_cast<std::uint64_t>(m_movedPathCount), m_executionPath);
  }

  WavefrontFrontierCompactionResult::WavefrontFrontierCompactionResult(
    std::size_t inputPathCount, std::vector<std::size_t> retainedPathIndices,
    std::size_t movedPathCount, std::string executionPath)
      : m_inputPathCount(inputPathCount),
        m_retainedPathIndices(std::move(retainedPathIndices)),
        m_movedPathCount(movedPathCount),
        m_executionPath(std::move(executionPath)) {
  }

  void WavefrontFrontierCompactionResult::validateRetainedPathIndices(
    std::size_t inputPathCount, const std::vector<std::size_t>& retainedPathIndices) {
    std::size_t previous = 0;
    bool hasPrevious = false;
    for (const std::size_t pathIndex : retainedPathIndices) {
      if (pathIndex >= inputPathCount) {
        throw std::out_of_range("frontier compaction retained path index is out of range");
      }
      if (hasPrevious && pathIndex <= previous) {
        throw std::invalid_argument(
          "frontier compaction retained path indices must be strictly increasing");
      }
      previous = pathIndex;
      hasPrevious = true;
    }
  }

  std::size_t WavefrontFrontierCompactionResult::movedPathCountFor(
    const std::vector<std::size_t>& retainedPathIndices) {
    std::size_t movedPathCount = 0;
    for (std::size_t outputIndex = 0; outputIndex != retainedPathIndices.size(); ++outputIndex) {
      if (retainedPathIndices[outputIndex] != outputIndex) {
        ++movedPathCount;
      }
    }
    return movedPathCount;
  }
}
