#include "engine/graph/RenderGraphExecutionTrace.h"

#include "core/Buffer.h"
#include "core/util/BufferUtils.h"
#include "engine/graph/RenderResourceStorage.h"
#include "engine/raster/RasterVisibilitySet.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  namespace {
    constexpr double boostedDiffScale = 8.0;

    template<class T>
    const char* enumName(T value, std::initializer_list<std::pair<T, const char*>> values,
                         const char* fallback = "unknown") {
      for (const auto& [parsed, name] : values) {
        if (value == parsed)
          return name;
      }
      return fallback;
    }

    std::shared_ptr<const Buffer<Colord>> colorPreviewFor(const Buffer<Colord>& source) {
      auto result = std::make_shared<Buffer<Colord>>(source.width(), source.height());
      core::util::copyBuffer(*result, source);
      return result;
    }

    std::shared_ptr<const Buffer<double>> depthPreviewFor(const Buffer<double>& source) {
      auto result = std::make_shared<Buffer<double>>(source.width(), source.height());
      core::util::copyBuffer(*result, source);
      return result;
    }

    std::shared_ptr<const Buffer<Colord>> stencilPreviewFor(const Buffer<std::uint8_t>& source) {
      auto result = std::make_shared<Buffer<Colord>>(source.width(), source.height());

      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          const double value = static_cast<double>(source[y][x]) / 255.0;
          (*result)[y][x] = Colord(value, value, value);
        }
      }
      return result;
    }

    Colord colorForObjectId(std::uint32_t id) {
      if (id == 0) {
        return Colord::black();
      }

      std::uint32_t hash = id * 2654435761u;
      hash ^= hash >> 16;
      return Colord(static_cast<double>((hash >> 16) & 0xffu) / 255.0,
                    static_cast<double>((hash >> 8) & 0xffu) / 255.0,
                    static_cast<double>(hash & 0xffu) / 255.0);
    }

    std::shared_ptr<const Buffer<Colord>> objectIdPreviewFor(const Buffer<std::uint32_t>& source) {
      auto result = std::make_shared<Buffer<Colord>>(source.width(), source.height());

      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          (*result)[y][x] = colorForObjectId(source[y][x]);
        }
      }
      return result;
    }

    bool finiteColorRange(const Buffer<Colord>& source, Colord* minimum, Colord* maximum) {
      double minValues[3] = {std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity()};
      double maxValues[3] = {-std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity()};

      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          for (int component = 0; component != 3; ++component) {
            const double value = source[y][x][component];
            if (!std::isfinite(value)) {
              continue;
            }
            minValues[component] = std::min(minValues[component], value);
            maxValues[component] = std::max(maxValues[component], value);
          }
        }
      }

      *minimum = Colord(minValues[0], minValues[1], minValues[2]);
      *maximum = Colord(maxValues[0], maxValues[1], maxValues[2]);
      return std::isfinite(minValues[0]) && std::isfinite(minValues[1]) &&
             std::isfinite(minValues[2]) && std::isfinite(maxValues[0]) &&
             std::isfinite(maxValues[1]) && std::isfinite(maxValues[2]);
    }

    double normalizedComponent(double value, double minimum, double maximum) {
      if (!std::isfinite(value)) {
        return 0.0;
      }
      const double range = maximum - minimum;
      if (range <= 1e-9) {
        return 0.5;
      }
      return std::clamp((value - minimum) / range, 0.0, 1.0);
    }

    std::shared_ptr<const Buffer<Colord>> worldPositionPreviewFor(const Buffer<Colord>& source) {
      auto result = std::make_shared<Buffer<Colord>>(source.width(), source.height());
      Colord minimum;
      Colord maximum;
      if (!finiteColorRange(source, &minimum, &maximum)) {
        result->clear(Colord::black());
        return result;
      }

      for (int y = 0; y != source.height(); ++y) {
        for (int x = 0; x != source.width(); ++x) {
          (*result)[y][x] = Colord(normalizedComponent(source[y][x].r(), minimum.r(), maximum.r()),
                                   normalizedComponent(source[y][x].g(), minimum.g(), maximum.g()),
                                   normalizedComponent(source[y][x].b(), minimum.b(), maximum.b()));
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

    QJsonArray featureArray(const std::vector<RenderFeatureKind>& features) {
      QJsonArray array;
      for (const auto& feature : features) {
        array.append(QString::fromStdString(feature));
      }
      return array;
    }

    std::string visibilitySetSummary(const engine::raster::RasterVisibilitySet& visibilitySet) {
      std::ostringstream out;
      out << "visibility set has no image preview; leaves=" << visibilitySet.visibleLeafCount()
          << "/" << visibilitySet.leafCount()
          << "; rejectedLeaves=" << visibilitySet.rejectedLeafCount() << "; frustumRejectedLeaves="
          << visibilitySet.rejectedLeafCount(
               engine::raster::RasterVisibilitySet::RejectionReason::Frustum)
          << "; backfaceRejectedLeaves="
          << visibilitySet.rejectedLeafCount(
               engine::raster::RasterVisibilitySet::RejectionReason::Backface)
          << "; tileGrid=" << visibilitySet.tileGrid().columns << "x"
          << visibilitySet.tileGrid().rows
          << "; visibleTileReferences=" << visibilitySet.visibleLeafTileReferenceCount()
          << "; depthSummarizedTiles=" << visibilitySet.tileDepthSummarizedTileCount()
          << "; uncertainTileLeaves=" << visibilitySet.tileUncertainVisibleLeafCount();
      return out.str();
    }

    std::string metadataOnlyReason(const RenderResource& resource) {
      if (resource.visibilitySetBacked()) {
        if (const auto visibilitySet = resource.visibilitySet()) {
          return visibilitySetSummary(*visibilitySet);
        }
      }
      if (const auto& residency = resource.gpuResidency()) {
        std::string reason = "GPU resource is resident on " + residency->backend;
        if (!residency->description.empty()) {
          reason += ": " + residency->description;
        }
        return reason;
      }
      return metadataOnlyReason(resource.descriptor());
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

    RenderGraphCacheMetadata cacheMetadataFor(const RenderResource& resource) {
      if (resource.cacheMetadata()) {
        return *resource.cacheMetadata();
      }
      return cacheMetadataFor(resource.descriptor());
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

  bool RenderGraphCacheMetadata::cacheable() const {
    return m_status != RenderGraphCacheStatus::NotCacheable;
  }

  bool RenderGraphCacheMetadata::usedCachedArtifact() const {
    return m_status == RenderGraphCacheStatus::Hit;
  }

  bool RenderGraphCacheMetadata::storedCachedArtifact() const {
    return m_status == RenderGraphCacheStatus::Stored;
  }

  QJsonObject RenderGraphCacheMetadata::toJson() const {
    QJsonObject object;
    object["status"] = toString(m_status);
    object["message"] = QString::fromStdString(m_message);
    object["cacheable"] = cacheable();
    object["usedCachedArtifact"] = usedCachedArtifact();
    object["storedCachedArtifact"] = storedCachedArtifact();
    return object;
  }

  RenderGraphResourceSnapshot::RenderGraphResourceSnapshot(
    RenderResourceId resourceId, RenderResourceDescriptor descriptor,
    std::shared_ptr<const Buffer<Colord>> colorPreview,
    std::shared_ptr<const Buffer<double>> depthPreview, std::string unavailableReason,
    RenderGraphCacheMetadata cacheMetadata)
      : m_resourceId(std::move(resourceId)),
        m_descriptor(std::move(descriptor)),
        m_colorPreview(std::move(colorPreview)),
        m_depthPreview(std::move(depthPreview)),
        m_unavailableReason(std::move(unavailableReason)),
        m_cacheMetadata(std::move(cacheMetadata)) {
  }

  const RenderResourceId& RenderGraphResourceSnapshot::resourceId() const {
    return m_resourceId;
  }

  const RenderResourceDescriptor& RenderGraphResourceSnapshot::descriptor() const {
    return m_descriptor;
  }

  bool RenderGraphResourceSnapshot::hasPreview() const {
    return hasColorPreview() || hasDepthPreview();
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

  bool RenderGraphResourceSnapshot::hasDepthPreview() const {
    return static_cast<bool>(m_depthPreview);
  }

  const Buffer<double>& RenderGraphResourceSnapshot::depthPreview() const {
    if (!m_depthPreview) {
      throw std::logic_error("render graph resource snapshot has no depth preview");
    }
    return *m_depthPreview;
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
    if (!m_descriptor.features.empty()) {
      object["features"] = featureArray(m_descriptor.features);
    }
    object["width"] = m_descriptor.width;
    object["height"] = m_descriptor.height;
    object["previewAvailable"] = hasPreview();
    object["cache"] = m_cacheMetadata.toJson();
    if (m_colorPreview) {
      object["previewKind"] = "color";
      object["previewWidth"] = m_colorPreview->width();
      object["previewHeight"] = m_colorPreview->height();
    } else if (m_depthPreview) {
      object["previewKind"] = "depth";
      object["previewWidth"] = m_depthPreview->width();
      object["previewHeight"] = m_depthPreview->height();
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

  bool RenderGraphExecutionTrace::hasResourceSnapshots(const RenderResourceId& id) const {
    return !inputSnapshotsForResource(id).empty() || !outputSnapshotsForResource(id).empty();
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

  void RenderGraphExecutionTraceRecorder::recordPassMessage(
    std::shared_ptr<const RenderGraphExecutionTraceSession> session, const RenderPassNode& pass,
    std::string message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!session || !m_current || !currentSessionMatches(*session) || message.empty()) {
      return;
    }

    RenderPassTrace* trace = m_current->findMutablePass(pass.id);
    if (!trace) {
      return;
    }

    if (!trace->m_message.empty()) {
      trace->m_message += "\n";
    }
    trace->m_message += std::move(message);
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
      return RenderGraphResourceSnapshot(resourceId, descriptor, nullptr, nullptr,
                                         "resource was not materialized by this execution path",
                                         cacheMetadataFor(descriptor));
    }

    const auto& resource = storage.resource(resourceId);
    if (resource.depthBacked()) {
      return RenderGraphResourceSnapshot(resourceId, resource.descriptor(), nullptr,
                                         depthPreviewFor(resource.depth()), "",
                                         cacheMetadataFor(resource));
    }

    if (resource.stencilBacked()) {
      return RenderGraphResourceSnapshot(resourceId, resource.descriptor(),
                                         stencilPreviewFor(resource.stencil()), nullptr, "",
                                         cacheMetadataFor(resource));
    }

    if (resource.objectIdBacked()) {
      return RenderGraphResourceSnapshot(resourceId, resource.descriptor(),
                                         objectIdPreviewFor(resource.objectId()), nullptr, "",
                                         cacheMetadataFor(resource));
    }

    if (!resource.colorBacked()) {
      return RenderGraphResourceSnapshot(resourceId, resource.descriptor(), nullptr, nullptr,
                                         metadataOnlyReason(resource), cacheMetadataFor(resource));
    }

    if (resource.descriptor().type == RenderResourceType::WorldPosition) {
      return RenderGraphResourceSnapshot(resourceId, resource.descriptor(),
                                         worldPositionPreviewFor(resource.color()), nullptr, "",
                                         cacheMetadataFor(resource));
    }

    return RenderGraphResourceSnapshot(resourceId, resource.descriptor(),
                                       colorPreviewFor(resource.color()), nullptr, "",
                                       cacheMetadataFor(resource));
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
    return enumName<RenderPassExecutionStatus>(value,
                                               {{RenderPassExecutionStatus::Pending, "pending"},
                                                {RenderPassExecutionStatus::Running, "running"},
                                                {RenderPassExecutionStatus::Completed, "completed"},
                                                {RenderPassExecutionStatus::Failed, "failed"},
                                                {RenderPassExecutionStatus::Skipped, "skipped"}});
  }

  const char* toString(RenderGraphCacheStatus value) {
    return enumName<RenderGraphCacheStatus>(
      value, {{RenderGraphCacheStatus::NotCacheable, "not_cacheable"},
              {RenderGraphCacheStatus::Uncached, "uncached"},
              {RenderGraphCacheStatus::Hit, "hit"},
              {RenderGraphCacheStatus::Miss, "miss"},
              {RenderGraphCacheStatus::Stored, "stored"},
              {RenderGraphCacheStatus::Invalidated, "invalidated"}});
  }

  RenderGraphExecutionTraceSession::RenderGraphExecutionTraceSession(std::uint64_t generation)
      : m_generation(generation) {
  }

  std::uint64_t RenderGraphExecutionTraceSession::generation() const {
    return m_generation;
  }
}
