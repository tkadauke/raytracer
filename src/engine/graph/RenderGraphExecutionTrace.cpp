#include "engine/graph/RenderGraphExecutionTrace.h"

#include "core/Buffer.h"
#include "engine/graph/RenderResourceStorage.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    constexpr double boostedDiffScale = 8.0;

    std::shared_ptr<const Buffer<Colord>> colorPreviewFor(const Buffer<Colord>& source) {
      auto result = std::make_shared<Buffer<Colord>>(source.width(), source.height());

      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          (*result)[y][x] = source[y][x];
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

    RenderGraphCacheMetadata cacheMetadataFor(const RenderResourceDescriptor& descriptor) {
      if (descriptor.lifetime != RenderResourceLifetime::PersistentCache) {
        return RenderGraphCacheMetadata(RenderGraphCacheStatus::NotCacheable,
                                        std::string("resource lifetime is ") +
                                          toString(descriptor.lifetime));
      }

      return RenderGraphCacheMetadata(
        RenderGraphCacheStatus::Uncached,
        "persistent cache resource was materialized without a cached artifact payload");
    }
  }

  RenderGraphCacheMetadata::RenderGraphCacheMetadata(RenderGraphCacheStatus status,
                                                     std::string message)
      : m_status(status),
        m_message(std::move(message)) {
  }

  RenderGraphCacheStatus RenderGraphCacheMetadata::status() const {
    return m_status;
  }

  const std::string& RenderGraphCacheMetadata::message() const {
    return m_message;
  }

  QJsonObject RenderGraphCacheMetadata::toJson() const {
    QJsonObject object;
    object["status"] = toString(m_status);
    object["message"] = QString::fromStdString(m_message);
    return object;
  }

  RenderGraphResourceSnapshot::RenderGraphResourceSnapshot(
    RenderResourceId resourceId, RenderResourceDescriptor descriptor,
    std::shared_ptr<const Buffer<Colord>> colorPreview, std::string unavailableReason,
    RenderGraphCacheMetadata cacheMetadata)
      : m_resourceId(std::move(resourceId)),
        m_descriptor(std::move(descriptor)),
        m_colorPreview(std::move(colorPreview)),
        m_unavailableReason(std::move(unavailableReason)),
        m_cacheMetadata(std::move(cacheMetadata)) {
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

  const RenderGraphCacheMetadata& RenderGraphResourceSnapshot::cacheMetadata() const {
    return m_cacheMetadata;
  }

  QJsonObject RenderGraphResourceSnapshot::toJson() const {
    QJsonObject object;
    object["resource"] = QString::fromStdString(m_resourceId);
    object["type"] = toString(m_descriptor.type);
    object["format"] = toString(m_descriptor.format);
    object["width"] = m_descriptor.width;
    object["height"] = m_descriptor.height;
    object["previewAvailable"] = hasColorPreview();
    object["cache"] = m_cacheMetadata.toJson();
    if (m_colorPreview) {
      object["previewWidth"] = m_colorPreview->width();
      object["previewHeight"] = m_colorPreview->height();
    } else {
      object["unavailableReason"] = QString::fromStdString(m_unavailableReason);
    }
    return object;
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

  QJsonObject RenderGraphResourceDiff::toJson() const {
    QJsonObject object;
    object["inputResource"] = QString::fromStdString(m_inputResourceId);
    object["outputResource"] = QString::fromStdString(m_outputResourceId);
    object["previewAvailable"] = hasPreview();
    if (m_absolutePreview && m_boostedPreview) {
      object["previewWidth"] = m_absolutePreview->width();
      object["previewHeight"] = m_absolutePreview->height();
      object["boostedPreviewWidth"] = m_boostedPreview->width();
      object["boostedPreviewHeight"] = m_boostedPreview->height();
    } else {
      object["unavailableReason"] = QString::fromStdString(m_unavailableReason);
    }
    return object;
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

  QJsonObject RenderPassTrace::toJson() const {
    QJsonArray inputs;
    for (const auto& input : m_inputs) {
      inputs.push_back(input.toJson());
    }

    QJsonArray outputs;
    for (const auto& output : m_outputs) {
      outputs.push_back(output.toJson());
    }

    QJsonArray diffs;
    for (const auto& diff : m_diffs) {
      diffs.push_back(diff.toJson());
    }

    QJsonObject object;
    object["id"] = QString::fromStdString(m_passId);
    object["status"] = toString(m_status);
    object["elapsedMs"] = m_elapsed.count() / 1000000.0;
    object["message"] = QString::fromStdString(m_message);
    object["inputs"] = inputs;
    object["outputs"] = outputs;
    object["diffs"] = diffs;
    return object;
  }

  RenderGraphExecutionTrace::RenderGraphExecutionTrace(RenderPlan plan,
                                                       std::string inputFingerprint)
      : m_plan(std::move(plan)),
        m_inputFingerprint(std::move(inputFingerprint)) {
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

  const std::string& RenderGraphExecutionTrace::inputFingerprint() const {
    return m_inputFingerprint;
  }

  const RenderPassTrace* RenderGraphExecutionTrace::findPass(const RenderPassId& id) const {
    const auto it = m_passIndexes.find(id);
    return it == m_passIndexes.end() ? nullptr : &m_passes[it->second];
  }

  std::vector<const RenderGraphResourceSnapshot*>
  RenderGraphExecutionTrace::inputSnapshotsForResource(const RenderResourceId& id) const {
    std::vector<const RenderGraphResourceSnapshot*> result;
    for (const auto& pass : m_passes) {
      for (const auto& input : pass.inputs()) {
        if (input.resourceId() == id) {
          result.push_back(&input);
        }
      }
    }
    return result;
  }

  std::vector<const RenderGraphResourceSnapshot*>
  RenderGraphExecutionTrace::outputSnapshotsForResource(const RenderResourceId& id) const {
    std::vector<const RenderGraphResourceSnapshot*> result;
    for (const auto& pass : m_passes) {
      for (const auto& output : pass.outputs()) {
        if (output.resourceId() == id) {
          result.push_back(&output);
        }
      }
    }
    return result;
  }

  std::vector<const RenderGraphResourceDiff*>
  RenderGraphExecutionTrace::diffsForResource(const RenderResourceId& id) const {
    std::vector<const RenderGraphResourceDiff*> result;
    for (const auto& pass : m_passes) {
      for (const auto& diff : pass.diffs()) {
        if (diff.inputResourceId() == id || diff.outputResourceId() == id) {
          result.push_back(&diff);
        }
      }
    }
    return result;
  }

  bool RenderGraphExecutionTrace::matchesPlan(const RenderPlan& plan) const {
    return m_plan.executionEquivalentTo(plan);
  }

  bool RenderGraphExecutionTrace::matchesPlanAndInputs(const RenderPlan& plan,
                                                       const std::string& inputFingerprint) const {
    return matchesPlan(plan) && m_inputFingerprint == inputFingerprint;
  }

  QJsonObject RenderGraphExecutionTrace::toJson() const {
    QJsonArray passes;
    for (const auto& pass : m_passes) {
      passes.push_back(pass.toJson());
    }

    QJsonObject object;
    object["plan"] = m_plan.toJson();
    object["inputFingerprint"] = QString::fromStdString(m_inputFingerprint);
    object["passes"] = passes;
    return object;
  }

  RenderPassTrace* RenderGraphExecutionTrace::findMutablePass(const RenderPassId& id) {
    const auto it = m_passIndexes.find(id);
    return it == m_passIndexes.end() ? nullptr : &m_passes[it->second];
  }

  std::shared_ptr<const RenderGraphExecutionTraceSession>
  RenderGraphExecutionTraceRecorder::begin(RenderPlan plan, std::string inputFingerprint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint64_t generation = m_nextGeneration++;
    m_currentGeneration = generation;
    m_current = std::shared_ptr<RenderGraphExecutionTrace>(
      new RenderGraphExecutionTrace(std::move(plan), std::move(inputFingerprint)));
    m_last = m_current;
    return std::shared_ptr<const RenderGraphExecutionTraceSession>(
      new RenderGraphExecutionTraceSession(generation));
  }

  void RenderGraphExecutionTraceRecorder::finish(
    std::shared_ptr<const RenderGraphExecutionTraceSession> session) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (session && m_current && currentSessionMatches(*session)) {
      m_last = m_current;
      m_current.reset();
      m_currentGeneration = 0;
    }
  }

  void RenderGraphExecutionTraceRecorder::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_current.reset();
    m_last.reset();
    m_currentGeneration = 0;
  }

  void RenderGraphExecutionTraceRecorder::passStarted(
    std::shared_ptr<const RenderGraphExecutionTraceSession> session, const RenderPassNode& pass,
    const RenderResourceStorage& storage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!session || !m_current || !currentSessionMatches(*session)) {
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

  void RenderGraphExecutionTraceRecorder::passCompleted(
    std::shared_ptr<const RenderGraphExecutionTraceSession> session, const RenderPassNode& pass,
    const RenderResourceStorage& storage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!session || !m_current || !currentSessionMatches(*session)) {
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

  void RenderGraphExecutionTraceRecorder::passSkipped(
    std::shared_ptr<const RenderGraphExecutionTraceSession> session, const RenderPassNode& pass,
    const RenderResourceStorage& storage, std::string message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!session || !m_current || !currentSessionMatches(*session)) {
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

  void RenderGraphExecutionTraceRecorder::passFailed(
    std::shared_ptr<const RenderGraphExecutionTraceSession> session, const RenderPassNode& pass,
    const RenderResourceStorage& storage, std::string message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!session || !m_current || !currentSessionMatches(*session)) {
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

  bool RenderGraphExecutionTraceRecorder::currentSessionMatches(
    const RenderGraphExecutionTraceSession& session) const {
    return session.generation() == m_currentGeneration;
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
                                         "resource was not materialized by this execution path",
                                         cacheMetadataFor(descriptor));
    }

    const auto& resource = storage.resource(resourceId);
    if (!resource.colorBacked()) {
      return RenderGraphResourceSnapshot(resourceId, resource.descriptor(), nullptr,
                                         metadataOnlyReason(resource.descriptor()),
                                         cacheMetadataFor(resource.descriptor()));
    }

    return RenderGraphResourceSnapshot(resourceId, resource.descriptor(),
                                       colorPreviewFor(resource.color()), "",
                                       cacheMetadataFor(resource.descriptor()));
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

  const char* toString(RenderGraphCacheStatus value) {
    switch (value) {
    case RenderGraphCacheStatus::NotCacheable:
      return "not_cacheable";
    case RenderGraphCacheStatus::Uncached:
      return "uncached";
    case RenderGraphCacheStatus::Hit:
      return "hit";
    case RenderGraphCacheStatus::Miss:
      return "miss";
    case RenderGraphCacheStatus::Stored:
      return "stored";
    case RenderGraphCacheStatus::Invalidated:
      return "invalidated";
    }
    return "unknown";
  }

  RenderGraphExecutionTraceSession::RenderGraphExecutionTraceSession(std::uint64_t generation)
      : m_generation(generation) {
  }

  std::uint64_t RenderGraphExecutionTraceSession::generation() const {
    return m_generation;
  }
}
