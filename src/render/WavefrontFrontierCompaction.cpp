#include "render/WavefrontFrontierCompaction.h"

#include "render/Integrator.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace render {
  namespace {
    std::uint64_t saturatedByteProduct(std::size_t count, std::uint64_t bytesPerItem) {
      constexpr std::uint64_t maxValue = std::numeric_limits<std::uint64_t>::max();
      if (bytesPerItem != 0 && count > maxValue / bytesPerItem) {
        return maxValue;
      }
      return static_cast<std::uint64_t>(count) * bytesPerItem;
    }

    std::uint64_t retainedIndexBytesFor(std::size_t retainedPathCount) {
      return saturatedByteProduct(retainedPathCount, sizeof(std::uint32_t));
    }
  }

  WavefrontFrontierCompactionRequest::WavefrontFrontierCompactionRequest(std::size_t inputPathCount)
      : m_inputPathCount(inputPathCount) {
  }

  void WavefrontFrontierCompactionRequest::setPathStateBytesPerPath(std::uint64_t bytes) {
    m_pathStateBytesPerPath = bytes;
  }

  void WavefrontFrontierCompactionRequest::setPathStateResidency(std::string residency) {
    m_pathStateResidency = residency.empty() ? "unknown" : std::move(residency);
  }

  void WavefrontFrontierCompactionRequest::retain(std::size_t pathIndex) {
    if (pathIndex >= m_inputPathCount) {
      throw std::out_of_range("frontier compaction retained path index is out of range");
    }
    if (pathIndex > std::numeric_limits<std::uint32_t>::max()) {
      throw std::overflow_error(
        "frontier compaction retained path index exceeds GPU retained-index range");
    }
    if (!m_retainedPathIndices.empty() && pathIndex <= m_retainedPathIndices.back()) {
      throw std::invalid_argument(
        "frontier compaction retained path indices must be strictly increasing");
    }
    m_retainedPathIndices.push_back(static_cast<std::uint32_t>(pathIndex));
  }

  std::size_t WavefrontFrontierCompactionRequest::inputPathCount() const {
    return m_inputPathCount;
  }

  const std::vector<std::uint32_t>&
  WavefrontFrontierCompactionRequest::retainedPathIndices() const {
    return m_retainedPathIndices;
  }

  std::uint64_t WavefrontFrontierCompactionRequest::pathStateBytesPerPath() const {
    return m_pathStateBytesPerPath;
  }

  const std::string& WavefrontFrontierCompactionRequest::pathStateResidency() const {
    return m_pathStateResidency;
  }

  std::uint64_t WavefrontFrontierCompactionRequest::inputPathStateBytes() const {
    return saturatedByteProduct(m_inputPathCount, m_pathStateBytesPerPath);
  }

  std::uint64_t WavefrontFrontierCompactionRequest::retainedPathStateBytes() const {
    return saturatedByteProduct(m_retainedPathIndices.size(), m_pathStateBytesPerPath);
  }

  std::uint64_t WavefrontFrontierCompactionRequest::removedPathStateBytes() const {
    const std::size_t removed = m_inputPathCount > m_retainedPathIndices.size()
                                  ? m_inputPathCount - m_retainedPathIndices.size()
                                  : 0;
    return saturatedByteProduct(removed, m_pathStateBytesPerPath);
  }

  std::uint64_t WavefrontFrontierCompactionRequest::retainedIndexBytes() const {
    return retainedIndexBytesFor(m_retainedPathIndices.size());
  }

  WavefrontFrontierCompactionResult WavefrontFrontierCompactionResult::hostCompaction(
    const WavefrontFrontierCompactionRequest& request) {
    return fromRetainedPathIndices(request.inputPathCount(), request.retainedPathIndices(), "host",
                                   request.pathStateBytesPerPath(), request.pathStateResidency());
  }

  WavefrontFrontierCompactionResult WavefrontFrontierCompactionResult::fromRetainedPathIndices(
    std::size_t inputPathCount, std::vector<std::uint32_t> retainedPathIndices,
    std::string executionPath, std::uint64_t pathStateBytesPerPath,
    std::string pathStateResidency) {
    validateRetainedPathIndices(inputPathCount, retainedPathIndices);
    const std::size_t movedPathCount = movedPathCountFor(retainedPathIndices);
    return WavefrontFrontierCompactionResult(
      inputPathCount, std::move(retainedPathIndices), movedPathCount,
      executionPath.empty() ? "unknown" : std::move(executionPath), pathStateBytesPerPath,
      pathStateResidency.empty() ? "unknown" : std::move(pathStateResidency));
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

  double WavefrontFrontierCompactionResult::removedPathFraction() const {
    if (m_inputPathCount == 0) {
      return 0.0;
    }
    return static_cast<double>(removedPathCount()) / static_cast<double>(m_inputPathCount);
  }

  std::size_t WavefrontFrontierCompactionResult::movedPathCount() const {
    return m_movedPathCount;
  }

  double WavefrontFrontierCompactionResult::movedRetainedPathFraction() const {
    const std::size_t retained = retainedPathCount();
    if (retained == 0) {
      return 0.0;
    }
    return static_cast<double>(m_movedPathCount) / static_cast<double>(retained);
  }

  const std::vector<std::uint32_t>& WavefrontFrontierCompactionResult::retainedPathIndices() const {
    return m_retainedPathIndices;
  }

  std::uint64_t WavefrontFrontierCompactionResult::pathStateBytesPerPath() const {
    return m_pathStateBytesPerPath;
  }

  const std::string& WavefrontFrontierCompactionResult::pathStateResidency() const {
    return m_pathStateResidency;
  }

  std::uint64_t WavefrontFrontierCompactionResult::inputPathStateBytes() const {
    return saturatedByteProduct(m_inputPathCount, m_pathStateBytesPerPath);
  }

  std::uint64_t WavefrontFrontierCompactionResult::retainedPathStateBytes() const {
    return saturatedByteProduct(m_retainedPathIndices.size(), m_pathStateBytesPerPath);
  }

  std::uint64_t WavefrontFrontierCompactionResult::removedPathStateBytes() const {
    return saturatedByteProduct(removedPathCount(), m_pathStateBytesPerPath);
  }

  std::uint64_t WavefrontFrontierCompactionResult::retainedIndexBytes() const {
    return retainedIndexBytesFor(m_retainedPathIndices.size());
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
      static_cast<std::uint64_t>(m_movedPathCount), m_executionPath, retainedIndexBytes(),
      inputPathStateBytes(), retainedPathStateBytes(), removedPathStateBytes(),
      m_pathStateResidency);
  }

  WavefrontFrontierCompactionResult::WavefrontFrontierCompactionResult(
    std::size_t inputPathCount, std::vector<std::uint32_t> retainedPathIndices,
    std::size_t movedPathCount, std::string executionPath, std::uint64_t pathStateBytesPerPath,
    std::string pathStateResidency)
      : m_inputPathCount(inputPathCount),
        m_pathStateBytesPerPath(pathStateBytesPerPath),
        m_retainedPathIndices(std::move(retainedPathIndices)),
        m_movedPathCount(movedPathCount),
        m_executionPath(std::move(executionPath)),
        m_pathStateResidency(std::move(pathStateResidency)) {
  }

  void WavefrontFrontierCompactionResult::validateRetainedPathIndices(
    std::size_t inputPathCount, const std::vector<std::uint32_t>& retainedPathIndices) {
    std::uint32_t previous = 0;
    bool hasPrevious = false;
    for (const std::uint32_t pathIndex : retainedPathIndices) {
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
    const std::vector<std::uint32_t>& retainedPathIndices) {
    std::size_t movedPathCount = 0;
    for (std::size_t outputIndex = 0; outputIndex != retainedPathIndices.size(); ++outputIndex) {
      if (static_cast<std::size_t>(retainedPathIndices[outputIndex]) != outputIndex) {
        ++movedPathCount;
      }
    }
    return movedPathCount;
  }
}
