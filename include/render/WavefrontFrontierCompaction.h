#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace render {
  struct IntegratorBatchMetrics;

  class WavefrontFrontierCompactionRequest {
  public:
    explicit WavefrontFrontierCompactionRequest(std::size_t inputPathCount = 0);

    void retain(std::size_t pathIndex);

    [[nodiscard]] std::size_t inputPathCount() const;
    [[nodiscard]] const std::vector<std::size_t>& retainedPathIndices() const;

  private:
    std::size_t m_inputPathCount{0};
    std::vector<std::size_t> m_retainedPathIndices;
  };

  class WavefrontFrontierCompactionResult {
  public:
    static WavefrontFrontierCompactionResult
    hostCompaction(const WavefrontFrontierCompactionRequest& request);
    static WavefrontFrontierCompactionResult
    fromRetainedPathIndices(std::size_t inputPathCount,
                            std::vector<std::size_t> retainedPathIndices,
                            std::string executionPath);

    [[nodiscard]] std::size_t inputPathCount() const;
    [[nodiscard]] std::size_t retainedPathCount() const;
    [[nodiscard]] std::size_t removedPathCount() const;
    [[nodiscard]] std::size_t movedPathCount() const;
    [[nodiscard]] const std::vector<std::size_t>& retainedPathIndices() const;
    [[nodiscard]] const std::string& executionPath() const;

    void record(IntegratorBatchMetrics* metrics) const;

  private:
    WavefrontFrontierCompactionResult(std::size_t inputPathCount,
                                      std::vector<std::size_t> retainedPathIndices,
                                      std::size_t movedPathCount, std::string executionPath);

    static void validateRetainedPathIndices(std::size_t inputPathCount,
                                            const std::vector<std::size_t>& retainedPathIndices);
    static std::size_t movedPathCountFor(const std::vector<std::size_t>& retainedPathIndices);

    std::size_t m_inputPathCount{0};
    std::vector<std::size_t> m_retainedPathIndices;
    std::size_t m_movedPathCount{0};
    std::string m_executionPath;
  };
}
