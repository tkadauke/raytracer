#pragma once

#include <QJsonObject>

#include <string>

namespace engine::graph {
  enum class RenderGraphCacheStatus { NotCacheable, Uncached, Hit, Miss, Stored, Invalidated };

  /**
    * Cache provenance for one graph resource snapshot.
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

  const char* toString(RenderGraphCacheStatus value);
}
