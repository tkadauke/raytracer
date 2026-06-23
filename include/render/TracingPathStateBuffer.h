#pragma once

#include "core/Color.h"
#include "core/math/Ray.h"
#include "render/TracingAccumulationLayout.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

template<class T>
class Buffer;

namespace render {
  class Tonemap;

  inline constexpr std::uint32_t gpuPathStateLayoutVersion = 1u;

  enum class TracingPathStateFormat { GpuPathStateRecordV1 };

  enum class GpuPathStateFlags : std::uint32_t {
    None = 0u,
    Active = 1u << 0u,
    BackgroundVisible = 1u << 1u,
    SampledFromBsdf = 1u << 2u,
    BsdfSampleDelta = 1u << 3u
  };

  [[nodiscard]] constexpr std::uint32_t flagValue(GpuPathStateFlags flag) {
    return static_cast<std::uint32_t>(flag);
  }

  struct alignas(16) GpuPathStateRecord {
    std::array<float, 4> origin{};
    std::array<float, 4> direction{};
    std::array<float, 4> throughput{};
    std::array<float, 4> accumulatedRadiance{};
    std::array<float, 4> continuation{};
    std::uint32_t pixelIndex{0};
    std::uint32_t sampleIndex{0};
    std::uint32_t depth{0};
    std::uint32_t flags{0};
    std::uint32_t rngSeed{0};
    std::array<std::uint32_t, 3> reserved{};
  };

  struct TracingPathStateLayout {
    std::uint64_t capacity{0};
    TracingPathStateFormat activeFormat{TracingPathStateFormat::GpuPathStateRecordV1};
    TracingPathStateFormat nextFormat{TracingPathStateFormat::GpuPathStateRecordV1};

    static TracingPathStateLayout pathCapacity(std::uint64_t capacity);

    [[nodiscard]] bool hasActiveAndNextBuffers() const;
    [[nodiscard]] std::uint64_t pathStateBytes() const;
    [[nodiscard]] std::uint64_t activeBytes() const;
    [[nodiscard]] std::uint64_t nextBytes() const;
    [[nodiscard]] std::uint64_t totalBytes() const;

    void validate() const;
  };

  struct TracingPathStateDiagnostics {
    std::string backend;
    std::string residency;
    TracingPathStateLayout layout;
    std::uint64_t residentBytes{0};
    std::uint64_t activeCount{0};
    std::uint64_t nextCount{0};
    std::uint64_t clearOperations{0};
    std::uint64_t appendOperations{0};
    std::uint64_t appendedActiveRecords{0};
    std::uint64_t appendedNextRecords{0};
    std::uint64_t swapOperations{0};

    [[nodiscard]] static TracingPathStateDiagnostics
    forLayout(const TracingPathStateLayout& layout, const char* backend, const char* residency);

    void recordClear();
    void recordAppendActive(std::uint64_t records);
    void recordAppendNext(std::uint64_t records);
    void recordSwap(std::uint64_t newActiveCount);
  };

  class ResidentPathCompactionContract {
  public:
    static ResidentPathCompactionContract
    fromRetainedIndices(std::uint64_t inputPathCount,
                        std::vector<std::uint32_t> retainedPathIndices, std::string executionPath,
                        std::uint64_t pathStateBytesPerPath = sizeof(GpuPathStateRecord));

    [[nodiscard]] std::uint64_t inputPathCount() const;
    [[nodiscard]] std::uint64_t retainedPathCount() const;
    [[nodiscard]] std::uint64_t removedPathCount() const;
    [[nodiscard]] std::uint64_t movedPathCount() const;
    [[nodiscard]] double removedPathFraction() const;
    [[nodiscard]] double movedRetainedPathFraction() const;
    [[nodiscard]] const std::vector<std::uint32_t>& retainedPathIndices() const;
    [[nodiscard]] const std::string& executionPath() const;
    [[nodiscard]] std::uint64_t pathStateBytesPerPath() const;
    [[nodiscard]] std::uint64_t retainedIndexBytes() const;
    [[nodiscard]] std::uint64_t inputResidentPathStateBytes() const;
    [[nodiscard]] std::uint64_t retainedResidentPathStateBytes() const;
    [[nodiscard]] std::uint64_t removedResidentPathStateBytes() const;

  private:
    ResidentPathCompactionContract(std::uint64_t inputPathCount,
                                   std::vector<std::uint32_t> retainedPathIndices,
                                   std::uint64_t movedPathCount, std::string executionPath,
                                   std::uint64_t pathStateBytesPerPath);

    static void validateRetainedPathIndices(std::uint64_t inputPathCount,
                                            const std::vector<std::uint32_t>& retainedPathIndices);
    static std::uint64_t movedPathCountFor(const std::vector<std::uint32_t>& retainedPathIndices);

    std::uint64_t m_inputPathCount{0};
    std::vector<std::uint32_t> m_retainedPathIndices;
    std::uint64_t m_movedPathCount{0};
    std::string m_executionPath;
    std::uint64_t m_pathStateBytesPerPath{0};
  };

  class TracingPathStateBuffers {
  public:
    explicit TracingPathStateBuffers(const TracingPathStateLayout& layout);
    explicit TracingPathStateBuffers(std::uint64_t capacity);

    [[nodiscard]] const TracingPathStateLayout& layout() const;
    [[nodiscard]] const std::vector<GpuPathStateRecord>& active() const;
    [[nodiscard]] const std::vector<GpuPathStateRecord>& next() const;
    [[nodiscard]] std::vector<GpuPathStateRecord>& active();
    [[nodiscard]] std::vector<GpuPathStateRecord>& next();
    [[nodiscard]] const TracingPathStateDiagnostics& diagnostics() const;

    void clear();
    void clearActive();
    void clearNext();
    void appendActive(const GpuPathStateRecord& record);
    void appendNext(const GpuPathStateRecord& record);
    void swapActiveAndNext();

  private:
    void validateCanAppend(const std::vector<GpuPathStateRecord>& buffer) const;

    TracingPathStateLayout m_layout;
    std::vector<GpuPathStateRecord> m_active;
    std::vector<GpuPathStateRecord> m_next;
    TracingPathStateDiagnostics m_diagnostics;
  };

  struct ResidentPathLoopSettings {
    std::uint32_t maxDepth{8};
    std::uint32_t russianRouletteDepth{3};
  };

  struct ResidentPathLoopDepthDiagnostics {
    std::uint32_t depth{0};
    std::uint64_t inputPathCount{0};
    std::vector<GpuPathStateRecord> retainedRecords;
    ResidentPathCompactionContract compaction =
      ResidentPathCompactionContract::fromRetainedIndices(0, {}, "cpu_resident_path_compaction");
  };

  struct ResidentPathLoopDiagnostics {
    std::vector<ResidentPathLoopDepthDiagnostics> depths;
    std::vector<GpuPathStateRecord> resolvedRecords;
    std::uint64_t finalActiveCount{0};
    TracingPathStateDiagnostics buffers;
  };

  using ResidentDiffusePathStep = std::function<std::optional<GpuPathStateRecord>(
    const GpuPathStateRecord& record, std::uint32_t depth)>;

  struct ResidentPathCompactionResult {
    std::vector<GpuPathStateRecord> retainedRecords;
    ResidentPathCompactionContract contract =
      ResidentPathCompactionContract::fromRetainedIndices(0, {}, "cpu_resident_path_compaction");
  };

  class ResidentPathCompactionBackend {
  public:
    virtual ~ResidentPathCompactionBackend() = default;

    [[nodiscard]] virtual const char* name() const = 0;
    [[nodiscard]] virtual const char* pathStateResidency() const = 0;
    [[nodiscard]] virtual ResidentPathCompactionResult
    compact(const std::vector<GpuPathStateRecord>& sourceRecords,
            const std::vector<std::uint32_t>& retainedPathIndices) const = 0;
  };

  class CpuResidentPathCompactionBackend final : public ResidentPathCompactionBackend {
  public:
    [[nodiscard]] static const CpuResidentPathCompactionBackend& instance();

    [[nodiscard]] const char* name() const override;
    [[nodiscard]] const char* pathStateResidency() const override;
    [[nodiscard]] ResidentPathCompactionResult
    compact(const std::vector<GpuPathStateRecord>& sourceRecords,
            const std::vector<std::uint32_t>& retainedPathIndices) const override;
  };

  [[nodiscard]] ResidentPathLoopDiagnostics
  loopResidentDiffusePaths(TracingPathStateBuffers& buffers,
                           const ResidentPathLoopSettings& settings,
                           const ResidentDiffusePathStep& step);
  [[nodiscard]] ResidentPathLoopDiagnostics loopResidentDiffusePaths(
    TracingPathStateBuffers& buffers, const ResidentPathLoopSettings& settings,
    const ResidentDiffusePathStep& step, const ResidentPathCompactionBackend& compactionBackend);

  [[nodiscard]] TracingAccumulationDiagnostics
  resolveResidentPathLoopImage(const std::vector<GpuPathStateRecord>& records,
                               const TracingAccumulationLayout& layout,
                               Buffer<unsigned int>& target, const Tonemap* tonemap = nullptr);
  [[nodiscard]] TracingAccumulationDiagnostics
  resolveResidentPathLoopImage(const ResidentPathLoopDiagnostics& diagnostics,
                               const TracingAccumulationLayout& layout,
                               Buffer<unsigned int>& target, const Tonemap* tonemap = nullptr);

  [[nodiscard]] std::size_t bytesPerPathState(TracingPathStateFormat format);
  [[nodiscard]] const char* toString(TracingPathStateFormat format);
  [[nodiscard]] bool hasFlag(const GpuPathStateRecord& record, GpuPathStateFlags flag);
  void setFlag(GpuPathStateRecord& record, GpuPathStateFlags flag, bool enabled = true);
  [[nodiscard]] GpuPathStateRecord makeGpuPathStateRecord(
    const Rayd& ray, const Colord& throughput, const Colord& accumulatedRadiance,
    std::uint32_t pixelIndex, std::uint32_t sampleIndex, std::uint32_t depth,
    std::uint32_t flags = flagValue(GpuPathStateFlags::Active) |
                          flagValue(GpuPathStateFlags::BackgroundVisible),
    double bsdfSamplePdf = 0.0, double timeSample = 0.0, double animationFrame = 0.0,
    double animationTime = 0.0, std::uint32_t rngSeed = 0);
  [[nodiscard]] Rayd rayFromGpuPathStateRecord(const GpuPathStateRecord& record);
  [[nodiscard]] Colord throughputFromGpuPathStateRecord(const GpuPathStateRecord& record);
  [[nodiscard]] Colord accumulatedRadianceFromGpuPathStateRecord(const GpuPathStateRecord& record);
}
