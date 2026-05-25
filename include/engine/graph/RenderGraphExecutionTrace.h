#pragma once

#include "core/Color.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/graph/RenderPlan.h"

#include <QJsonObject>

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

template<class T>
class Buffer;

namespace engine::graph {
  class RenderResourceStorage;
  class RenderGraphExecutionTraceSession;

  enum class RenderPassExecutionStatus { Pending, Running, Completed, Failed, Skipped };
  enum class RenderGraphCacheStatus { NotCacheable, Uncached, Hit, Miss, Stored, Invalidated };

  /**
    * Cache provenance for one resource snapshot in an execution trace.
    */
  class RenderGraphCacheMetadata {
  public:
    RenderGraphCacheMetadata(RenderGraphCacheStatus status = RenderGraphCacheStatus::NotCacheable,
                             std::string message = {});

    RenderGraphCacheStatus status() const;
    const std::string& message() const;
    QJsonObject toJson() const;

  private:
    RenderGraphCacheStatus m_status;
    std::string m_message;
  };

  /**
    * Snapshot of one render resource as captured during graph execution.
    *
    * The first trace implementation stores color previews for CPU color
    * resources. Other resource types remain metadata-only until they have
    * specialized viewers.
    */
  class RenderGraphResourceSnapshot {
  public:
    RenderGraphResourceSnapshot(RenderResourceId resourceId, RenderResourceDescriptor descriptor,
                                std::shared_ptr<const Buffer<Colord>> colorPreview,
                                std::shared_ptr<const Buffer<double>> depthPreview,
                                std::string unavailableReason,
                                RenderGraphCacheMetadata cacheMetadata);

    const RenderResourceId& resourceId() const;
    const RenderResourceDescriptor& descriptor() const;
    bool hasColorPreview() const;
    const Buffer<Colord>& colorPreview() const;
    bool hasDepthPreview() const;
    const Buffer<double>& depthPreview() const;
    const std::string& unavailableReason() const;
    const RenderGraphCacheMetadata& cacheMetadata() const;
    QJsonObject toJson() const;

  private:
    RenderResourceId m_resourceId;
    RenderResourceDescriptor m_descriptor;
    std::shared_ptr<const Buffer<Colord>> m_colorPreview;
    std::shared_ptr<const Buffer<double>> m_depthPreview;
    std::string m_unavailableReason;
    RenderGraphCacheMetadata m_cacheMetadata;
  };

  /**
    * Difference image between a pass's single color input and single color
    * output. Both absolute and boosted previews are provided because subtle
    * filters can be hard to see in raw RGB difference.
    */
  class RenderGraphResourceDiff {
  public:
    RenderGraphResourceDiff(RenderResourceId inputResourceId, RenderResourceId outputResourceId,
                            std::shared_ptr<const Buffer<Colord>> absolutePreview,
                            std::shared_ptr<const Buffer<Colord>> boostedPreview,
                            std::string unavailableReason);

    const RenderResourceId& inputResourceId() const;
    const RenderResourceId& outputResourceId() const;
    bool hasPreview() const;
    const Buffer<Colord>& absolutePreview() const;
    const Buffer<Colord>& boostedPreview() const;
    const std::string& unavailableReason() const;
    QJsonObject toJson() const;

  private:
    RenderResourceId m_inputResourceId;
    RenderResourceId m_outputResourceId;
    std::shared_ptr<const Buffer<Colord>> m_absolutePreview;
    std::shared_ptr<const Buffer<Colord>> m_boostedPreview;
    std::string m_unavailableReason;
  };

  /**
    * Trace record for one pass in one executed graph.
    */
  class RenderPassTrace {
  public:
    const RenderPassId& passId() const;
    RenderPassExecutionStatus status() const;
    const std::vector<RenderGraphResourceSnapshot>& inputs() const;
    const std::vector<RenderGraphResourceSnapshot>& outputs() const;
    const std::vector<RenderGraphResourceDiff>& diffs() const;
    std::chrono::nanoseconds elapsed() const;
    const std::string& message() const;
    QJsonObject toJson() const;

  private:
    friend class RenderGraphExecutionTrace;
    friend class RenderGraphExecutionTraceRecorder;

    explicit RenderPassTrace(RenderPassId passId);

    RenderPassId m_passId;
    RenderPassExecutionStatus m_status{RenderPassExecutionStatus::Pending};
    std::vector<RenderGraphResourceSnapshot> m_inputs;
    std::vector<RenderGraphResourceSnapshot> m_outputs;
    std::vector<RenderGraphResourceDiff> m_diffs;
    std::chrono::nanoseconds m_elapsed{0};
    std::optional<std::chrono::steady_clock::time_point> m_startedAt;
    std::string m_message;
  };

  /**
    * Result of one graph execution, separate from the declarative plan.
    */
  class RenderGraphExecutionTrace {
  public:
    const RenderPlan& plan() const;
    const std::vector<RenderPassTrace>& passes() const;
    const std::string& inputFingerprint() const;
    const RenderPassTrace* findPass(const RenderPassId& id) const;
    std::vector<const RenderGraphResourceSnapshot*>
    inputSnapshotsForResource(const RenderResourceId& id) const;
    std::vector<const RenderGraphResourceSnapshot*>
    outputSnapshotsForResource(const RenderResourceId& id) const;
    std::vector<const RenderGraphResourceDiff*> diffsForResource(const RenderResourceId& id) const;
    bool matchesPlan(const RenderPlan& plan) const;
    bool matchesPlanAndInputs(const RenderPlan& plan, const std::string& inputFingerprint) const;
    QJsonObject toJson() const;

  private:
    friend class RenderGraphExecutionTraceRecorder;

    RenderGraphExecutionTrace(RenderPlan plan, std::string inputFingerprint);
    RenderPassTrace* findMutablePass(const RenderPassId& id);

    RenderPlan m_plan;
    std::string m_inputFingerprint;
    std::vector<RenderPassTrace> m_passes;
    std::map<RenderPassId, std::size_t> m_passIndexes;
  };

  /**
    * Thread-safe sink shared by original and cloned graph engines.
    */
  class RenderGraphExecutionTraceRecorder {
  public:
    std::shared_ptr<const RenderGraphExecutionTraceSession>
    begin(RenderPlan plan, std::string inputFingerprint = {});
    void finish(std::shared_ptr<const RenderGraphExecutionTraceSession> session);
    void clear();
    void passStarted(std::shared_ptr<const RenderGraphExecutionTraceSession> session,
                     const RenderPassNode& pass, const RenderResourceStorage& storage);
    void passCompleted(std::shared_ptr<const RenderGraphExecutionTraceSession> session,
                       const RenderPassNode& pass, const RenderResourceStorage& storage);
    void passSkipped(std::shared_ptr<const RenderGraphExecutionTraceSession> session,
                     const RenderPassNode& pass, const RenderResourceStorage& storage,
                     std::string message);
    void passFailed(std::shared_ptr<const RenderGraphExecutionTraceSession> session,
                    const RenderPassNode& pass, const RenderResourceStorage& storage,
                    std::string message);
    std::shared_ptr<const RenderGraphExecutionTrace> lastTrace() const;

  private:
    bool currentSessionMatches(const RenderGraphExecutionTraceSession& session) const;

    std::vector<RenderGraphResourceSnapshot>
    snapshotsForReads(const RenderGraphExecutionTrace& trace, const RenderPassNode& pass,
                      const RenderResourceStorage& storage) const;
    std::vector<RenderGraphResourceSnapshot>
    snapshotsForWrites(const RenderGraphExecutionTrace& trace, const RenderPassNode& pass,
                       const RenderResourceStorage& storage) const;
    RenderGraphResourceSnapshot snapshotForResource(const RenderGraphExecutionTrace& trace,
                                                    const RenderResourceId& resourceId,
                                                    const RenderResourceStorage& storage) const;
    std::vector<RenderGraphResourceDiff>
    diffsFor(const std::vector<RenderGraphResourceSnapshot>& inputs,
             const std::vector<RenderGraphResourceSnapshot>& outputs) const;

    mutable std::mutex m_mutex;
    std::uint64_t m_nextGeneration{1};
    std::uint64_t m_currentGeneration{0};
    std::shared_ptr<RenderGraphExecutionTrace> m_current;
    std::shared_ptr<const RenderGraphExecutionTrace> m_last;
  };

  class RenderGraphExecutionTraceSession {
  public:
    std::uint64_t generation() const;

  private:
    friend class RenderGraphExecutionTraceRecorder;

    explicit RenderGraphExecutionTraceSession(std::uint64_t generation);

    std::uint64_t m_generation;
  };

  const char* toString(RenderPassExecutionStatus value);
  const char* toString(RenderGraphCacheStatus value);
}
