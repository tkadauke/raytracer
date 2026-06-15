#include "render/TracingPathStateBuffer.h"

#include <limits>
#include <stdexcept>
#include <type_traits>

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
    record.origin = {static_cast<float>(ray.origin().x()), static_cast<float>(ray.origin().y()),
                     static_cast<float>(ray.origin().z()), static_cast<float>(ray.origin().w())};
    record.direction = {static_cast<float>(ray.direction().x()),
                        static_cast<float>(ray.direction().y()),
                        static_cast<float>(ray.direction().z()), 0.0f};
    record.throughput = {static_cast<float>(throughput.r()), static_cast<float>(throughput.g()),
                         static_cast<float>(throughput.b()), 1.0f};
    record.accumulatedRadiance = {static_cast<float>(accumulatedRadiance.r()),
                                  static_cast<float>(accumulatedRadiance.g()),
                                  static_cast<float>(accumulatedRadiance.b()), 1.0f};
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
    return Rayd(Vector4d(record.origin[0], record.origin[1], record.origin[2], record.origin[3]),
                Vector3d(record.direction[0], record.direction[1], record.direction[2]));
  }

  Colord throughputFromGpuPathStateRecord(const GpuPathStateRecord& record) {
    return Colord(record.throughput[0], record.throughput[1], record.throughput[2]);
  }

  Colord accumulatedRadianceFromGpuPathStateRecord(const GpuPathStateRecord& record) {
    return Colord(record.accumulatedRadiance[0], record.accumulatedRadiance[1],
                  record.accumulatedRadiance[2]);
  }
}
