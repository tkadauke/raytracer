#pragma once

#include <string>
#include <utility>
#include <vector>

namespace render {
  enum class TracingExecutionDomain {
    Intersection,
    SceneRecords,
    Sampling,
    DirectLighting,
    BSDF,
    PathState,
    Accumulation
  };

  enum class TracingExecutionDevice { CPU, Hybrid, GPU, Unsupported };

  enum class TracingCapabilitySupport { Supported, Restricted, Unsupported, Fallback };

  struct TracingFallbackStatus {
    bool active = false;
    TracingExecutionDevice requestedDevice = TracingExecutionDevice::Unsupported;
    TracingExecutionDevice resolvedDevice = TracingExecutionDevice::Unsupported;
    std::string reason;

    [[nodiscard]] static TracingFallbackStatus none() {
      return {};
    }

    [[nodiscard]] static TracingFallbackStatus fallback(TracingExecutionDevice requested,
                                                        TracingExecutionDevice resolved,
                                                        std::string fallbackReason) {
      TracingFallbackStatus status;
      status.active = true;
      status.requestedDevice = requested;
      status.resolvedDevice = resolved;
      status.reason = std::move(fallbackReason);
      return status;
    }
  };

  struct TracingCapabilityRecord {
    TracingExecutionDomain domain = TracingExecutionDomain::Intersection;
    std::string name;
    TracingCapabilitySupport support = TracingCapabilitySupport::Unsupported;
    TracingExecutionDevice requestedDevice = TracingExecutionDevice::Unsupported;
    TracingExecutionDevice resolvedDevice = TracingExecutionDevice::Unsupported;
    std::string executionPath;
    std::string availability;
    std::string platform;
    std::string unsupportedReason;
    TracingFallbackStatus fallback;

    [[nodiscard]] bool supported() const {
      return support == TracingCapabilitySupport::Supported ||
             support == TracingCapabilitySupport::Restricted ||
             support == TracingCapabilitySupport::Fallback;
    }

    [[nodiscard]] bool restricted() const {
      return support == TracingCapabilitySupport::Restricted;
    }

    [[nodiscard]] bool usesCpu() const {
      return resolvedDevice == TracingExecutionDevice::CPU ||
             resolvedDevice == TracingExecutionDevice::Hybrid;
    }

    [[nodiscard]] bool usesGpu() const {
      return resolvedDevice == TracingExecutionDevice::GPU ||
             resolvedDevice == TracingExecutionDevice::Hybrid;
    }

    [[nodiscard]] bool fallsBack() const {
      return support == TracingCapabilitySupport::Fallback || fallback.active;
    }

    [[nodiscard]] static TracingCapabilityRecord
    cpu(TracingExecutionDomain domain, std::string capabilityName, std::string path = "cpu") {
      TracingCapabilityRecord record;
      record.domain = domain;
      record.name = std::move(capabilityName);
      record.support = TracingCapabilitySupport::Supported;
      record.requestedDevice = TracingExecutionDevice::CPU;
      record.resolvedDevice = TracingExecutionDevice::CPU;
      record.executionPath = std::move(path);
      record.availability = "available";
      return record;
    }

    [[nodiscard]] static TracingCapabilityRecord gpu(TracingExecutionDomain domain,
                                                     std::string capabilityName,
                                                     std::string platformName, std::string path) {
      TracingCapabilityRecord record;
      record.domain = domain;
      record.name = std::move(capabilityName);
      record.support = TracingCapabilitySupport::Supported;
      record.requestedDevice = TracingExecutionDevice::GPU;
      record.resolvedDevice = TracingExecutionDevice::GPU;
      record.platform = std::move(platformName);
      record.executionPath = std::move(path);
      record.availability = "available";
      return record;
    }

    [[nodiscard]] static TracingCapabilityRecord
    hybrid(TracingExecutionDomain domain, std::string capabilityName, std::string path) {
      TracingCapabilityRecord record;
      record.domain = domain;
      record.name = std::move(capabilityName);
      record.support = TracingCapabilitySupport::Supported;
      record.requestedDevice = TracingExecutionDevice::Hybrid;
      record.resolvedDevice = TracingExecutionDevice::Hybrid;
      record.executionPath = std::move(path);
      record.availability = "available";
      return record;
    }

    [[nodiscard]] static TracingCapabilityRecord restricted(TracingExecutionDomain domain,
                                                            std::string capabilityName,
                                                            TracingExecutionDevice resolved,
                                                            std::string path, std::string reason) {
      TracingCapabilityRecord record;
      record.domain = domain;
      record.name = std::move(capabilityName);
      record.support = TracingCapabilitySupport::Restricted;
      record.requestedDevice = resolved;
      record.resolvedDevice = resolved;
      record.executionPath = std::move(path);
      record.availability = "restricted";
      record.unsupportedReason = std::move(reason);
      return record;
    }

    [[nodiscard]] static TracingCapabilityRecord
    unsupported(TracingExecutionDomain domain, std::string capabilityName, std::string reason) {
      TracingCapabilityRecord record;
      record.domain = domain;
      record.name = std::move(capabilityName);
      record.support = TracingCapabilitySupport::Unsupported;
      record.unsupportedReason = std::move(reason);
      record.availability = "unsupported";
      return record;
    }

    [[nodiscard]] static TracingCapabilityRecord
    fallbackRecord(TracingExecutionDomain domain, std::string capabilityName,
                   TracingExecutionDevice requested, TracingExecutionDevice resolved,
                   std::string path, std::string reason) {
      TracingCapabilityRecord record;
      record.domain = domain;
      record.name = std::move(capabilityName);
      record.support = TracingCapabilitySupport::Fallback;
      record.requestedDevice = requested;
      record.resolvedDevice = resolved;
      record.executionPath = std::move(path);
      record.availability = "fallback";
      record.fallback = TracingFallbackStatus::fallback(requested, resolved, std::move(reason));
      return record;
    }
  };

  struct TracingIntersectionCapabilityRecords {
    TracingCapabilityRecord closestHit;
    TracingCapabilityRecord anyHit;
  };

  struct TracingSceneCapabilityRecords {
    TracingCapabilityRecord geometryRecords;
    TracingCapabilityRecord materialRecords;
    TracingCapabilityRecord textureRecords;
    TracingCapabilityRecord lightRecords;
  };

  struct TracingSamplingCapabilityRecords {
    TracingCapabilityRecord gpuRng;
    TracingCapabilityRecord namedDimensions;
  };

  struct TracingDirectLightingCapabilityRecords {
    TracingCapabilityRecord lightSampling;
    TracingCapabilityRecord visibility;
    TracingCapabilityRecord contribution;
    TracingCapabilityRecord residentBatch;
  };

  struct TracingBsdfCapabilityRecords {
    TracingCapabilityRecord eval;
    TracingCapabilityRecord sample;
    TracingCapabilityRecord deltaBranches;
  };

  struct TracingPathStateCapabilityRecords {
    TracingCapabilityRecord residency;
    TracingCapabilityRecord frontierCompaction;
    TracingCapabilityRecord spawnedContinuations;
  };

  struct TracingAccumulationCapabilityRecords {
    TracingCapabilityRecord sampleAccumulation;
    TracingCapabilityRecord progressiveReadback;
  };

  struct TracingExecutionCapabilityRecords {
    TracingIntersectionCapabilityRecords intersection;
    TracingSceneCapabilityRecords scene;
    TracingSamplingCapabilityRecords sampling;
    TracingDirectLightingCapabilityRecords directLighting;
    TracingBsdfCapabilityRecords bsdf;
    TracingPathStateCapabilityRecords pathState;
    TracingAccumulationCapabilityRecords accumulation;

    [[nodiscard]] std::vector<TracingCapabilityRecord> flattened() const {
      return {
        intersection.closestHit,
        intersection.anyHit,
        scene.geometryRecords,
        scene.materialRecords,
        scene.textureRecords,
        scene.lightRecords,
        sampling.gpuRng,
        sampling.namedDimensions,
        directLighting.lightSampling,
        directLighting.visibility,
        directLighting.contribution,
        directLighting.residentBatch,
        bsdf.eval,
        bsdf.sample,
        bsdf.deltaBranches,
        pathState.residency,
        pathState.frontierCompaction,
        pathState.spawnedContinuations,
        accumulation.sampleAccumulation,
        accumulation.progressiveReadback,
      };
    }

    [[nodiscard]] bool hasFallback() const {
      const auto records = flattened();
      for (const auto& record : records) {
        if (record.fallsBack()) {
          return true;
        }
      }
      return false;
    }
  };
}
