#include "engine/graph/RenderGraphArtifactCache.h"

#include "core/util/BufferUtils.h"
#include "engine/graph/RenderPassState.h"

#include <QJsonDocument>

#include <stdexcept>
#include <tuple>
#include <utility>

namespace engine::graph {
  namespace {
    std::string stateFingerprint(const RenderPassNode& pass) {
      if (!pass.state) {
        return {};
      }

      const QJsonDocument document(pass.state->toJson());
      return document.toJson(QJsonDocument::Compact).toStdString();
    }

    auto keyFields(const RenderGraphCacheKey& key) {
      const auto& descriptor = key.descriptor();
      return std::tie(key.producerPassId(), key.resourceId(), descriptor.name, descriptor.type,
                      descriptor.format, descriptor.width, descriptor.height,
                      descriptor.sampleCount, descriptor.domain, descriptor.lifetime,
                      key.producerStateFingerprint(), key.inputFingerprint());
    }
  }

  RenderGraphCacheKey::RenderGraphCacheKey(RenderPassId producerPassId, RenderResourceId resourceId,
                                           RenderResourceDescriptor descriptor,
                                           std::string producerStateFingerprint,
                                           std::string inputFingerprint)
      : m_producerPassId(std::move(producerPassId)),
        m_resourceId(std::move(resourceId)),
        m_descriptor(std::move(descriptor)),
        m_producerStateFingerprint(std::move(producerStateFingerprint)),
        m_inputFingerprint(std::move(inputFingerprint)) {
  }

  RenderGraphCacheKey RenderGraphCacheKey::forPassOutput(const RenderPassNode& pass,
                                                         const RenderResourceDescriptor& descriptor,
                                                         std::string inputFingerprint) {
    return RenderGraphCacheKey(pass.id, descriptor.id, descriptor, stateFingerprint(pass),
                               std::move(inputFingerprint));
  }

  const RenderPassId& RenderGraphCacheKey::producerPassId() const {
    return m_producerPassId;
  }

  const RenderResourceId& RenderGraphCacheKey::resourceId() const {
    return m_resourceId;
  }

  const RenderResourceDescriptor& RenderGraphCacheKey::descriptor() const {
    return m_descriptor;
  }

  const std::string& RenderGraphCacheKey::producerStateFingerprint() const {
    return m_producerStateFingerprint;
  }

  const std::string& RenderGraphCacheKey::inputFingerprint() const {
    return m_inputFingerprint;
  }

  bool RenderGraphCacheKey::operator==(const RenderGraphCacheKey& other) const {
    return keyFields(*this) == keyFields(other);
  }

  bool RenderGraphCacheKey::operator<(const RenderGraphCacheKey& other) const {
    return keyFields(*this) < keyFields(other);
  }

  RenderGraphCachedArtifact::RenderGraphCachedArtifact(RenderGraphCacheKey key,
                                                       std::string description)
      : m_key(std::move(key)),
        m_description(std::move(description)) {
  }

  RenderGraphCachedArtifact::~RenderGraphCachedArtifact() = default;

  const RenderGraphCacheKey& RenderGraphCachedArtifact::key() const {
    return m_key;
  }

  const RenderResourceDescriptor& RenderGraphCachedArtifact::descriptor() const {
    return m_key.descriptor();
  }

  const std::string& RenderGraphCachedArtifact::description() const {
    return m_description;
  }

  bool RenderGraphCachedArtifact::copyDepthTo(Buffer<double>&) const {
    return false;
  }

  bool RenderGraphCachedArtifact::copyRasterShadowMapPreviewTo(Buffer<double>&) const {
    return false;
  }

  bool RenderGraphCachedArtifact::applyRasterShadowMapsTo(engine::raster::Rasterizer&) const {
    return false;
  }

  RenderGraphDepthArtifact::RenderGraphDepthArtifact(RenderGraphCacheKey key,
                                                     const Buffer<double>& depth,
                                                     std::string description)
      : RenderGraphCachedArtifact(std::move(key), std::move(description)),
        m_depth(depth.width(), depth.height()) {
    core::util::copyBuffer(m_depth, depth);
  }

  const Buffer<double>& RenderGraphDepthArtifact::depth() const {
    return m_depth;
  }

  bool RenderGraphDepthArtifact::copyDepthTo(Buffer<double>& destination) const {
    if (!core::util::bufferDimensionsEqual(destination, m_depth)) {
      throw std::runtime_error("cached depth artifact copy requires matching buffer dimensions");
    }

    core::util::copyBuffer(destination, m_depth);
    return true;
  }

  void RenderGraphArtifactCache::store(std::shared_ptr<const RenderGraphCachedArtifact> artifact) {
    if (!artifact) {
      throw std::invalid_argument("cannot cache a null render graph artifact");
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_artifacts[artifact->key()] = std::move(artifact);
  }

  std::shared_ptr<const RenderGraphCachedArtifact>
  RenderGraphArtifactCache::find(const RenderGraphCacheKey& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_artifacts.find(key);
    return it == m_artifacts.end() ? nullptr : it->second;
  }

  bool RenderGraphArtifactCache::contains(const RenderGraphCacheKey& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_artifacts.find(key) != m_artifacts.end();
  }

  void RenderGraphArtifactCache::erase(const RenderGraphCacheKey& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_artifacts.erase(key);
  }

  std::size_t RenderGraphArtifactCache::eraseProducerOutputs(const RenderPassId& producerPassId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t removed = 0;
    for (auto it = m_artifacts.begin(); it != m_artifacts.end();) {
      if (it->first.producerPassId() == producerPassId) {
        it = m_artifacts.erase(it);
        ++removed;
      } else {
        ++it;
      }
    }
    return removed;
  }

  std::size_t RenderGraphArtifactCache::eraseResource(const RenderResourceId& resourceId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t removed = 0;
    for (auto it = m_artifacts.begin(); it != m_artifacts.end();) {
      if (it->first.resourceId() == resourceId) {
        it = m_artifacts.erase(it);
        ++removed;
      } else {
        ++it;
      }
    }
    return removed;
  }

  void RenderGraphArtifactCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_artifacts.clear();
  }

  std::size_t RenderGraphArtifactCache::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_artifacts.size();
  }
}
