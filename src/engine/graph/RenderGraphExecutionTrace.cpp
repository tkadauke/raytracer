#include "engine/graph/RenderGraphExecutionTrace.h"

#include "core/Buffer.h"
#include "engine/graph/RenderResourceStorage.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    constexpr int maxPreviewDimension = 256;
    constexpr double boostedDiffScale = 8.0;

    int previewDimension(int sourceWidth, int sourceHeight, bool width) {
      const double scale = std::max(sourceWidth / double(maxPreviewDimension),
                                    sourceHeight / double(maxPreviewDimension));
      if (scale <= 1.0) {
        return width ? sourceWidth : sourceHeight;
      }

      const int sourceDimension = width ? sourceWidth : sourceHeight;
      return std::max(1, static_cast<int>(std::lround(sourceDimension / scale)));
    }

    std::shared_ptr<const Buffer<Colord>> colorPreviewFor(const Buffer<Colord>& source) {
      const int width = previewDimension(source.width(), source.height(), true);
      const int height = previewDimension(source.width(), source.height(), false);
      auto result = std::make_shared<Buffer<Colord>>(width, height);

      for (int y = 0; y != height; ++y) {
        const int sourceY = std::min(source.height() - 1, y * source.height() / height);
        for (int x = 0; x != width; ++x) {
          const int sourceX = std::min(source.width() - 1, x * source.width() / width);
          (*result)[y][x] = source[sourceY][sourceX];
        }
      }
      return result;
    }

    Colord absoluteDifference(const Colord& first, const Colord& second) {
      return Colord(std::abs(first.r() - second.r()), std::abs(first.g() - second.g()),
                    std::abs(first.b() - second.b()));
    }

    Colord boostedDifference(const Colord& difference) {
      return Colord(std::min(1.0, difference.r() * boostedDiffScale),
                    std::min(1.0, difference.g() * boostedDiffScale),
                    std::min(1.0, difference.b() * boostedDiffScale));
    }

    std::pair<std::shared_ptr<const Buffer<Colord>>, std::shared_ptr<const Buffer<Colord>>>
    diffPreviewsFor(const Buffer<Colord>& input, const Buffer<Colord>& output) {
      auto absolute = std::make_shared<Buffer<Colord>>(input.width(), input.height());
      auto boosted = std::make_shared<Buffer<Colord>>(input.width(), input.height());

      for (int y = 0; y != input.height(); ++y) {
        for (int x = 0; x != input.width(); ++x) {
          const Colord difference = absoluteDifference(input[y][x], output[y][x]);
          (*absolute)[y][x] = difference;
          (*boosted)[y][x] = boostedDifference(difference);
        }
      }
      return {absolute, boosted};
    }

    std::string metadataOnlyReason(const RenderResourceDescriptor& descriptor) {
      return std::string("preview is not available for ") + toString(descriptor.type) +
             " resources";
    }
  }

  RenderGraphResourceSnapshot::RenderGraphResourceSnapshot(
    RenderResourceId resourceId, RenderResourceDescriptor descriptor,
    std::shared_ptr<const Buffer<Colord>> colorPreview, std::string unavailableReason)
      : m_resourceId(std::move(resourceId)),
        m_descriptor(std::move(descriptor)),
        m_colorPreview(std::move(colorPreview)),
        m_unavailableReason(std::move(unavailableReason)) {
  }

  const RenderResourceId& RenderGraphResourceSnapshot::resourceId() const {
    return m_resourceId;
  }

  const RenderResourceDescriptor& RenderGraphResourceSnapshot::descriptor() const {
    return m_descriptor;
  }

  bool RenderGraphResourceSnapshot::hasColorPreview() const {
    return static_cast<bool>(m_colorPreview);
  }

  const Buffer<Colord>& RenderGraphResourceSnapshot::colorPreview() const {
    if (!m_colorPreview) {
      throw std::logic_error("render graph resource snapshot has no color preview");
    }
    return *m_colorPreview;
  }

  const std::string& RenderGraphResourceSnapshot::unavailableReason() const {
    return m_unavailableReason;
  }

  RenderGraphResourceDiff::RenderGraphResourceDiff(
    RenderResourceId inputResourceId, RenderResourceId outputResourceId,
    std::shared_ptr<const Buffer<Colord>> absolutePreview,
    std::shared_ptr<const Buffer<Colord>> boostedPreview, std::string unavailableReason)
      : m_inputResourceId(std::move(inputResourceId)),
        m_outputResourceId(std::move(outputResourceId)),
        m_absolutePreview(std::move(absolutePreview)),
        m_boostedPreview(std::move(boostedPreview)),
        m_unavailableReason(std::move(unavailableReason)) {
  }

  const RenderResourceId& RenderGraphResourceDiff::inputResourceId() const {
    return m_inputResourceId;
  }

  const RenderResourceId& RenderGraphResourceDiff::outputResourceId() const {
    return m_outputResourceId;
  }

  bool RenderGraphResourceDiff::hasPreview() const {
    return m_absolutePreview && m_boostedPreview;
  }

  const Buffer<Colord>& RenderGraphResourceDiff::absolutePreview() const {
    if (!m_absolutePreview) {
      throw std::logic_error("render graph resource diff has no absolute preview");
    }
    return *m_absolutePreview;
  }

  const Buffer<Colord>& RenderGraphResourceDiff::boostedPreview() const {
    if (!m_boostedPreview) {
      throw std::logic_error("render graph resource diff has no boosted preview");
    }
    return *m_boostedPreview;
  }

  const std::string& RenderGraphResourceDiff::unavailableReason() const {
    return m_unavailableReason;
  }

  RenderPassTrace::RenderPassTrace(RenderPassId passId)
      : m_passId(std::move(passId)) {
  }

  const RenderPassId& RenderPassTrace::passId() const {
    return m_passId;
  }

  RenderPassExecutionStatus RenderPassTrace::status() const {
    return m_status;
  }

  const std::vector<RenderGraphResourceSnapshot>& RenderPassTrace::inputs() const {
    return m_inputs;
  }

  const std::vector<RenderGraphResourceSnapshot>& RenderPassTrace::outputs() const {
    return m_outputs;
  }

  const std::vector<RenderGraphResourceDiff>& RenderPassTrace::diffs() const {
    return m_diffs;
  }

  std::chrono::nanoseconds RenderPassTrace::elapsed() const {
    return m_elapsed;
  }

  const std::string& RenderPassTrace::message() const {
    return m_message;
  }

  RenderGraphExecutionTrace::RenderGraphExecutionTrace(RenderPlan plan)
      : m_plan(std::move(plan)) {
    m_passes.reserve(m_plan.passes().size());
    for (const auto& pass : m_plan.passes()) {
      m_passIndexes.emplace(pass.id, m_passes.size());
      m_passes.push_back(RenderPassTrace(pass.id));
    }
  }

  const RenderPlan& RenderGraphExecutionTrace::plan() const {
    return m_plan;
  }

  const std::vector<RenderPassTrace>& RenderGraphExecutionTrace::passes() const {
    return m_passes;
  }

  const RenderPassTrace* RenderGraphExecutionTrace::findPass(const RenderPassId& id) const {
    const auto it = m_passIndexes.find(id);
    return it == m_passIndexes.end() ? nullptr : &m_passes[it->second];
  }

  RenderPassTrace* RenderGraphExecutionTrace::findMutablePass(const RenderPassId& id) {
    const auto it = m_passIndexes.find(id);
    return it == m_passIndexes.end() ? nullptr : &m_passes[it->second];
  }

  void RenderGraphExecutionTraceRecorder::begin(RenderPlan plan) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_current =
      std::shared_ptr<RenderGraphExecutionTrace>(new RenderGraphExecutionTrace(std::move(plan)));
    m_last = m_current;
  }

  void RenderGraphExecutionTraceRecorder::finish() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_current) {
      m_last = m_current;
      m_current.reset();
    }
  }

  void RenderGraphExecutionTraceRecorder::passStarted(const RenderPassNode& pass,
                                                      const RenderResourceStorage& storage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_current) {
      return;
    }

    RenderPassTrace* trace = m_current->findMutablePass(pass.id);
    if (!trace) {
      return;
    }

    trace->m_status = RenderPassExecutionStatus::Running;
    trace->m_startedAt = std::chrono::steady_clock::now();
    trace->m_elapsed = std::chrono::nanoseconds(0);
    trace->m_message.clear();
    trace->m_inputs = snapshotsForReads(*m_current, pass, storage);
    trace->m_outputs.clear();
    trace->m_diffs.clear();
  }

  void RenderGraphExecutionTraceRecorder::passCompleted(const RenderPassNode& pass,
                                                        const RenderResourceStorage& storage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_current) {
      return;
    }

    RenderPassTrace* trace = m_current->findMutablePass(pass.id);
    if (!trace) {
      return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (trace->m_startedAt) {
      trace->m_elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now - *trace->m_startedAt);
    }
    trace->m_status = RenderPassExecutionStatus::Completed;
    trace->m_outputs = snapshotsForWrites(*m_current, pass, storage);
    trace->m_diffs = diffsFor(trace->m_inputs, trace->m_outputs);
  }

  void RenderGraphExecutionTraceRecorder::passSkipped(const RenderPassNode& pass,
                                                      const RenderResourceStorage& storage,
                                                      std::string message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_current) {
      return;
    }

    RenderPassTrace* trace = m_current->findMutablePass(pass.id);
    if (!trace) {
      return;
    }

    trace->m_status = RenderPassExecutionStatus::Skipped;
    trace->m_elapsed = std::chrono::nanoseconds(0);
    trace->m_startedAt.reset();
    trace->m_message = std::move(message);
    trace->m_inputs = snapshotsForReads(*m_current, pass, storage);
    trace->m_outputs = snapshotsForWrites(*m_current, pass, storage);
    trace->m_diffs = diffsFor(trace->m_inputs, trace->m_outputs);
  }

  void RenderGraphExecutionTraceRecorder::passFailed(const RenderPassNode& pass,
                                                     const RenderResourceStorage& storage,
                                                     std::string message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_current) {
      return;
    }

    RenderPassTrace* trace = m_current->findMutablePass(pass.id);
    if (!trace) {
      return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (trace->m_startedAt) {
      trace->m_elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now - *trace->m_startedAt);
    }
    trace->m_status = RenderPassExecutionStatus::Failed;
    trace->m_message = std::move(message);
    trace->m_outputs = snapshotsForWrites(*m_current, pass, storage);
    trace->m_diffs = diffsFor(trace->m_inputs, trace->m_outputs);
  }

  std::shared_ptr<const RenderGraphExecutionTrace>
  RenderGraphExecutionTraceRecorder::lastTrace() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_current) {
      return std::make_shared<RenderGraphExecutionTrace>(*m_current);
    }
    if (m_last) {
      return std::make_shared<RenderGraphExecutionTrace>(*m_last);
    }
    return nullptr;
  }

  std::vector<RenderGraphResourceSnapshot>
  RenderGraphExecutionTraceRecorder::snapshotsForReads(const RenderGraphExecutionTrace& trace,
                                                       const RenderPassNode& pass,
                                                       const RenderResourceStorage& storage) const {
    std::vector<RenderGraphResourceSnapshot> result;
    result.reserve(pass.reads.size());
    for (const auto& read : pass.reads) {
      result.push_back(snapshotForResource(trace, read.resource, storage));
    }
    return result;
  }

  std::vector<RenderGraphResourceSnapshot> RenderGraphExecutionTraceRecorder::snapshotsForWrites(
    const RenderGraphExecutionTrace& trace, const RenderPassNode& pass,
    const RenderResourceStorage& storage) const {
    std::vector<RenderGraphResourceSnapshot> result;
    result.reserve(pass.writes.size());
    for (const auto& write : pass.writes) {
      result.push_back(snapshotForResource(trace, write.resource, storage));
    }
    return result;
  }

  RenderGraphResourceSnapshot RenderGraphExecutionTraceRecorder::snapshotForResource(
    const RenderGraphExecutionTrace& trace, const RenderResourceId& resourceId,
    const RenderResourceStorage& storage) const {
    RenderResourceDescriptor descriptor;
    if (const auto* planned = trace.plan().findResource(resourceId)) {
      descriptor = *planned;
    } else {
      descriptor.id = resourceId;
    }

    if (!storage.contains(resourceId)) {
      return RenderGraphResourceSnapshot(resourceId, descriptor, nullptr,
                                         "resource was not materialized by this execution path");
    }

    const auto& resource = storage.resource(resourceId);
    if (!resource.colorBacked()) {
      return RenderGraphResourceSnapshot(resourceId, resource.descriptor(), nullptr,
                                         metadataOnlyReason(resource.descriptor()));
    }

    return RenderGraphResourceSnapshot(resourceId, resource.descriptor(),
                                       colorPreviewFor(resource.color()), "");
  }

  std::vector<RenderGraphResourceDiff> RenderGraphExecutionTraceRecorder::diffsFor(
    const std::vector<RenderGraphResourceSnapshot>& inputs,
    const std::vector<RenderGraphResourceSnapshot>& outputs) const {
    std::vector<const RenderGraphResourceSnapshot*> colorInputs;
    std::vector<const RenderGraphResourceSnapshot*> colorOutputs;
    for (const auto& input : inputs) {
      if (input.hasColorPreview()) {
        colorInputs.push_back(&input);
      }
    }
    for (const auto& output : outputs) {
      if (output.hasColorPreview()) {
        colorOutputs.push_back(&output);
      }
    }

    if (colorInputs.size() != 1 || colorOutputs.size() != 1) {
      return {};
    }

    const auto& input = *colorInputs.front();
    const auto& output = *colorOutputs.front();
    const Buffer<Colord>& inputPreview = input.colorPreview();
    const Buffer<Colord>& outputPreview = output.colorPreview();
    if (inputPreview.width() != outputPreview.width() ||
        inputPreview.height() != outputPreview.height()) {
      return {RenderGraphResourceDiff(input.resourceId(), output.resourceId(), nullptr, nullptr,
                                      "color previews have different dimensions")};
    }

    auto previews = diffPreviewsFor(inputPreview, outputPreview);
    return {RenderGraphResourceDiff(input.resourceId(), output.resourceId(), previews.first,
                                    previews.second, "")};
  }

  const char* toString(RenderPassExecutionStatus value) {
    switch (value) {
    case RenderPassExecutionStatus::Pending:
      return "pending";
    case RenderPassExecutionStatus::Running:
      return "running";
    case RenderPassExecutionStatus::Completed:
      return "completed";
    case RenderPassExecutionStatus::Failed:
      return "failed";
    case RenderPassExecutionStatus::Skipped:
      return "skipped";
    }
    return "unknown";
  }
}
