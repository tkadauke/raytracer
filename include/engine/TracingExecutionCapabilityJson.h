#pragma once

#include "render/TracingExecutionCapability.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace engine {
  [[nodiscard]] QString tracingDomainLabel(render::TracingExecutionDomain domain);
  [[nodiscard]] QString tracingDeviceLabel(render::TracingExecutionDevice device);
  [[nodiscard]] QString tracingSupportLabel(render::TracingCapabilitySupport support);
  [[nodiscard]] QJsonObject tracingFallbackToJson(const render::TracingFallbackStatus& fallback);
  [[nodiscard]] QJsonObject tracingCapabilityToJson(const render::TracingCapabilityRecord& record);
  [[nodiscard]] QJsonArray
  tracingCapabilitiesToJson(const render::TracingExecutionCapabilityRecords& records);
  [[nodiscard]] QJsonObject
  tracingBackendFallbackToJson(const render::TracingExecutionCapabilityRecords& records);
}
