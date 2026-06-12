#include "render/PathTracingIntegrator.h"

#include "core/math/Constants.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/util/ScopedTimer.h"
#include "render/MIS.h"
#include "render/PathTermination.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/WavefrontFrontierCompaction.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/lights/Light.h"
#include "render/lights/LightSampler.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/samplers/SampleStream.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace render {
  struct PathTracingIntegrator::BatchPath {
    BatchPath(const IntegratorRaySample& sample, Colord& accumulated)
        : ray(sample.ray),
          m_accumulated(&accumulated) {
      this->accumulated() = Colord::black();
      state.timeSample = sample.timeSample;
      state.animationFrame = sample.animationFrame;
      state.animationTime = sample.animationTime;
      state.sampleStream = sample.sampleStream();
    }

    BatchPath(Rayd nextRay, Colord nextThroughput, bool nextBackgroundVisible, State nextState,
              Colord& accumulated, bool nextSampledFromBsdf, double nextBsdfSamplePdf,
              bool nextBsdfSampleDelta)
        : ray(std::move(nextRay)),
          throughput(nextThroughput),
          backgroundVisible(nextBackgroundVisible),
          sampledFromBsdf(nextSampledFromBsdf),
          bsdfSamplePdf(nextBsdfSamplePdf),
          bsdfSampleDelta(nextBsdfSampleDelta),
          state(std::move(nextState)),
          m_accumulated(&accumulated) {
    }

    Colord& accumulated() {
      return *m_accumulated;
    }

    const Colord& accumulated() const {
      return *m_accumulated;
    }

    Rayd ray;
    Colord throughput{Colord::white()};
    bool backgroundVisible{true};
    bool sampledFromBsdf{false};
    double bsdfSamplePdf{0.0};
    bool bsdfSampleDelta{false};
    State state;

  private:
    Colord* m_accumulated;
  };

  class PathTracingIntegrator::HostBatchPathFrontier {
  public:
    class Compaction {
    public:
      void retain(std::size_t pathIndex) {
        m_request.retain(pathIndex);
      }

      [[nodiscard]] const WavefrontFrontierCompactionRequest& request() const {
        return m_request;
      }

    private:
      friend class HostBatchPathFrontier;

      Compaction(std::size_t inputPathCount, std::uint64_t pathStateBytesPerPath)
          : m_request(inputPathCount) {
        m_request.setPathStateBytesPerPath(pathStateBytesPerPath);
      }

      WavefrontFrontierCompactionRequest m_request;
    };

    void reserve(std::size_t pathCount) {
      m_paths.reserve(pathCount);
    }

    void emplacePrimary(const IntegratorRaySample& sample, Colord& accumulated) {
      m_paths.emplace_back(sample, accumulated);
    }

    void emplaceContinuation(Rayd nextRay, Colord nextThroughput, bool nextBackgroundVisible,
                             State nextState, Colord& accumulated, bool nextSampledFromBsdf,
                             double nextBsdfSamplePdf, bool nextBsdfSampleDelta) {
      m_paths.emplace_back(std::move(nextRay), nextThroughput, nextBackgroundVisible,
                           std::move(nextState), accumulated, nextSampledFromBsdf,
                           nextBsdfSamplePdf, nextBsdfSampleDelta);
    }

    void appendAll(HostBatchPathFrontier& frontier) {
      for (auto& path : frontier.m_paths) {
        m_paths.push_back(std::move(path));
      }
      frontier.m_paths.clear();
    }

    [[nodiscard]] bool empty() const {
      return m_paths.empty();
    }

    [[nodiscard]] std::size_t size() const {
      return m_paths.size();
    }

    BatchPath& operator[](std::size_t pathIndex) {
      return m_paths[pathIndex];
    }

    [[nodiscard]] std::uint64_t hostPathStateBytes() const {
      return beginCompaction().request().inputPathStateBytes();
    }

    [[nodiscard]] Compaction beginCompaction() const {
      return Compaction(m_paths.size(), sizeof(BatchPath));
    }

    void applyCompaction(const WavefrontFrontierCompactionResult& compaction) {
      assert(m_paths.size() == compaction.inputPathCount());
      const std::vector<std::size_t>& retainedPathIndices = compaction.retainedPathIndices();
      for (std::size_t outputIndex = 0; outputIndex != retainedPathIndices.size(); ++outputIndex) {
        const std::size_t inputIndex = retainedPathIndices[outputIndex];
        assert(inputIndex < m_paths.size());
        if (inputIndex != outputIndex) {
          m_paths[outputIndex] = std::move(m_paths[inputIndex]);
        }
      }
      while (m_paths.size() != retainedPathIndices.size()) {
        m_paths.pop_back();
      }
    }

    void compactWith(const WavefrontIntersectionBackend& backend, const Compaction& compaction,
                     IntegratorBatchMetrics* metrics) {
      const WavefrontFrontierCompactionResult result =
        backend.compactFrontier(compaction.request());
      applyCompaction(result);
      result.record(metrics);
    }

  private:
    std::vector<BatchPath> m_paths;
  };

  struct PathTracingIntegrator::ScalarPath {
    ScalarPath(const Rayd& primaryRay, State& primaryState)
        : ray(primaryRay),
          state(&primaryState) {
    }

    ScalarPath(Rayd nextRay, Colord nextThroughput, bool nextBackgroundVisible, State nextState,
               bool nextSampledFromBsdf, double nextBsdfSamplePdf, bool nextBsdfSampleDelta)
        : ray(std::move(nextRay)),
          throughput(nextThroughput),
          backgroundVisible(nextBackgroundVisible),
          sampledFromBsdf(nextSampledFromBsdf),
          bsdfSamplePdf(nextBsdfSamplePdf),
          bsdfSampleDelta(nextBsdfSampleDelta),
          ownedState(std::make_unique<State>(std::move(nextState))),
          state(ownedState.get()) {
    }

    ScalarPath(ScalarPath&&) noexcept = default;
    ScalarPath& operator=(ScalarPath&&) noexcept = default;

    State& pathState() {
      return *state;
    }

    Rayd ray;
    Colord throughput{Colord::white()};
    bool backgroundVisible{true};
    bool sampledFromBsdf{false};
    double bsdfSamplePdf{0.0};
    bool bsdfSampleDelta{false};
    std::unique_ptr<State> ownedState;
    State* state{nullptr};
  };

  struct PathTracingIntegrator::BatchHit {
    std::size_t pathIndex{0};
    const Primitive* primitive{nullptr};
    std::shared_ptr<Material> material;
    HitPoint hitPoint;
  };

  class PathTracingIntegrator::ActivePathHits {
  public:
    void reserve(std::size_t count) {
      m_hits.reserve(count);
    }

    void clear() {
      m_hits.clear();
    }

    bool empty() const {
      return m_hits.empty();
    }

    std::size_t size() const {
      return m_hits.size();
    }

    std::uint64_t hostBytes() const {
      return m_hits.size() * sizeof(BatchHit);
    }

    BatchHit& operator[](std::size_t index) {
      return m_hits[index];
    }

    const BatchHit& operator[](std::size_t index) const {
      return m_hits[index];
    }

    void add(std::size_t pathIndex, const Primitive& primitive, const HitPoint& hitPoint) {
      m_hits.push_back(BatchHit{pathIndex, &primitive, primitive.material(), hitPoint});
    }

    void setLastMaterial(std::shared_ptr<Material> material) {
      m_hits.back().material = std::move(material);
    }

  private:
    std::vector<BatchHit> m_hits;
  };

  struct PathTracingIntegrator::DirectLightingCandidate {
    bool valid{false};
    Colord radiance{Colord::black()};
    double lightPdf{0.0};
    Rayd shadowRay{Vector4d(0, 0, 0, 1), Vector3d(0, 0, 1)};
    double shadowDistance{0.0};
    Vector3d wo;
    double normalDotOut{0.0};
    bool delta{false};
  };

  struct PathTracingIntegrator::DirectLightingSelection {
    std::size_t hitIndex{0};
    DirectLightingCandidate candidate;
    double selectionPdf{1.0};
  };

  struct PathTracingIntegrator::DirectLightingSample {
    Colord contribution{Colord::black()};
    bool occluded{false};

    bool contributing() const {
      return contribution != Colord::black();
    }
  };

  class PathTracingIntegrator::DirectLightVisibilityBatch {
  public:
    explicit DirectLightVisibilityBatch(std::size_t reserveCount) {
      m_selections.reserve(reserveCount);
      m_shadowQueries.reserve(reserveCount);
    }

    void add(std::size_t hitIndex, DirectLightingCandidate candidate, double selectionPdf,
             State& state) {
      m_shadowQueries.push_back(
        WavefrontAnyHitQuery{candidate.shadowRay, candidate.shadowDistance, &state});
      m_selections.push_back(DirectLightingSelection{hitIndex, std::move(candidate), selectionPdf});
    }

    [[nodiscard]] bool empty() const {
      return m_shadowQueries.empty();
    }

    [[nodiscard]] std::size_t size() const {
      return m_selections.size();
    }

    [[nodiscard]] const DirectLightingSelection& selection(std::size_t index) const {
      return m_selections[index];
    }

    [[nodiscard]] bool occluded(std::size_t index) const {
      return index < m_occluded.size() && m_occluded[index] != 0U;
    }

    [[nodiscard]] std::uint64_t hostSelectionBytes() const {
      return static_cast<std::uint64_t>(m_selections.size()) * sizeof(DirectLightingSelection);
    }

    [[nodiscard]] std::uint64_t hostOcclusionBytes() const {
      return static_cast<std::uint64_t>(m_occluded.size()) *
             sizeof(WavefrontOcclusionFlags::value_type);
    }

    void resolveOcclusion(const Scene& scene,
                          const WavefrontIntersectionBackend& intersectionBackend, int bounce,
                          IntegratorBatchMetrics* metrics) {
      WavefrontIntersectionQueryTiming intersectionTiming;
      m_occluded.clear();
      if (metrics) {
        metrics->recordDirectLightSelectionHostBytes(
          static_cast<std::uint64_t>(std::max(0, bounce)), hostSelectionBytes());
      }
      if (intersectionBackend.prefersAnyHitBatch(m_shadowQueries.size())) {
        const std::unique_ptr<WavefrontAnyHitFrontier> frontier =
          intersectionBackend.createAnyHitFrontier(std::move(m_shadowQueries));
        m_occluded = intersectionBackend.intersectAnyFrontier(
          scene, *frontier, metrics ? &intersectionTiming : nullptr);
        if (metrics) {
          recordDirectLightChunks(bounce, /*batchChunks=*/1, frontier->rayCount(),
                                  frontier->packedRayBytes(), frontier->hostQueryBytes(),
                                  frontier->stateHandleBytes(), metrics);
          metrics->recordAnyHitFrontierResidency(frontier->residency(), frontier->packedRayBytes(),
                                                 frontier->hostQueryBytes(),
                                                 frontier->stateHandleBytes());
          metrics->recordAnyHitQuery(intersectionBackend, frontier->rayCount(), intersectionTiming);
          metrics->recordDirectLightOcclusionHostBytes(
            static_cast<std::uint64_t>(std::max(0, bounce)), hostOcclusionBytes());
        }
        return;
      }

      m_occluded.reserve(m_shadowQueries.size());
      if (metrics) {
        const std::uint64_t queryCount = static_cast<std::uint64_t>(m_shadowQueries.size());
        recordDirectLightChunks(bounce, queryCount, queryCount, /*packedRayBytes=*/0,
                                queryCount * sizeof(WavefrontAnyHitQuery),
                                /*stateHandleBytes=*/0, metrics);
      }
      for (const WavefrontAnyHitQuery& query : m_shadowQueries) {
        WavefrontIntersectionQueryTiming queryTiming;
        State scratchState;
        State& queryState = query.state ? *query.state : scratchState;
        const bool occluded = intersectionBackend.intersectAny(
          scene, query.ray, query.maxDistance, queryState, metrics ? &queryTiming : nullptr);
        m_occluded.push_back(occluded ? 1U : 0U);
        if (metrics) {
          metrics->recordAnyHitQuery(intersectionBackend, 1, queryTiming);
        }
      }
      if (metrics) {
        metrics->recordDirectLightOcclusionHostBytes(
          static_cast<std::uint64_t>(std::max(0, bounce)), hostOcclusionBytes());
      }
    }

  private:
    static void recordDirectLightChunks(int bounce, std::uint64_t batchChunks,
                                        std::uint64_t batchRays, std::uint64_t packedRayBytes,
                                        std::uint64_t hostQueryBytes,
                                        std::uint64_t stateHandleBytes,
                                        IntegratorBatchMetrics* metrics) {
      metrics->recordDirectLightAnyHitBatch(static_cast<std::uint64_t>(std::max(0, bounce)),
                                            batchChunks, batchRays, packedRayBytes, hostQueryBytes,
                                            stateHandleBytes);
    }

    std::vector<DirectLightingSelection> m_selections;
    std::vector<WavefrontAnyHitQuery> m_shadowQueries;
    WavefrontOcclusionFlags m_occluded;
  };

  class PathTracingIntegrator::DirectLightContributionBatch {
  public:
    explicit DirectLightContributionBatch(std::size_t count)
        : m_contributions(count, Colord::black()) {
    }

    [[nodiscard]] std::size_t size() const {
      return m_contributions.size();
    }

    [[nodiscard]] bool empty() const {
      return m_contributions.empty();
    }

    [[nodiscard]] std::uint64_t hostBytes() const {
      return static_cast<std::uint64_t>(m_contributions.size()) * sizeof(Colord);
    }

    [[nodiscard]] Colord at(std::size_t index) const {
      return index < m_contributions.size() ? m_contributions[index] : Colord::black();
    }

    void addWeighted(std::size_t index, const Colord& contribution, double weight) {
      if (index >= m_contributions.size()) {
        return;
      }
      m_contributions[index] += contribution * weight;
    }

    void average(int sampleCount) {
      const auto divisor = static_cast<double>(std::max(1, sampleCount));
      for (Colord& contribution : m_contributions) {
        contribution = contribution / divisor;
      }
    }

  private:
    std::vector<Colord> m_contributions;
  };

  struct PathTracingIntegrator::BatchDepthMetrics {
    bool trackRadianceDelta{false};
    std::uint64_t frontierRayHits{0};
    std::uint64_t frontierRayMisses{0};
    std::uint64_t frontierPacketChunks{0};
    std::uint64_t frontierPacketRays{0};
    std::uint64_t frontierClosestHitBatchChunks{0};
    std::uint64_t frontierClosestHitBatchRays{0};
    std::uint64_t frontierRay4PacketChunks{0};
    std::uint64_t frontierRay8PacketChunks{0};
    std::uint64_t frontierScalarRays{0};
    std::uint64_t frontierPacketScalarFallbackRays{0};
    std::map<std::string, std::uint64_t> frontierPacketScalarFallbackRaysByReason;
    double depthDeltaSquaredSum{0.0};
    double depthMaxDelta{0.0};
    IntegratorBatchMetrics* metrics{nullptr};

    bool trackFrontierMetrics() const {
      return metrics != nullptr;
    }

    void recordPacketScalarFallbacks(const State& state,
                                     const std::map<std::string, std::uint64_t>& reasonsBefore) {
      for (const auto& [reason, count] : state.packetHitScalarFallbacksByReason) {
        const auto before = reasonsBefore.find(reason);
        const std::uint64_t previous = before == reasonsBefore.end() ? 0 : before->second;
        if (count > previous) {
          const std::uint64_t delta = count - previous;
          frontierPacketScalarFallbackRays += delta;
          frontierPacketScalarFallbackRaysByReason[reason] += delta;
        }
      }
    }
  };

  class PathTracingIntegrator::ClosestHitPathFrontierBatch {
  public:
    ClosestHitPathFrontierBatch(HostBatchPathFrontier& paths, BatchDepthMetrics& depthMetrics,
                                IntegratorBatchMetrics* metrics) {
      m_queries.reserve(paths.size());
      if (depthMetrics.trackRadianceDelta) {
        m_accumulatedBeforeDepths.resize(paths.size());
      }

      {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        if (depthMetrics.trackFrontierMetrics()) {
          ++depthMetrics.frontierClosestHitBatchChunks;
          depthMetrics.frontierClosestHitBatchRays += paths.size();
        }
        for (std::size_t pathIndex = 0; pathIndex != paths.size(); ++pathIndex) {
          BatchPath& path = paths[pathIndex];
          if (depthMetrics.trackRadianceDelta) {
            m_accumulatedBeforeDepths[pathIndex] = path.accumulated();
          }
          path.state.recurseIn();
          m_queries.push_back(WavefrontClosestHitQuery{path.ray, &path.state});
        }
      }
    }

    void intersect(const Scene& scene, const WavefrontIntersectionBackend& intersectionBackend,
                   IntegratorBatchMetrics* metrics) {
      WavefrontIntersectionQueryTiming intersectionTiming;
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      m_frontier = intersectionBackend.createClosestHitFrontier(std::move(m_queries));
      m_hits =
        intersectionBackend.intersectClosestFrontier(scene, *m_frontier, &intersectionTiming);
      if (metrics) {
        metrics->recordClosestHitFrontierResidency(
          m_frontier->residency(), m_frontier->packedRayBytes(), m_frontier->hostQueryBytes(),
          m_frontier->stateHandleBytes());
        metrics->recordClosestHitQuery(intersectionBackend, m_frontier->rayCount(),
                                       intersectionTiming);
      }
    }

    [[nodiscard]] Colord accumulatedBeforeDepth(std::size_t pathIndex,
                                                const BatchDepthMetrics& depthMetrics) const {
      return depthMetrics.trackRadianceDelta ? m_accumulatedBeforeDepths[pathIndex]
                                             : Colord::black();
    }

    [[nodiscard]] const WavefrontClosestHitResult* hit(std::size_t pathIndex) const {
      if (pathIndex >= m_hits.size() || !m_hits[pathIndex].hit()) {
        return nullptr;
      }
      return &m_hits[pathIndex];
    }

  private:
    std::vector<WavefrontClosestHitQuery> m_queries;
    std::vector<Colord> m_accumulatedBeforeDepths;
    std::unique_ptr<WavefrontClosestHitFrontier> m_frontier;
    std::vector<WavefrontClosestHitResult> m_hits;
  };

  PathTracingIntegrator::PathTracingIntegrator() = default;

  std::unique_ptr<Integrator> PathTracingIntegrator::clone() const {
    auto result = std::make_unique<PathTracingIntegrator>();
    result->setMaximumRecursionDepth(m_maximumRecursionDepth);
    result->setRussianRouletteDepth(m_russianRouletteDepth);
    result->setDirectLightSamples(m_directLightSamples);
    result->setCancellationCallback(m_cancellationCallback);
    return result;
  }

  const char* PathTracingIntegrator::diagnosticName() const {
    return "pathtracer";
  }

  const char* PathTracingIntegrator::batchExecutionMode() const {
    return "depth_major_paths";
  }

  bool PathTracingIntegrator::prefersProgressiveSamplePublishing() const {
    return true;
  }

  std::uint64_t PathTracingIntegrator::estimatedIntersectionRaysPerPrimarySample() const {
    return estimatedClosestHitRaysPerPrimarySample() + estimatedAnyHitRaysPerPrimarySample();
  }

  std::uint64_t PathTracingIntegrator::estimatedClosestHitRaysPerPrimarySample() const {
    return static_cast<std::uint64_t>(std::max(1, m_maximumRecursionDepth));
  }

  std::uint64_t PathTracingIntegrator::estimatedAnyHitRaysPerPrimarySample() const {
    return estimatedClosestHitRaysPerPrimarySample() *
           static_cast<std::uint64_t>(std::max(1, m_directLightSamples));
  }

  void PathTracingIntegrator::setCancellationCallback(CancellationCallback callback) {
    m_cancellationCallback = std::move(callback);
  }

  bool PathTracingIntegrator::isCancelled() const {
    return m_cancellationCallback && m_cancellationCallback();
  }

  Colord PathTracingIntegrator::missRadiance(const Scene& scene, bool backgroundVisible) const {
    return backgroundVisible ? scene.background() : scene.environmentRadiance();
  }

  double PathTracingIntegrator::lightSelectionSample(State& state, int bounce,
                                                     int directSampleIndex) const {
    return state.sampleStream->sample1D(
      SampleDimension::LightSelection,
      SampleStream::lightSelectionSampleIndex(static_cast<std::uint64_t>(bounce),
                                              static_cast<std::uint64_t>(directSampleIndex)));
  }

  Vector2d PathTracingIntegrator::lightSample(State& state, int bounce, std::size_t lightIndex,
                                              int directSampleIndex) const {
    return state.sampleStream->sample2D(
      SampleDimension::Light,
      SampleStream::lightSampleIndex(static_cast<std::uint64_t>(bounce),
                                     static_cast<std::uint64_t>(lightIndex),
                                     static_cast<std::uint64_t>(directSampleIndex)));
  }

  PathTracingIntegrator::DirectLightingCandidate
  PathTracingIntegrator::directLightingCandidate(const Light& light, const HitPoint& hitPoint,
                                                 const Vector2d& lightSample) const {
    LightSample sample = light.sample(hitPoint.point(), lightSample);
    if (sample.pdf <= 0.0 || sample.radiance == Colord::black()) {
      return {};
    }

    const Vector3d wo = sample.direction;
    const double normalDotOut = hitPoint.normal() * wo;
    if (normalDotOut <= 0.0) {
      return {};
    }

    DirectLightingCandidate candidate;
    candidate.valid = true;
    candidate.radiance = sample.radiance;
    candidate.lightPdf = sample.pdf;
    candidate.shadowRay = Rayd(hitPoint.point(), wo).epsilonShifted();
    candidate.shadowDistance = sample.distance;
    candidate.wo = wo;
    candidate.normalDotOut = normalDotOut;
    candidate.delta = sample.delta;
    return candidate;
  }

  Colord PathTracingIntegrator::sampleDirectLighting(
    const Scene& scene, const LightSampler& lightSampler, const HitPoint& hitPoint,
    const PathMaterialTransport& material, const Vector3d& wi, State& state, int bounce,
    const WavefrontIntersectionBackend* intersectionBackend,
    IntegratorBatchMetrics* metrics) const {
    const WavefrontIntersectionBackend& resolvedIntersectionBackend =
      intersectionBackend ? *intersectionBackend : CpuWavefrontIntersectionBackend::instance();
    DirectLightVisibilityBatch visibilityBatch(static_cast<std::size_t>(m_directLightSamples));

    for (int sampleIndex = 0; sampleIndex != m_directLightSamples; ++sampleIndex) {
      const LightSampler::Selection selection =
        lightSampler.select(lightSelectionSample(state, bounce, sampleIndex));
      if (!selection) {
        continue;
      }

      DirectLightingCandidate candidate = directLightingCandidate(
        *selection.light, hitPoint, lightSample(state, bounce, selection.lightIndex, sampleIndex));
      if (!candidate.valid) {
        continue;
      }

      visibilityBatch.add(/*hitIndex=*/0, std::move(candidate), selection.pdf, state);
    }

    if (visibilityBatch.empty()) {
      return Colord::black();
    }

    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      visibilityBatch.resolveOcclusion(scene, resolvedIntersectionBackend, bounce, metrics);
    }

    Colord contribution = Colord::black();
    for (std::size_t index = 0; index != visibilityBatch.size(); ++index) {
      const DirectLightingSelection& selection = visibilityBatch.selection(index);
      const DirectLightingSample sample = resolveDirectLightingCandidate(
        selection.candidate, material, hitPoint, wi, visibilityBatch.occluded(index), state);
      if (metrics) {
        metrics->recordDirectLightSample(sample.occluded, sample.contributing());
      }
      contribution += sample.contribution / selection.selectionPdf;
    }
    return contribution / static_cast<double>(m_directLightSamples);
  }

  PathTracingIntegrator::DirectLightContributionBatch
  PathTracingIntegrator::sampleDirectLightingBatch(
    const Scene& scene, const LightSampler& lightSampler, const ActivePathHits& activeHits,
    HostBatchPathFrontier& paths, int bounce,
    const WavefrontIntersectionBackend& intersectionBackend,
    IntegratorBatchMetrics* metrics) const {
    DirectLightContributionBatch contributions(activeHits.size());
    if (metrics) {
      metrics->recordDirectLightContributionHostBytes(
        static_cast<std::uint64_t>(std::max(0, bounce)), contributions.hostBytes());
    }
    if (activeHits.empty()) {
      return contributions;
    }

    DirectLightVisibilityBatch visibilityBatch(activeHits.size() *
                                               static_cast<std::size_t>(m_directLightSamples));

    {
      core::util::ScopedTimer timer(metrics ? &metrics->shadingWorkerSeconds : nullptr);
      for (std::size_t hitIndex = 0; hitIndex != activeHits.size(); ++hitIndex) {
        const BatchHit& hit = activeHits[hitIndex];
        BatchPath& path = paths[hit.pathIndex];
        const auto material = hit.material ? hit.material : hit.primitive->material();
        if (!material) {
          continue;
        }
        const PathMaterialTransport& transport = material->pathTransport();
        if (!transport.supportsPathTracing()) {
          continue;
        }

        for (int sampleIndex = 0; sampleIndex != m_directLightSamples; ++sampleIndex) {
          const LightSampler::Selection selection =
            lightSampler.select(lightSelectionSample(path.state, bounce, sampleIndex));
          if (!selection) {
            continue;
          }

          DirectLightingCandidate candidate = directLightingCandidate(
            *selection.light, hit.hitPoint,
            lightSample(path.state, bounce, selection.lightIndex, sampleIndex));
          if (!candidate.valid) {
            continue;
          }

          visibilityBatch.add(hitIndex, std::move(candidate), selection.pdf, path.state);
        }
      }
    }

    if (visibilityBatch.empty()) {
      return contributions;
    }

    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      visibilityBatch.resolveOcclusion(scene, intersectionBackend, bounce, metrics);
    }

    {
      core::util::ScopedTimer timer(metrics ? &metrics->shadingWorkerSeconds : nullptr);
      for (std::size_t selectionIndex = 0; selectionIndex != visibilityBatch.size();
           ++selectionIndex) {
        const DirectLightingSelection& selection = visibilityBatch.selection(selectionIndex);
        const BatchHit& hit = activeHits[selection.hitIndex];
        BatchPath& path = paths[hit.pathIndex];
        const auto material = hit.material ? hit.material : hit.primitive->material();
        if (!material) {
          continue;
        }

        const PathMaterialTransport& transport = material->pathTransport();
        const Vector3d wi = -path.ray.direction().normalized();
        const DirectLightingSample sample =
          resolveDirectLightingCandidate(selection.candidate, transport, hit.hitPoint, wi,
                                         visibilityBatch.occluded(selectionIndex), path.state);
        if (metrics) {
          metrics->recordDirectLightSample(sample.occluded, sample.contributing());
        }
        contributions.addWeighted(selection.hitIndex, sample.contribution,
                                  1.0 / selection.selectionPdf);
      }
    }

    contributions.average(m_directLightSamples);
    return contributions;
  }

  bool PathTracingIntegrator::canContinueWithSample(const MaterialBsdfSample& sample,
                                                    const HitPoint& hitPoint) const {
    if (sample.pdf <= 0.0 || sample.value == Colord::black()) {
      return false;
    }

    const double normalDotWo = hitPoint.normal() * sample.direction;
    return sample.isDelta || normalDotWo > 0.0;
  }

  Colord PathTracingIntegrator::continuedThroughput(const Colord& throughput,
                                                    const MaterialBsdfSample& sample,
                                                    const HitPoint& hitPoint) const {
    if (sample.isDelta) {
      return throughput * sample.value;
    }

    const double normalDotWo = hitPoint.normal() * sample.direction;
    return throughput * (sample.value * (normalDotWo / sample.pdf));
  }

  bool PathTracingIntegrator::continuesExactDeltaBranch(const Colord& throughput) const {
    return throughput.max() >= RAYTRACER_THROUGHPUT_CUTOFF;
  }

  void PathTracingIntegrator::setStateThroughput(State& state, const Colord& throughput) const {
    state.throughput = throughput.max();
  }

  void PathTracingIntegrator::recordUnsupportedPathMaterial(State& state,
                                                            IntegratorBatchMetrics* metrics) const {
    state.recordEvent(nullptr,
                      "PathTracing: material does not support path tracing; terminating path");
    if (metrics) {
      metrics->recordUnsupportedPathMaterial();
    }
  }

  bool PathTracingIntegrator::survivesRussianRoulette(Colord& throughput, State& state,
                                                      int bounce) const {
    if (bounce < m_russianRouletteDepth) {
      setStateThroughput(state, throughput);
      return true;
    }

    const double roulette = state.sampleStream->sample1D(SampleDimension::Continuation,
                                                         static_cast<std::uint64_t>(bounce));
    const PathContinuation continuation = pathContinuation(throughput, roulette);
    throughput = render::continuedThroughput(throughput, continuation);
    setStateThroughput(state, throughput);
    if (!continuation.continues) {
      return false;
    }

    return true;
  }

  bool PathTracingIntegrator::prepareSampledContinuation(
    const MaterialBsdfSample& sample, const HitPoint& hitPoint, Colord& throughput,
    bool& backgroundVisible, bool& sampledFromBsdf, double& bsdfSamplePdf, bool& bsdfSampleDelta,
    State& state, int bounce) const {
    if (!canContinueWithSample(sample, hitPoint)) {
      return false;
    }

    throughput = continuedThroughput(throughput, sample, hitPoint);
    if (!sample.isDelta) {
      backgroundVisible = false;
    }
    sampledFromBsdf = true;
    bsdfSamplePdf = sample.pdf;
    bsdfSampleDelta = sample.isDelta;

    return survivesRussianRoulette(throughput, state, bounce);
  }

  void PathTracingIntegrator::recordDepthDelta(BatchDepthMetrics& depthMetrics,
                                               const Colord& before, const Colord& after) const {
    if (!depthMetrics.trackRadianceDelta) {
      return;
    }

    const double deltaSquared = radianceDeltaSquared(before, after);
    depthMetrics.depthDeltaSquaredSum += deltaSquared;
    if (depthMetrics.metrics) {
      depthMetrics.depthMaxDelta = std::max(depthMetrics.depthMaxDelta, std::sqrt(deltaSquared));
    }
  }

  void PathTracingIntegrator::recordFrontierHit(std::size_t pathIndex, BatchPath& path,
                                                const Primitive& primitive,
                                                const HitPoint& hitPoint, int bounce,
                                                BatchDepthMetrics& depthMetrics,
                                                ActivePathHits& activeHits) const {
    if (depthMetrics.trackFrontierMetrics()) {
      ++depthMetrics.frontierRayHits;
    }
    if (bounce == 0) {
      path.state.hitPoint = hitPoint;
    }
    activeHits.add(pathIndex, primitive, hitPoint);
  }

  void PathTracingIntegrator::recordFrontierMiss(const Scene& scene, BatchPath& path,
                                                 BatchDepthMetrics& depthMetrics,
                                                 const Colord& accumulatedBeforeDepth) const {
    if (depthMetrics.trackFrontierMetrics()) {
      ++depthMetrics.frontierRayMisses;
    }
    const Colord contribution = path.throughput * missRadiance(scene, path.backgroundVisible);
    path.accumulated() += contribution;
    if (depthMetrics.metrics) {
      depthMetrics.metrics->recordMissRadiance(contribution);
    }
    path.state.recurseOut();
    recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
  }

  void PathTracingIntegrator::intersectActivePathScalar(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    std::size_t pathIndex, HostBatchPathFrontier& paths, ActivePathHits& activeHits, int bounce,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    auto& path = paths[pathIndex];
    const Colord accumulatedBeforeDepth =
      depthMetrics.trackRadianceDelta ? path.accumulated() : Colord::black();

    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      path.state.recurseIn();
      if (depthMetrics.trackFrontierMetrics()) {
        ++depthMetrics.frontierScalarRays;
      }
    }

    WavefrontClosestHitResult hit;
    {
      WavefrontIntersectionQueryTiming intersectionTiming;
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      hit = intersectionBackend.intersectClosestResult(scene, path.ray, path.state,
                                                       &intersectionTiming);
      if (metrics) {
        metrics->recordClosestHitQuery(intersectionBackend, 1, intersectionTiming);
      }
    }

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    if (!hit.hit()) {
      recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepth);
      return;
    }

    recordFrontierHit(pathIndex, path, *hit.primitive, hit.hitPoint, bounce, depthMetrics,
                      activeHits);
    activeHits.setLastMaterial(hit.material);
  }

  void PathTracingIntegrator::intersectActivePathPacket(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    std::size_t firstPathIndex, std::size_t laneCount, HostBatchPathFrontier& paths,
    ActivePathHits& activeHits, int bounce, BatchDepthMetrics& depthMetrics,
    IntegratorBatchMetrics* metrics) const {
    if (laneCount > Ray4::lanes) {
      throw std::logic_error("Ray4 path packet lane count exceeds packet width");
    }
    const std::size_t activeLaneCount = laneCount;
    const std::size_t packetLaneCount = activeLaneCount;
    std::array<Rayd, Ray4::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined};
    std::array<Colord, Ray4::lanes> accumulatedBeforeDepths;
    std::optional<std::array<std::map<std::string, std::uint64_t>, Ray4::lanes>>
      packetFallbacksBefore;
    PrimitivePacketState4 states{};
    assert(laneCount <= Ray4::lanes);

    PrimitivePacketHit4 packetHits;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (depthMetrics.trackFrontierMetrics()) {
        ++depthMetrics.frontierPacketChunks;
        ++depthMetrics.frontierRay4PacketChunks;
        depthMetrics.frontierPacketRays += activeLaneCount;
        packetFallbacksBefore.emplace();
      }
      for (std::size_t lane = 0; lane < Ray4::lanes && lane < laneCount; ++lane) {
        const std::size_t pathIndex = firstPathIndex + lane;
        auto& path = paths[pathIndex];
        if (depthMetrics.trackRadianceDelta) {
          accumulatedBeforeDepths[lane] = path.accumulated();
        }
        path.state.recurseIn();
        if (depthMetrics.trackFrontierMetrics()) {
          (*packetFallbacksBefore)[lane] = path.state.packetHitScalarFallbacksByReason;
        }
        rays[lane] = path.ray;
        states[lane] = &path.state;
      }
    }

    {
      WavefrontIntersectionQueryTiming intersectionTiming;
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits =
        intersectionBackend.intersectPacketClosest(scene, Ray4(rays), states, &intersectionTiming);
      if (metrics) {
        metrics->recordClosestHitQuery(intersectionBackend, packetLaneCount, intersectionTiming);
      }
    }

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
      const std::size_t pathIndex = firstPathIndex + lane;
      auto& path = paths[pathIndex];
      if (depthMetrics.trackFrontierMetrics()) {
        depthMetrics.recordPacketScalarFallbacks(path.state, (*packetFallbacksBefore)[lane]);
      }
      if (!packetHits.hit(lane)) {
        recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepths[lane]);
        continue;
      }

      recordFrontierHit(pathIndex, path, *packetHits.primitive(lane), packetHits.hitPoint(lane),
                        bounce, depthMetrics, activeHits);
    }
  }

  void PathTracingIntegrator::intersectActivePathPacket8(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    std::size_t firstPathIndex, std::size_t laneCount, HostBatchPathFrontier& paths,
    ActivePathHits& activeHits, int bounce, BatchDepthMetrics& depthMetrics,
    IntegratorBatchMetrics* metrics) const {
    if (laneCount > Ray8::lanes) {
      throw std::logic_error("Ray8 path packet lane count exceeds packet width");
    }
    const std::size_t activeLaneCount = laneCount;
    const std::size_t packetLaneCount = activeLaneCount;
    std::array<Rayd, Ray8::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined};
    std::array<Colord, Ray8::lanes> accumulatedBeforeDepths;
    std::optional<std::array<std::map<std::string, std::uint64_t>, Ray8::lanes>>
      packetFallbacksBefore;
    PrimitivePacketState8 states{};
    assert(laneCount <= Ray8::lanes);

    PrimitivePacketHit8 packetHits;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (depthMetrics.trackFrontierMetrics()) {
        ++depthMetrics.frontierPacketChunks;
        ++depthMetrics.frontierRay8PacketChunks;
        depthMetrics.frontierPacketRays += activeLaneCount;
        packetFallbacksBefore.emplace();
      }
      for (std::size_t lane = 0; lane < Ray8::lanes && lane < laneCount; ++lane) {
        const std::size_t pathIndex = firstPathIndex + lane;
        auto& path = paths[pathIndex];
        if (depthMetrics.trackRadianceDelta) {
          accumulatedBeforeDepths[lane] = path.accumulated();
        }
        path.state.recurseIn();
        if (depthMetrics.trackFrontierMetrics()) {
          (*packetFallbacksBefore)[lane] = path.state.packetHitScalarFallbacksByReason;
        }
        rays[lane] = path.ray;
        states[lane] = &path.state;
      }
    }

    {
      WavefrontIntersectionQueryTiming intersectionTiming;
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits =
        intersectionBackend.intersectPacketClosest(scene, Ray8(rays), states, &intersectionTiming);
      if (metrics) {
        metrics->recordClosestHitQuery(intersectionBackend, packetLaneCount, intersectionTiming);
      }
    }

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
      const std::size_t pathIndex = firstPathIndex + lane;
      auto& path = paths[pathIndex];
      if (depthMetrics.trackFrontierMetrics()) {
        depthMetrics.recordPacketScalarFallbacks(path.state, (*packetFallbacksBefore)[lane]);
      }
      if (!packetHits.hit(lane)) {
        recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepths[lane]);
        continue;
      }

      recordFrontierHit(pathIndex, path, *packetHits.primitive(lane), packetHits.hitPoint(lane),
                        bounce, depthMetrics, activeHits);
    }
  }

  void PathTracingIntegrator::intersectActiveFrontierBatch(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    HostBatchPathFrontier& paths, ActivePathHits& activeHits, int bounce,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    ClosestHitPathFrontierBatch frontier(paths, depthMetrics, metrics);
    frontier.intersect(scene, intersectionBackend, metrics);

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    for (std::size_t pathIndex = 0; pathIndex != paths.size(); ++pathIndex) {
      BatchPath& path = paths[pathIndex];
      const Colord accumulatedBeforeDepth =
        frontier.accumulatedBeforeDepth(pathIndex, depthMetrics);
      const WavefrontClosestHitResult* hit = frontier.hit(pathIndex);
      if (!hit) {
        recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepth);
        continue;
      }

      recordFrontierHit(pathIndex, path, *hit->primitive, hit->hitPoint, bounce, depthMetrics,
                        activeHits);
      activeHits.setLastMaterial(hit->material);
    }
  }

  void PathTracingIntegrator::intersectActiveFrontier(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    HostBatchPathFrontier& paths, ActivePathHits& activeHits, int bounce,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    activeHits.clear();
    if (intersectionBackend.prefersClosestHitBatch(paths.size())) {
      intersectActiveFrontierBatch(intersectionBackend, scene, paths, activeHits, bounce,
                                   depthMetrics, metrics);
      return;
    }

    std::size_t activeIndex = 0;
    while (activeIndex != paths.size()) {
      if (activeIndex + Ray8::lanes <= paths.size()) {
        intersectActivePathPacket8(intersectionBackend, scene, activeIndex, Ray8::lanes, paths,
                                   activeHits, bounce, depthMetrics, metrics);
        activeIndex += Ray8::lanes;
        continue;
      }

      const std::size_t remainingPaths = paths.size() - activeIndex;
      if (remainingPaths > Ray4::lanes) {
        intersectActivePathPacket8(intersectionBackend, scene, activeIndex, remainingPaths, paths,
                                   activeHits, bounce, depthMetrics, metrics);
        activeIndex += remainingPaths;
        continue;
      }

      if (activeIndex + Ray4::lanes <= paths.size()) {
        intersectActivePathPacket(intersectionBackend, scene, activeIndex, Ray4::lanes, paths,
                                  activeHits, bounce, depthMetrics, metrics);
        activeIndex += Ray4::lanes;
        continue;
      }

      if (remainingPaths > 1) {
        intersectActivePathPacket(intersectionBackend, scene, activeIndex, remainingPaths, paths,
                                  activeHits, bounce, depthMetrics, metrics);
        activeIndex += remainingPaths;
        continue;
      }

      intersectActivePathScalar(intersectionBackend, scene, activeIndex, paths, activeHits, bounce,
                                depthMetrics, metrics);
      ++activeIndex;
    }
  }

  Colord PathTracingIntegrator::emittedRadiance(const LightSampler& lightSampler,
                                                const PathMaterialTransport& material,
                                                const Rayd& ray, const HitPoint& hitPoint,
                                                bool sampledFromBsdf, double bsdfSamplePdf,
                                                bool bsdfSampleDelta,
                                                IntegratorBatchMetrics* metrics) const {
    const Colord emitted = material.emittedRadiance(ray, hitPoint);
    if (emitted == Colord::black()) {
      return emitted;
    }

    const bool misWeighted = sampledFromBsdf && !bsdfSampleDelta;
    if (metrics) {
      metrics->recordEmitterHit(sampledFromBsdf, bsdfSampleDelta, misWeighted);
    }
    if (!misWeighted) {
      return emitted;
    }

    const double lightPdf = lightSampler.pdf(Vector3d(ray.origin()), ray.direction().normalized());
    return emitted * mis::weight(mis::Heuristic::Power, bsdfSamplePdf, lightPdf);
  }

  PathTracingIntegrator::DirectLightingSample PathTracingIntegrator::resolveDirectLightingCandidate(
    const DirectLightingCandidate& candidate, const PathMaterialTransport& material,
    const HitPoint& hitPoint, const Vector3d& wi, bool occluded, State& state) const {
    if (occluded) {
      state.shadowHit(nullptr, "PathTracingIntegrator");
      return {Colord::black(), true};
    }
    state.shadowMiss(nullptr, "PathTracingIntegrator");

    const Colord bsdfValue = material.evalBsdf(hitPoint, wi, candidate.wo);
    if (bsdfValue == Colord::black()) {
      return {};
    }

    const double bsdfPdf = candidate.delta ? 0.0 : material.bsdfPdf(hitPoint, wi, candidate.wo);
    return {mis::estimateDirectLightingFromLightSample(bsdfValue, candidate.radiance,
                                                       candidate.normalDotOut, candidate.lightPdf,
                                                       bsdfPdf, candidate.delta),
            false};
  }

  Colord PathTracingIntegrator::radiance(const Scene& scene, const Rayd& primaryRay, State& state,
                                         const RayCaster& recursiveRayCaster) const {
    if (state.sampleStream == nullptr) {
      state.recordEvent(nullptr,
                        "PathTracing: no sample stream on state — falling back to Whitted");
      // Recursive Whitted-style entry. Lets unit tests that construct a
      // bare `State{}` still get a useful color out of the integrator
      // for smoke purposes, even though they aren't really sampling.
      return recursiveRayCaster.rayColor(primaryRay, state);
    }

    Colord accumulated = Colord::black();
    const LightSampler lightSampler(scene.lights());
    std::vector<ScalarPath> paths;
    paths.emplace_back(primaryRay, state);

    for (int bounce = 0; bounce < m_maximumRecursionDepth && !paths.empty(); ++bounce) {
      std::vector<ScalarPath> nextPaths;
      nextPaths.reserve(paths.size() * 2);

      for (auto& path : paths) {
        if (isCancelled()) {
          return accumulated;
        }

        State& pathState = path.pathState();
        pathState.recurseIn();

        HitPointInterval hitPoints;
        const Primitive* primitive = scene.intersect(path.ray, hitPoints, pathState);
        if (!primitive) {
          accumulated += path.throughput * missRadiance(scene, path.backgroundVisible);
          pathState.recurseOut();
          continue;
        }

        const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
        if (bounce == 0) {
          pathState.hitPoint = hitPoint;
        }

        const auto material = primitive->material();
        if (!material) {
          pathState.recurseOut();
          continue;
        }
        const PathMaterialTransport& transport = material->pathTransport();

        accumulated += path.throughput * emittedRadiance(lightSampler, transport, path.ray,
                                                         hitPoint, path.sampledFromBsdf,
                                                         path.bsdfSamplePdf, path.bsdfSampleDelta);

        // wi is the direction back along the incoming ray, pointing
        // AWAY from the surface — matches the BSDF convention.
        const Vector3d wi = -path.ray.direction().normalized();

        if (!transport.supportsPathTracing()) {
          recordUnsupportedPathMaterial(pathState);
          pathState.recurseOut();
          continue;
        }

        accumulated += path.throughput * transport.ambientRadiance(scene, path.ray, hitPoint);

        // Direct lighting via NEE.
        accumulated +=
          path.throughput * sampleDirectLighting(scene, lightSampler, hitPoint, transport, wi,
                                                 pathState, bounce, nullptr);

        const std::vector<MaterialBsdfSample> deltaSamples =
          transport.deltaBsdfSamples(hitPoint, wi);
        if (!deltaSamples.empty()) {
          State baseState = pathState.cloneForPathContinuation();
          for (const MaterialBsdfSample& sampled : deltaSamples) {
            if (!canContinueWithSample(sampled, hitPoint)) {
              continue;
            }

            Colord nextThroughput = continuedThroughput(path.throughput, sampled, hitPoint);
            if (!continuesExactDeltaBranch(nextThroughput)) {
              continue;
            }
            State childState = baseState;
            setStateThroughput(childState, nextThroughput);
            nextPaths.emplace_back(sampled.rayFrom(hitPoint), nextThroughput,
                                   path.backgroundVisible, std::move(childState),
                                   /*nextSampledFromBsdf=*/true, sampled.pdf,
                                   /*nextBsdfSampleDelta=*/true);
          }
          pathState.recurseOut();
          continue;
        }

        // Indirect: sample a continuation direction.
        const Vector2d bsdfSample = pathState.sampleStream->sample2D(
          SampleDimension::BSDF, static_cast<std::uint64_t>(bounce));
        const MaterialBsdfSample sampled = transport.sampleBsdf(hitPoint, wi, bsdfSample);
        Colord nextThroughput = path.throughput;
        bool nextBackgroundVisible = path.backgroundVisible;
        bool nextSampledFromBsdf = false;
        double nextBsdfSamplePdf = 0.0;
        bool nextBsdfSampleDelta = false;
        if (!prepareSampledContinuation(sampled, hitPoint, nextThroughput, nextBackgroundVisible,
                                        nextSampledFromBsdf, nextBsdfSamplePdf, nextBsdfSampleDelta,
                                        pathState, bounce)) {
          pathState.recurseOut();
          continue;
        }

        State childState = pathState.cloneForPathContinuation();
        pathState.recurseOut();
        nextPaths.emplace_back(sampled.rayFrom(hitPoint), nextThroughput, nextBackgroundVisible,
                               std::move(childState), nextSampledFromBsdf, nextBsdfSamplePdf,
                               nextBsdfSampleDelta);
      }

      paths = std::move(nextPaths);
    }

    return accumulated;
  }

  bool PathTracingIntegrator::shadeActiveHit(
    const Scene& scene, const LightSampler& lightSampler, const ActivePathHits& activeHits,
    std::size_t hitIndex, HostBatchPathFrontier& paths, HostBatchPathFrontier& spawnedPaths,
    const DirectLightContributionBatch& directLightContributions, int bounce,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    const auto& hit = activeHits[hitIndex];
    auto& path = paths[hit.pathIndex];
    const Colord accumulatedBeforeDepth =
      depthMetrics.trackRadianceDelta ? path.accumulated() : Colord::black();

    core::util::ScopedTimer timer(metrics ? &metrics->shadingWorkerSeconds : nullptr);
    const auto material = hit.material ? hit.material : hit.primitive->material();
    if (!material) {
      path.state.recurseOut();
      recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
      return false;
    }
    const PathMaterialTransport& transport = material->pathTransport();

    const Colord emittedContribution =
      path.throughput * emittedRadiance(lightSampler, transport, path.ray, hit.hitPoint,
                                        path.sampledFromBsdf, path.bsdfSamplePdf,
                                        path.bsdfSampleDelta, metrics);
    path.accumulated() += emittedContribution;
    if (metrics) {
      metrics->recordEmittedRadiance(emittedContribution);
    }

    const Vector3d wi = -path.ray.direction().normalized();
    if (!transport.supportsPathTracing()) {
      recordUnsupportedPathMaterial(path.state, metrics);
      path.state.recurseOut();
      recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
      return false;
    }

    const Colord ambientContribution =
      path.throughput * transport.ambientRadiance(scene, path.ray, hit.hitPoint);
    path.accumulated() += ambientContribution;
    if (metrics) {
      metrics->recordAmbientRadiance(ambientContribution);
    }

    const Colord directLightContribution = path.throughput * directLightContributions.at(hitIndex);
    path.accumulated() += directLightContribution;
    if (metrics) {
      metrics->recordDirectLightRadiance(directLightContribution, bounce == 0);
    }

    const std::vector<MaterialBsdfSample> deltaSamples =
      transport.deltaBsdfSamples(hit.hitPoint, wi);
    if (!deltaSamples.empty()) {
      State baseState = path.state.cloneForPathContinuation();
      for (const MaterialBsdfSample& sampled : deltaSamples) {
        if (!canContinueWithSample(sampled, hit.hitPoint)) {
          continue;
        }

        Colord nextThroughput = continuedThroughput(path.throughput, sampled, hit.hitPoint);
        if (!continuesExactDeltaBranch(nextThroughput)) {
          continue;
        }
        State childState = baseState;
        setStateThroughput(childState, nextThroughput);
        spawnedPaths.emplaceContinuation(sampled.rayFrom(hit.hitPoint), nextThroughput,
                                         path.backgroundVisible, std::move(childState),
                                         path.accumulated(), /*nextSampledFromBsdf=*/true,
                                         sampled.pdf, /*nextBsdfSampleDelta=*/true);
      }
      path.state.recurseOut();
      recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
      return false;
    }

    const Vector2d bsdfSample =
      path.state.sampleStream->sample2D(SampleDimension::BSDF, static_cast<std::uint64_t>(bounce));
    const MaterialBsdfSample sampled = transport.sampleBsdf(hit.hitPoint, wi, bsdfSample);
    if (!prepareSampledContinuation(sampled, hit.hitPoint, path.throughput, path.backgroundVisible,
                                    path.sampledFromBsdf, path.bsdfSamplePdf, path.bsdfSampleDelta,
                                    path.state, bounce)) {
      path.state.recurseOut();
      recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
      return false;
    }

    path.ray = sampled.rayFrom(hit.hitPoint);
    path.state.recurseOut();
    recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
    return true;
  }

  std::vector<Colord> PathTracingIntegrator::radianceBatch(
    const Scene& scene, const std::vector<IntegratorRaySample>& samples,
    const RayCaster& recursiveRayCaster, IntegratorBatchMetrics* metrics,
    const IntegratorBatchSettings& settings) const {
    if (metrics) {
      metrics->reset(/*scalarFallback=*/false);
    }
    const WavefrontIntersectionBackend& intersectionBackend =
      settings.resolvedIntersectionBackend();
    if (metrics) {
      metrics->recordIntersectionBackend(intersectionBackend);
    }

    std::vector<Colord> sampleColors;
    const LightSampler lightSampler(scene.lights());
    HostBatchPathFrontier paths;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->pathSetupWorkerSeconds : nullptr);
      sampleColors.resize(samples.size(), Colord::black());
      paths.reserve(samples.size());
      for (std::size_t index = 0; index != samples.size(); ++index) {
        const auto& sample = samples[index];
        if (!sample.sampleStream()) {
          return Integrator::radianceBatch(scene, samples, recursiveRayCaster, metrics, settings);
        }

        paths.emplacePrimary(sample, sampleColors[index]);
      }
    }

    const bool trackRadianceDelta = metrics || settings.convergenceEnabled;
    const std::uint64_t totalSampleCount = sampleColors.size();
    ActivePathHits activeHits;
    activeHits.reserve(samples.size());

    for (int bounce = 0; bounce < m_maximumRecursionDepth; ++bounce) {
      const std::uint64_t activeCount = paths.size();
      if (activeCount == 0) {
        break;
      }
      HostBatchPathFrontier::Compaction frontierCompaction = paths.beginCompaction();
      if (metrics) {
        metrics->recordActiveDepth(activeCount);
        metrics->recordActiveHostPathStateBytes(paths.hostPathStateBytes());
      }
      if (isCancelled()) {
        if (metrics) {
          metrics->recordRadianceDeltaDepth(0.0, 0.0);
          metrics->recordSpawnedContinuations(0, 0);
          metrics->recordRetainedActiveDepth(0);
          metrics->recordRetainedHostPathStateBytes(0);
        }
        break;
      }

      BatchDepthMetrics depthMetrics;
      depthMetrics.trackRadianceDelta = trackRadianceDelta;
      depthMetrics.metrics = metrics;
      intersectActiveFrontier(intersectionBackend, scene, paths, activeHits, bounce, depthMetrics,
                              metrics);
      if (metrics) {
        metrics->recordActiveHitHostBytes(activeHits.hostBytes());
        metrics->recordFrontierIntersections(depthMetrics.frontierRayHits,
                                             depthMetrics.frontierRayMisses);
        metrics->recordFrontierTraversal(
          depthMetrics.frontierPacketChunks, depthMetrics.frontierPacketRays,
          depthMetrics.frontierRay4PacketChunks, depthMetrics.frontierRay8PacketChunks,
          depthMetrics.frontierScalarRays, depthMetrics.frontierPacketScalarFallbackRays,
          /*packetRefinedRays=*/0);
        metrics->recordFrontierClosestHitBatch(depthMetrics.frontierClosestHitBatchChunks,
                                               depthMetrics.frontierClosestHitBatchRays);
        metrics->recordPacketScalarFallbacksByReason(
          depthMetrics.frontierPacketScalarFallbackRaysByReason);
      }

      HostBatchPathFrontier spawnedPaths;
      spawnedPaths.reserve(activeHits.size() * 2);
      const DirectLightContributionBatch directLightContributions = sampleDirectLightingBatch(
        scene, lightSampler, activeHits, paths, bounce, intersectionBackend, metrics);

      for (std::size_t hitIndex = 0; hitIndex != activeHits.size(); ++hitIndex) {
        if (shadeActiveHit(scene, lightSampler, activeHits, hitIndex, paths, spawnedPaths,
                           directLightContributions, bounce, depthMetrics, metrics)) {
          frontierCompaction.retain(activeHits[hitIndex].pathIndex);
        }
      }

      {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        const std::uint64_t spawnedPathCount = spawnedPaths.size();
        const std::uint64_t spawnedHostPathStateBytes = spawnedPaths.hostPathStateBytes();
        if (metrics) {
          metrics->recordSpawnedContinuations(spawnedPathCount, spawnedHostPathStateBytes);
        }
        paths.compactWith(intersectionBackend, frontierCompaction, metrics);
        paths.appendAll(spawnedPaths);
      }
      const std::size_t retainedPathCount = paths.size();

      if (metrics) {
        metrics->recordRadianceDeltaDepth(depthMetrics.depthDeltaSquaredSum,
                                          depthMetrics.depthMaxDelta);
        metrics->recordRetainedActiveDepth(retainedPathCount);
        metrics->recordRetainedHostPathStateBytes(paths.hostPathStateBytes());
      }

      IntegratorBatchFeedback feedback;
      if (settings.progressObserver) {
        core::util::ScopedTimer timer(metrics ? &metrics->progressSnapshotWorkerSeconds : nullptr);
        feedback = settings.progressObserver->depthCompleted(static_cast<std::uint64_t>(bounce + 1),
                                                             sampleColors, retainedPathCount);
      }

      if (settings.convergenceEnabled && totalSampleCount != 0) {
        core::util::ScopedTimer timer(metrics ? &metrics->convergenceTestWorkerSeconds : nullptr);
        const double activeFraction =
          static_cast<double>(retainedPathCount) / static_cast<double>(totalSampleCount);
        const double rawRadianceDeltaRms =
          activeCount == 0
            ? 0.0
            : std::sqrt(depthMetrics.depthDeltaSquaredSum / static_cast<double>(activeCount));
        const double radianceDeltaRms =
          feedback.convergenceRadianceDeltaRms.value_or(rawRadianceDeltaRms);
        if (metrics && feedback.convergenceRadianceDeltaRms) {
          ++metrics->observerConvergenceFeedbackDepths;
        }
        if (activeFraction <= settings.activeSampleFractionThreshold &&
            radianceDeltaRms <= settings.radianceDeltaRmsThreshold) {
          if (metrics) {
            metrics->stoppedByConvergence = true;
            metrics->stoppedAfterDepth = metrics->activeSamplesPerDepth.size();
          }
          break;
        }
      }
    }

    return sampleColors;
  }
}
