#include "render/TracingPathStateBuffer.h"

#include "render/PathTermination.h"
#include "render/TracingAccumulationReference.h"
#include "render/samplers/GpuSampleStream.h"
#include "render/samplers/SampleStream.h"

#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace render {
  namespace {
    constexpr bool isGpuRecord() {
      return std::is_standard_layout_v<GpuPathStateRecord> && alignof(GpuPathStateRecord) == 16 &&
             sizeof(GpuPathStateRecord) % 16 == 0;
    }

    static_assert(isGpuRecord());

    std::uint64_t checkedCapacity(std::uint64_t capacity) {
      if (capacity == 0) {
        throw std::invalid_argument("tracing path-state layout requires positive capacity");
      }
      return capacity;
    }

    std::uint64_t checkedProduct(std::uint64_t count, std::size_t bytesPerRecord) {
      const auto bytes = static_cast<std::uint64_t>(bytesPerRecord);
      if (bytes != 0 && count > std::numeric_limits<std::uint64_t>::max() / bytes) {
        throw std::overflow_error("tracing path-state layout byte size overflows");
      }
      return count * bytes;
    }

    std::uint64_t checkedByteProduct(std::uint64_t count, std::uint64_t bytesPerItem) {
      if (bytesPerItem != 0 && count > std::numeric_limits<std::uint64_t>::max() / bytesPerItem) {
        throw std::overflow_error("resident path compaction byte size overflows");
      }
      return count * bytesPerItem;
    }

    std::uint64_t checkedAdd(std::uint64_t a, std::uint64_t b) {
      if (a > std::numeric_limits<std::uint64_t>::max() - b) {
        throw std::overflow_error("tracing path-state layout byte size overflows");
      }
      return a + b;
    }

    TracingPathStateLayout validatedLayout(TracingPathStateLayout layout) {
      layout.validate();
      return layout;
    }
  }

  TracingPathStateLayout TracingPathStateLayout::pathCapacity(std::uint64_t capacity) {
    TracingPathStateLayout layout;
    layout.capacity = capacity;
    layout.validate();
    return layout;
  }

  bool TracingPathStateLayout::hasActiveAndNextBuffers() const {
    return capacity > 0;
  }

  std::uint64_t TracingPathStateLayout::pathStateBytes() const {
    return checkedProduct(1, bytesPerPathState(activeFormat));
  }

  std::uint64_t TracingPathStateLayout::activeBytes() const {
    return checkedProduct(checkedCapacity(capacity), bytesPerPathState(activeFormat));
  }

  std::uint64_t TracingPathStateLayout::nextBytes() const {
    return checkedProduct(checkedCapacity(capacity), bytesPerPathState(nextFormat));
  }

  std::uint64_t TracingPathStateLayout::totalBytes() const {
    return checkedAdd(activeBytes(), nextBytes());
  }

  void TracingPathStateLayout::validate() const {
    checkedCapacity(capacity);
    (void)activeBytes();
    (void)nextBytes();
    (void)totalBytes();
  }

  TracingPathStateDiagnostics
  TracingPathStateDiagnostics::forLayout(const TracingPathStateLayout& layout, const char* backend,
                                         const char* residency) {
    const TracingPathStateLayout validated = validatedLayout(layout);
    TracingPathStateDiagnostics diagnostics;
    diagnostics.backend = backend ? backend : "";
    diagnostics.residency = residency ? residency : "";
    diagnostics.layout = validated;
    diagnostics.residentBytes = validated.totalBytes();
    return diagnostics;
  }

  void TracingPathStateDiagnostics::recordClear() {
    activeCount = 0;
    nextCount = 0;
    ++clearOperations;
  }

  void TracingPathStateDiagnostics::recordAppendActive(std::uint64_t records) {
    activeCount += records;
    appendedActiveRecords += records;
    ++appendOperations;
  }

  void TracingPathStateDiagnostics::recordAppendNext(std::uint64_t records) {
    nextCount += records;
    appendedNextRecords += records;
    ++appendOperations;
  }

  void TracingPathStateDiagnostics::recordSwap(std::uint64_t newActiveCount) {
    activeCount = newActiveCount;
    nextCount = 0;
    ++swapOperations;
  }

  ResidentPathCompactionContract ResidentPathCompactionContract::fromRetainedIndices(
    std::uint64_t inputPathCount, std::vector<std::uint32_t> retainedPathIndices,
    std::string executionPath, std::uint64_t pathStateBytesPerPath) {
    validateRetainedPathIndices(inputPathCount, retainedPathIndices);
    const std::uint64_t movedPathCount = movedPathCountFor(retainedPathIndices);
    return ResidentPathCompactionContract(inputPathCount, std::move(retainedPathIndices),
                                          movedPathCount, std::move(executionPath),
                                          pathStateBytesPerPath);
  }

  std::uint64_t ResidentPathCompactionContract::inputPathCount() const {
    return m_inputPathCount;
  }

  std::uint64_t ResidentPathCompactionContract::retainedPathCount() const {
    return static_cast<std::uint64_t>(m_retainedPathIndices.size());
  }

  std::uint64_t ResidentPathCompactionContract::removedPathCount() const {
    return m_inputPathCount > retainedPathCount() ? m_inputPathCount - retainedPathCount() : 0;
  }

  std::uint64_t ResidentPathCompactionContract::movedPathCount() const {
    return m_movedPathCount;
  }

  double ResidentPathCompactionContract::removedPathFraction() const {
    if (m_inputPathCount == 0) {
      return 0.0;
    }
    return static_cast<double>(removedPathCount()) / static_cast<double>(m_inputPathCount);
  }

  double ResidentPathCompactionContract::movedRetainedPathFraction() const {
    if (retainedPathCount() == 0) {
      return 0.0;
    }
    return static_cast<double>(m_movedPathCount) / static_cast<double>(retainedPathCount());
  }

  const std::vector<std::uint32_t>& ResidentPathCompactionContract::retainedPathIndices() const {
    return m_retainedPathIndices;
  }

  const std::string& ResidentPathCompactionContract::executionPath() const {
    return m_executionPath;
  }

  std::uint64_t ResidentPathCompactionContract::pathStateBytesPerPath() const {
    return m_pathStateBytesPerPath;
  }

  std::uint64_t ResidentPathCompactionContract::retainedIndexBytes() const {
    return checkedByteProduct(retainedPathCount(), sizeof(std::uint32_t));
  }

  std::uint64_t ResidentPathCompactionContract::inputResidentPathStateBytes() const {
    return checkedByteProduct(m_inputPathCount, m_pathStateBytesPerPath);
  }

  std::uint64_t ResidentPathCompactionContract::retainedResidentPathStateBytes() const {
    return checkedByteProduct(retainedPathCount(), m_pathStateBytesPerPath);
  }

  std::uint64_t ResidentPathCompactionContract::removedResidentPathStateBytes() const {
    return checkedByteProduct(removedPathCount(), m_pathStateBytesPerPath);
  }

  ResidentPathCompactionContract::ResidentPathCompactionContract(
    std::uint64_t inputPathCount, std::vector<std::uint32_t> retainedPathIndices,
    std::uint64_t movedPathCount, std::string executionPath, std::uint64_t pathStateBytesPerPath)
      : m_inputPathCount(inputPathCount),
        m_retainedPathIndices(std::move(retainedPathIndices)),
        m_movedPathCount(movedPathCount),
        m_executionPath(std::move(executionPath)),
        m_pathStateBytesPerPath(pathStateBytesPerPath) {
  }

  void ResidentPathCompactionContract::validateRetainedPathIndices(
    std::uint64_t inputPathCount, const std::vector<std::uint32_t>& retainedPathIndices) {
    std::uint32_t previous = 0;
    bool hasPrevious = false;
    for (const std::uint32_t pathIndex : retainedPathIndices) {
      if (pathIndex >= inputPathCount) {
        throw std::out_of_range("resident path compaction retained path index is out of range");
      }
      if (hasPrevious && pathIndex <= previous) {
        throw std::invalid_argument(
          "resident path compaction retained path indices must be strictly increasing");
      }
      previous = pathIndex;
      hasPrevious = true;
    }
  }

  std::uint64_t ResidentPathCompactionContract::movedPathCountFor(
    const std::vector<std::uint32_t>& retainedPathIndices) {
    std::uint64_t movedPathCount = 0;
    for (std::size_t outputIndex = 0; outputIndex != retainedPathIndices.size(); ++outputIndex) {
      if (retainedPathIndices[outputIndex] != outputIndex) {
        ++movedPathCount;
      }
    }
    return movedPathCount;
  }

  TracingPathStateBuffers::TracingPathStateBuffers(const TracingPathStateLayout& layout)
      : m_layout(validatedLayout(layout)),
        m_diagnostics(
          TracingPathStateDiagnostics::forLayout(m_layout, "cpu_reference", "cpu_host")) {
    m_active.reserve(static_cast<std::size_t>(m_layout.capacity));
    m_next.reserve(static_cast<std::size_t>(m_layout.capacity));
  }

  TracingPathStateBuffers::TracingPathStateBuffers(std::uint64_t capacity)
      : TracingPathStateBuffers(TracingPathStateLayout::pathCapacity(capacity)) {
  }

  const TracingPathStateLayout& TracingPathStateBuffers::layout() const {
    return m_layout;
  }

  const std::vector<GpuPathStateRecord>& TracingPathStateBuffers::active() const {
    return m_active;
  }

  const std::vector<GpuPathStateRecord>& TracingPathStateBuffers::next() const {
    return m_next;
  }

  std::vector<GpuPathStateRecord>& TracingPathStateBuffers::active() {
    return m_active;
  }

  std::vector<GpuPathStateRecord>& TracingPathStateBuffers::next() {
    return m_next;
  }

  const TracingPathStateDiagnostics& TracingPathStateBuffers::diagnostics() const {
    return m_diagnostics;
  }

  void TracingPathStateBuffers::clear() {
    m_active.clear();
    m_next.clear();
    m_diagnostics.recordClear();
  }

  void TracingPathStateBuffers::clearActive() {
    m_active.clear();
    m_diagnostics.activeCount = 0;
    ++m_diagnostics.clearOperations;
  }

  void TracingPathStateBuffers::clearNext() {
    m_next.clear();
    m_diagnostics.nextCount = 0;
    ++m_diagnostics.clearOperations;
  }

  void TracingPathStateBuffers::appendActive(const GpuPathStateRecord& record) {
    validateCanAppend(m_active);
    m_active.push_back(record);
    m_diagnostics.recordAppendActive(1);
  }

  void TracingPathStateBuffers::appendNext(const GpuPathStateRecord& record) {
    validateCanAppend(m_next);
    m_next.push_back(record);
    m_diagnostics.recordAppendNext(1);
  }

  void TracingPathStateBuffers::swapActiveAndNext() {
    m_active.swap(m_next);
    m_next.clear();
    m_diagnostics.recordSwap(m_active.size());
  }

  void
  TracingPathStateBuffers::validateCanAppend(const std::vector<GpuPathStateRecord>& buffer) const {
    if (buffer.size() >= m_layout.capacity) {
      throw std::overflow_error("tracing path-state buffer capacity exceeded");
    }
  }

  ResidentPathLoopDiagnostics loopResidentDiffusePaths(TracingPathStateBuffers& buffers,
                                                       const ResidentPathLoopSettings& settings,
                                                       const ResidentDiffusePathStep& step) {
    if (!step) {
      throw std::invalid_argument("resident diffuse path loop requires a path-step callback");
    }
    if (settings.maxDepth == 0) {
      throw std::invalid_argument("resident diffuse path loop requires positive max depth");
    }

    ResidentPathLoopDiagnostics diagnostics;
    for (std::uint32_t depth = 0; depth != settings.maxDepth && !buffers.active().empty();
         ++depth) {
      const std::uint64_t inputPathCount = buffers.active().size();
      if (inputPathCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("resident diffuse path loop active path count exceeds retained "
                                  "index range");
      }
      std::vector<std::uint32_t> retainedPathIndices;
      retainedPathIndices.reserve(buffers.active().size());
      std::vector<GpuPathStateRecord> retainedRecords;
      retainedRecords.reserve(buffers.active().size());
      buffers.clearNext();

      const std::vector<GpuPathStateRecord> active = buffers.active();
      for (std::size_t pathIndex = 0; pathIndex != active.size(); ++pathIndex) {
        const GpuPathStateRecord& current = active[pathIndex];
        if (!hasFlag(current, GpuPathStateFlags::Active)) {
          continue;
        }

        std::optional<GpuPathStateRecord> next = step(current, depth);
        if (!next) {
          diagnostics.resolvedRecords.push_back(current);
          continue;
        }

        next->depth = depth + 1u;
        if (next->depth >= settings.maxDepth) {
          diagnostics.resolvedRecords.push_back(*next);
          continue;
        }

        Colord throughput = throughputFromGpuPathStateRecord(*next);
        if (depth >= settings.russianRouletteDepth) {
          const double roulette =
            GpuSampleStream(current.rngSeed, current.pixelIndex, current.sampleIndex)
              .sample1D(SampleDimension::Continuation, depth);
          const PathContinuation continuation = pathContinuation(throughput, roulette);
          throughput = continuedThroughput(throughput, continuation);
          if (!continuation.continues) {
            diagnostics.resolvedRecords.push_back(*next);
            continue;
          }
          next->throughput = {static_cast<float>(throughput.r()),
                              static_cast<float>(throughput.g()),
                              static_cast<float>(throughput.b()), 1.0f};
        }

        setFlag(*next, GpuPathStateFlags::Active, true);
        buffers.appendNext(*next);
        retainedPathIndices.push_back(static_cast<std::uint32_t>(pathIndex));
        retainedRecords.push_back(*next);
      }

      ResidentPathLoopDepthDiagnostics depthDiagnostics;
      depthDiagnostics.depth = depth;
      depthDiagnostics.inputPathCount = inputPathCount;
      depthDiagnostics.retainedRecords = std::move(retainedRecords);
      depthDiagnostics.compaction = ResidentPathCompactionContract::fromRetainedIndices(
        inputPathCount, std::move(retainedPathIndices), "gpu_resident_path_loop",
        sizeof(GpuPathStateRecord));
      diagnostics.depths.push_back(std::move(depthDiagnostics));

      buffers.swapActiveAndNext();
    }

    diagnostics.finalActiveCount = buffers.active().size();
    diagnostics.buffers = buffers.diagnostics();
    return diagnostics;
  }

  TracingAccumulationDiagnostics
  resolveResidentPathLoopImage(const std::vector<GpuPathStateRecord>& records,
                               const TracingAccumulationLayout& layout,
                               Buffer<unsigned int>& target, const Tonemap* tonemap) {
    TracingAccumulationBuffer accumulation(layout);
    TracingAccumulationDiagnostics diagnostics = TracingAccumulationDiagnostics::forLayout(
      layout, "gpu_resident_path_loop", "resident_accumulation_resolve");
    diagnostics.recordClear();

    const std::uint64_t pixelCount = layout.pixelCount();
    for (const GpuPathStateRecord& record : records) {
      if (record.pixelIndex >= pixelCount) {
        throw std::out_of_range("resident path-loop resolve pixel index is out of range");
      }
      const int x = static_cast<int>(record.pixelIndex % static_cast<std::uint64_t>(layout.width));
      const int y = static_cast<int>(record.pixelIndex / static_cast<std::uint64_t>(layout.width));
      accumulation.addSample(x, y, accumulatedRadianceFromGpuPathStateRecord(record));
      diagnostics.recordAdd(1);
    }

    accumulation.resolve(target, tonemap);
    diagnostics.recordResolve();
    diagnostics.recordReadback(layout.resolveBytes());
    return diagnostics;
  }

  TracingAccumulationDiagnostics
  resolveResidentPathLoopImage(const ResidentPathLoopDiagnostics& diagnostics,
                               const TracingAccumulationLayout& layout,
                               Buffer<unsigned int>& target, const Tonemap* tonemap) {
    return resolveResidentPathLoopImage(diagnostics.resolvedRecords, layout, target, tonemap);
  }

  std::size_t bytesPerPathState(TracingPathStateFormat format) {
    switch (format) {
    case TracingPathStateFormat::GpuPathStateRecordV1:
      return sizeof(GpuPathStateRecord);
    }
    return 0;
  }

  const char* toString(TracingPathStateFormat format) {
    switch (format) {
    case TracingPathStateFormat::GpuPathStateRecordV1:
      return "gpu_path_state_record_v1";
    }
    return "unknown";
  }

  bool hasFlag(const GpuPathStateRecord& record, GpuPathStateFlags flag) {
    return (record.flags & flagValue(flag)) != 0;
  }

  void setFlag(GpuPathStateRecord& record, GpuPathStateFlags flag, bool enabled) {
    if (enabled) {
      record.flags |= flagValue(flag);
    } else {
      record.flags &= ~flagValue(flag);
    }
  }

  GpuPathStateRecord makeGpuPathStateRecord(const Rayd& ray, const Colord& throughput,
                                            const Colord& accumulatedRadiance,
                                            std::uint32_t pixelIndex, std::uint32_t sampleIndex,
                                            std::uint32_t depth, std::uint32_t flags,
                                            double bsdfSamplePdf, double timeSample,
                                            double animationFrame, double animationTime,
                                            std::uint32_t rngSeed) {
    GpuPathStateRecord record;
    record.origin = ray.origin().toFloat4();
    record.direction = ray.direction().toFloat4();
    record.throughput = throughput.toFloat4();
    record.accumulatedRadiance = accumulatedRadiance.toFloat4();
    record.continuation = {static_cast<float>(bsdfSamplePdf), static_cast<float>(timeSample),
                           static_cast<float>(animationFrame), static_cast<float>(animationTime)};
    record.pixelIndex = pixelIndex;
    record.sampleIndex = sampleIndex;
    record.depth = depth;
    record.flags = flags;
    record.rngSeed = rngSeed;
    return record;
  }

  Rayd rayFromGpuPathStateRecord(const GpuPathStateRecord& record) {
    return Rayd(Vector4d(record.origin), Vector3d(record.direction));
  }

  Colord throughputFromGpuPathStateRecord(const GpuPathStateRecord& record) {
    return Colord(record.throughput);
  }

  Colord accumulatedRadianceFromGpuPathStateRecord(const GpuPathStateRecord& record) {
    return Colord(record.accumulatedRadiance);
  }
}
