#pragma once

#include "core/Buffer.h"
#include "engine/graph/RenderGraphTypes.h"

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace engine::graph {
  /**
    * Typed cache key for graph-produced persistent artifacts.
    *
    * The key is deliberately structured instead of being a raw serialized blob:
    * callers compare the producer pass, produced resource descriptor, pass-state
    * fingerprint, and render-input fingerprint independently.
    */
  class RenderGraphCacheKey {
  public:
    RenderGraphCacheKey() = default;
    RenderGraphCacheKey(RenderPassId producerPassId, RenderResourceId resourceId,
                        RenderResourceDescriptor descriptor, std::string producerStateFingerprint,
                        std::string inputFingerprint);

    static RenderGraphCacheKey forPassOutput(const RenderPassNode& pass,
                                             const RenderResourceDescriptor& descriptor,
                                             std::string inputFingerprint);

    const RenderPassId& producerPassId() const;
    const RenderResourceId& resourceId() const;
    const RenderResourceDescriptor& descriptor() const;
    const std::string& producerStateFingerprint() const;
    const std::string& inputFingerprint() const;

    bool operator==(const RenderGraphCacheKey& other) const;
    bool operator<(const RenderGraphCacheKey& other) const;

  private:
    RenderPassId m_producerPassId;
    RenderResourceId m_resourceId;
    RenderResourceDescriptor m_descriptor;
    std::string m_producerStateFingerprint;
    std::string m_inputFingerprint;
  };

  /**
    * Immutable artifact published by a graph pass and reused through the cache.
    *
    * The first artifact shape carries descriptor and provenance only. Concrete
    * artifact subclasses can add shadow-map cascades, history buffers, or other
    * resource payloads once those resources move out of direct engine internals.
    */
  class RenderGraphCachedArtifact {
  public:
    RenderGraphCachedArtifact(RenderGraphCacheKey key, std::string description = {});
    virtual ~RenderGraphCachedArtifact();

    const RenderGraphCacheKey& key() const;
    const RenderResourceDescriptor& descriptor() const;
    const std::string& description() const;

  private:
    RenderGraphCacheKey m_key;
    std::string m_description;
  };

  /**
    * Immutable cached CPU depth image.
    */
  class RenderGraphDepthArtifact : public RenderGraphCachedArtifact {
  public:
    RenderGraphDepthArtifact(RenderGraphCacheKey key, const Buffer<double>& depth,
                             std::string description = {});

    const Buffer<double>& depth() const;
    void copyTo(Buffer<double>& destination) const;

  private:
    Buffer<double> m_depth;
  };

  /**
    * Shared cache for immutable graph artifacts.
    *
    * `GraphRenderEngine` owns one of these and clones share the same instance so
    * preview workers can publish and reuse artifacts across frames.
    */
  class RenderGraphArtifactCache {
  public:
    void store(std::shared_ptr<const RenderGraphCachedArtifact> artifact);
    std::shared_ptr<const RenderGraphCachedArtifact> find(const RenderGraphCacheKey& key) const;
    bool contains(const RenderGraphCacheKey& key) const;
    void erase(const RenderGraphCacheKey& key);
    std::size_t eraseProducerOutputs(const RenderPassId& producerPassId);
    std::size_t eraseResource(const RenderResourceId& resourceId);
    void clear();
    std::size_t size() const;

  private:
    mutable std::mutex m_mutex;
    std::map<RenderGraphCacheKey, std::shared_ptr<const RenderGraphCachedArtifact>> m_artifacts;
  };
}
