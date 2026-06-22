#include "engine/TracingExecutionCapabilityJson.h"

namespace engine {
  QString tracingDomainLabel(render::TracingExecutionDomain domain) {
    switch (domain) {
    case render::TracingExecutionDomain::Intersection:
      return QStringLiteral("intersection");
    case render::TracingExecutionDomain::SceneRecords:
      return QStringLiteral("scene_records");
    case render::TracingExecutionDomain::Sampling:
      return QStringLiteral("sampling");
    case render::TracingExecutionDomain::DirectLighting:
      return QStringLiteral("direct_lighting");
    case render::TracingExecutionDomain::BSDF:
      return QStringLiteral("bsdf");
    case render::TracingExecutionDomain::PathState:
      return QStringLiteral("path_state");
    case render::TracingExecutionDomain::Accumulation:
      return QStringLiteral("accumulation");
    }
    return QStringLiteral("unknown");
  }

  QString tracingDeviceLabel(render::TracingExecutionDevice device) {
    switch (device) {
    case render::TracingExecutionDevice::CPU:
      return QStringLiteral("cpu");
    case render::TracingExecutionDevice::Hybrid:
      return QStringLiteral("hybrid");
    case render::TracingExecutionDevice::GPU:
      return QStringLiteral("gpu");
    case render::TracingExecutionDevice::Unsupported:
      return QStringLiteral("unsupported");
    }
    return QStringLiteral("unsupported");
  }

  QString tracingSupportLabel(render::TracingCapabilitySupport support) {
    switch (support) {
    case render::TracingCapabilitySupport::Supported:
      return QStringLiteral("supported");
    case render::TracingCapabilitySupport::Restricted:
      return QStringLiteral("restricted");
    case render::TracingCapabilitySupport::Unsupported:
      return QStringLiteral("unsupported");
    case render::TracingCapabilitySupport::Fallback:
      return QStringLiteral("fallback");
    }
    return QStringLiteral("unsupported");
  }

  QJsonObject tracingFallbackToJson(const render::TracingFallbackStatus& fallback) {
    QJsonObject json;
    json["active"] = fallback.active;
    json["requestedDevice"] = tracingDeviceLabel(fallback.requestedDevice);
    json["resolvedDevice"] = tracingDeviceLabel(fallback.resolvedDevice);
    json["reason"] = QString::fromStdString(fallback.reason);
    return json;
  }

  QJsonObject tracingCapabilityToJson(const render::TracingCapabilityRecord& record) {
    QJsonObject json;
    json["domain"] = tracingDomainLabel(record.domain);
    json["name"] = QString::fromStdString(record.name);
    json["support"] = tracingSupportLabel(record.support);
    json["requestedDevice"] = tracingDeviceLabel(record.requestedDevice);
    json["resolvedDevice"] = tracingDeviceLabel(record.resolvedDevice);
    json["executionPath"] = QString::fromStdString(record.executionPath);
    json["availability"] = QString::fromStdString(record.availability);
    json["platform"] = QString::fromStdString(record.platform);
    json["unsupportedReason"] = QString::fromStdString(record.unsupportedReason);
    json["fallback"] = tracingFallbackToJson(record.fallback);
    return json;
  }

  QJsonArray tracingCapabilitiesToJson(const render::TracingExecutionCapabilityRecords& records) {
    QJsonArray json;
    for (const auto& record : records.flattened()) {
      json.push_back(tracingCapabilityToJson(record));
    }
    return json;
  }

  QJsonObject
  tracingBackendFallbackToJson(const render::TracingExecutionCapabilityRecords& records) {
    for (const auto& record : records.flattened()) {
      if (record.fallsBack()) {
        QJsonObject json = tracingFallbackToJson(record.fallback);
        json["capability"] = QString::fromStdString(record.name);
        json["domain"] = tracingDomainLabel(record.domain);
        if (!record.fallback.active) {
          json["active"] = true;
          json["requestedDevice"] = tracingDeviceLabel(record.requestedDevice);
          json["resolvedDevice"] = tracingDeviceLabel(record.resolvedDevice);
          json["reason"] = QString::fromStdString(record.unsupportedReason);
        }
        return json;
      }
    }
    QJsonObject json = tracingFallbackToJson(render::TracingFallbackStatus::none());
    json["capability"] = QString();
    json["domain"] = QString();
    return json;
  }
}
