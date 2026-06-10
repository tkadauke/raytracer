#include "widgets/world/RenderGraphInspectorWidget.h"

#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderPassState.h"

#include <QBrush>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QMetaObject>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace engine::graph;

namespace {
  constexpr int GraphItemKindRole = 0;
  constexpr int GraphItemIdRole = 1;
  constexpr int GraphItemExecutionStateRole = 2;
  constexpr int GroupScopeRole = Qt::UserRole;
  constexpr int GroupValueRole = Qt::UserRole + 1;
  constexpr double PassWidth = 190.0;
  constexpr double PassHeight = 104.0;
  constexpr double ResourceWidth = 150.0;
  constexpr double ResourceHeight = 58.0;
  constexpr double ColumnGap = PassWidth + ResourceWidth + 120.0;
  constexpr double RowGap = 136.0;
  constexpr double OriginX = 40.0;
  constexpr double OriginY = 44.0;
  constexpr double NodeTextInset = 10.0;
  constexpr auto LiveExecutionDelay = std::chrono::milliseconds(500);

  QString qstr(const std::string& value) {
    return QString::fromStdString(value);
  }

  QString dashIfEmpty(const QString& value) {
    return value.isEmpty() ? QStringLiteral("-") : value;
  }

  bool hasFeature(const std::vector<RenderFeatureKind>& features, const char* feature) {
    return std::find(features.begin(), features.end(), RenderFeatureKind(feature)) !=
           features.end();
  }

  enum class PassExecutionState { Idle, Running, Completed, Failed };
}

struct RenderGraphInspectorWidget::Private {
  RenderPlan plan;
  RenderGraphOverrides overrides;
  std::shared_ptr<const RenderGraphExecutionTrace> trace;
  QString compileError;
  RenderPassId selectedPassId;
  RenderResourceId selectedResourceId;
  bool hasSelection{false};
  std::map<RenderPassId, std::chrono::steady_clock::time_point> pendingExecutionStarts;
  std::map<RenderPassId, PassExecutionState> executionStates;
  std::map<RenderPassId, QString> executionMessages;
  QTimer* liveExecutionTimer{nullptr};
  QGraphicsView* graph{nullptr};
  QGraphicsScene* graphScene{nullptr};
  QToolButton* exportText{nullptr};
  QToolButton* exportDot{nullptr};
  QToolButton* exportJson{nullptr};
  QTreeWidget* passes{nullptr};
  QTreeWidget* groups{nullptr};
  QTreeWidget* resources{nullptr};
  QLabel* validationStatus{nullptr};
  bool updating{false};

  QString humanizeIdentifier(QString value) const;
  QString displayName(const RenderPassNode& pass) const;
  QString displayName(const RenderResourceDescriptor& resource) const;
  QString displayResourceName(const RenderPlan& plan, const RenderResourceId& resourceId) const;
  QString displayText(RenderExecutorKind executor) const;
  QString displayText(RenderPassKind kind) const;
  QString displayText(DisabledBehavior behavior) const;
  QString displayText(RenderResourceType type) const;
  QString displayText(RenderResourceDomain domain) const;
  QString displayText(RenderResourceLifetime lifetime) const;
  QString displayText(RenderResourceFormat format) const;
  QString displayFeatureText(const RenderFeatureKind& feature) const;
  QString graphEnumText(const char* value) const;
  QString metadataIdentifierText(QString value) const;
  QString executionStateName(PassExecutionState state) const;
  qulonglong jsonIntegerValue(const QJsonObject& object, const QString& key) const;
  qulonglong jsonIntegerArraySum(const QJsonArray& array) const;
  QString jsonIntegerObjectSummary(const QJsonObject& object) const;
  QString percentage(double numerator, double denominator) const;
  QString average(double numerator, double denominator) const;
  QString intersectionScenePayloadSummary(const QJsonObject& batching) const;
  QString passStateText(const RenderPassNode& pass) const;
  void addDetailRow(DetailRows& rows, const QString& name, const QString& value) const;
  void addDetailStringMetadataRow(DetailRows& rows, const QString& name,
                                  const QJsonObject& metadata, const QString& key,
                                  bool humanize = false) const;
  void addDetailBoolMetadataRow(DetailRows& rows, const QString& name, const QJsonObject& metadata,
                                const QString& key) const;
  void addDetailIntegerMetadataRow(DetailRows& rows, const QString& name,
                                   const QJsonObject& metadata, const QString& key) const;
  void addDetailIntegerObjectMetadataRow(DetailRows& rows, const QString& name,
                                         const QJsonObject& metadata, const QString& key) const;
  void addDetailMillisecondsMetadataRow(DetailRows& rows, const QString& name,
                                        const QJsonObject& metadata, const QString& key) const;
  void addIntersectionBackendDetailRows(DetailRows& rows, const QJsonObject& batching) const;
  DetailRows passDetailRows(const RenderPlan& plan, const RenderPassId& passId) const;
  QString passTraceLine(const RenderPassNode& pass) const;
  const RenderGraphResourceSnapshot*
  firstSnapshotForResource(const RenderResourceId& resourceId) const;
  QString resourceTraceLine(const RenderResourceDescriptor& resource) const;
  QString sceneSelectorText(const SceneSelector& selector) const;
  QString cameraText(const std::optional<RenderCameraRef>& camera) const;
  QString shadingProfileText(const std::optional<ShadingProfileRef>& shadingProfile) const;
  QStringList passSceneViewLines(const RenderPassNode& pass) const;
  QString sizeText(const RenderResourceDescriptor& resource) const;
  QString resourceFeatures(const RenderResourceDescriptor& resource) const;
  QString resourceReads(const RenderPlan& plan, const std::vector<ResourceRead>& reads) const;
  QString resourceWrites(const RenderPlan& plan, const std::vector<ResourceWrite>& writes) const;
  QString dependencySummary(const RenderPlan& plan,
                            const std::vector<RenderPassDependency>& dependencies) const;
  QString passTooltip(const RenderPlan& plan, const RenderPassNode& pass,
                      const QString& executionMessage) const;
  QString resourceProducer(const RenderPlan& plan, const RenderResourceId& resource) const;
  QString resourceConsumers(const RenderPlan& plan, const RenderResourceId& resource) const;
  QString resourceTooltip(const RenderPlan& plan, const RenderResourceDescriptor& resource) const;
  std::map<RenderPassId, QPointF> passPositions(const RenderPlan& plan) const;
  QPointF passCenter(const QPointF& topLeft) const;
  QPointF resourcePosition(const RenderPlan& plan, const RenderResourceDescriptor& resource,
                           const std::map<RenderPassId, QPointF>& passes, int fallbackRow) const;
  QGraphicsRectItem* addNode(QGraphicsScene& scene, const QRectF& rect, const QString& kind,
                             const QString& id, const QStringList& lines, const QPen& pen,
                             const QBrush& brush) const;
  void addEdge(QGraphicsScene& scene, const QPointF& from, const QPointF& to) const;
  QGraphicsItem* graphNodeItem(QGraphicsItem* item) const;
};

QString RenderGraphInspectorWidget::Private::humanizeIdentifier(QString value) const {
  value.replace(QLatin1Char('_'), QLatin1Char(' '));
  value.replace(QLatin1Char('-'), QLatin1Char(' '));
  value = value.simplified();
  for (int i = 0; i != value.size(); ++i) {
    if (i == 0 || value[i - 1].isSpace()) {
      value[i] = value[i].toUpper();
    }
  }
  return value;
}

QString RenderGraphInspectorWidget::Private::displayName(const RenderPassNode& pass) const {
  return pass.name.empty() ? humanizeIdentifier(qstr(pass.id)) : qstr(pass.name);
}

QString
RenderGraphInspectorWidget::Private::displayName(const RenderResourceDescriptor& resource) const {
  return resource.name.empty() ? humanizeIdentifier(qstr(resource.id)) : qstr(resource.name);
}

QString
RenderGraphInspectorWidget::Private::displayResourceName(const RenderPlan& plan,
                                                         const RenderResourceId& resourceId) const {
  const RenderResourceDescriptor* resource = plan.findResource(resourceId);
  return resource ? displayName(*resource) : humanizeIdentifier(qstr(resourceId));
}

QString RenderGraphInspectorWidget::Private::displayText(RenderExecutorKind executor) const {
  switch (executor) {
  case RenderExecutorKind::Raytracer:
    return QStringLiteral("Raytracer");
  case RenderExecutorKind::Wavefront:
    return QStringLiteral("Wavefront");
  case RenderExecutorKind::Rasterizer:
    return QStringLiteral("Rasterizer");
  case RenderExecutorKind::Wireframe:
    return QStringLiteral("Wireframe");
  case RenderExecutorKind::Composite:
    return QStringLiteral("Composite");
  case RenderExecutorKind::PostProcess:
    return QStringLiteral("Postprocess");
  }
  return graphEnumText(toString(executor));
}

QString RenderGraphInspectorWidget::Private::displayText(RenderPassKind kind) const {
  switch (kind) {
  case RenderPassKind::Beauty:
    return QStringLiteral("Beauty");
  case RenderPassKind::Shadow:
    return QStringLiteral("Shadow");
  case RenderPassKind::Overlay:
    return QStringLiteral("Overlay");
  case RenderPassKind::Composite:
    return QStringLiteral("Composite");
  case RenderPassKind::Tonemap:
    return QStringLiteral("Tone map");
  case RenderPassKind::PostProcess:
    return QStringLiteral("Postprocess");
  case RenderPassKind::Readback:
    return QStringLiteral("Readback");
  case RenderPassKind::Visibility:
    return QStringLiteral("Visibility");
  case RenderPassKind::AOV:
    return QStringLiteral("AOV");
  case RenderPassKind::Debug:
    return QStringLiteral("Debug");
  case RenderPassKind::Custom:
    return QStringLiteral("Custom");
  }
  return graphEnumText(toString(kind));
}

QString RenderGraphInspectorWidget::Private::displayText(DisabledBehavior behavior) const {
  switch (behavior) {
  case DisabledBehavior::Error:
    return QStringLiteral("Error");
  case DisabledBehavior::CullDependents:
    return QStringLiteral("Cull dependents");
  case DisabledBehavior::SubstituteDefault:
    return QStringLiteral("Substitute default");
  case DisabledBehavior::Passthrough:
    return QStringLiteral("Passthrough");
  }
  return graphEnumText(toString(behavior));
}

QString RenderGraphInspectorWidget::Private::displayText(RenderResourceType type) const {
  switch (type) {
  case RenderResourceType::Color:
    return QStringLiteral("Color");
  case RenderResourceType::Depth:
    return QStringLiteral("Depth");
  case RenderResourceType::Stencil:
    return QStringLiteral("Stencil");
  case RenderResourceType::ObjectId:
    return QStringLiteral("Object ID");
  case RenderResourceType::MaterialId:
    return QStringLiteral("Material ID");
  case RenderResourceType::Normal:
    return QStringLiteral("Normal");
  case RenderResourceType::WorldPosition:
    return QStringLiteral("World position");
  case RenderResourceType::MotionVector:
    return QStringLiteral("Motion vector");
  case RenderResourceType::ShadowMap:
    return QStringLiteral("Shadow map");
  case RenderResourceType::ShadowMask:
    return QStringLiteral("Shadow mask");
  case RenderResourceType::VisibilitySet:
    return QStringLiteral("Visibility set");
  case RenderResourceType::CustomTexture:
    return QStringLiteral("Custom texture");
  }
  return graphEnumText(toString(type));
}

QString RenderGraphInspectorWidget::Private::displayText(RenderResourceDomain domain) const {
  switch (domain) {
  case RenderResourceDomain::CPU:
    return QStringLiteral("CPU");
  case RenderResourceDomain::GPU:
    return QStringLiteral("GPU");
  }
  return graphEnumText(toString(domain));
}

QString RenderGraphInspectorWidget::Private::displayText(RenderResourceLifetime lifetime) const {
  switch (lifetime) {
  case RenderResourceLifetime::Transient:
    return QStringLiteral("Transient");
  case RenderResourceLifetime::Imported:
    return QStringLiteral("Imported");
  case RenderResourceLifetime::Exported:
    return QStringLiteral("Exported");
  case RenderResourceLifetime::History:
    return QStringLiteral("History");
  case RenderResourceLifetime::PersistentCache:
    return QStringLiteral("Persistent cache");
  }
  return graphEnumText(toString(lifetime));
}

QString RenderGraphInspectorWidget::Private::displayText(RenderResourceFormat format) const {
  switch (format) {
  case RenderResourceFormat::Unknown:
    return QStringLiteral("Unknown");
  case RenderResourceFormat::RGBDouble:
    return QStringLiteral("RGB double");
  case RenderResourceFormat::DepthDouble:
    return QStringLiteral("Depth double");
  case RenderResourceFormat::UInt8:
    return QStringLiteral("UInt8");
  case RenderResourceFormat::UInt32:
    return QStringLiteral("UInt32");
  case RenderResourceFormat::ScalarDouble:
    return QStringLiteral("Scalar double");
  }
  return graphEnumText(toString(format));
}

QString
RenderGraphInspectorWidget::Private::displayFeatureText(const RenderFeatureKind& feature) const {
  QString value = qstr(feature);
  value.replace(QLatin1Char('_'), QLatin1Char(' '));
  value.replace(QLatin1Char('-'), QLatin1Char(' '));
  const QStringList words = value.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);

  QStringList labels;
  for (QString word : words) {
    const QString lower = word.toLower();
    if (lower == QStringLiteral("aa")) {
      labels << QStringLiteral("AA");
    } else if (lower == QStringLiteral("aov")) {
      labels << QStringLiteral("AOV");
    } else if (lower == QStringLiteral("cpu")) {
      labels << QStringLiteral("CPU");
    } else if (lower == QStringLiteral("gpu")) {
      labels << QStringLiteral("GPU");
    } else if (lower == QStringLiteral("id")) {
      labels << QStringLiteral("ID");
    } else if (lower == QStringLiteral("msaa")) {
      labels << QStringLiteral("MSAA");
    } else if (lower == QStringLiteral("opengl")) {
      labels << QStringLiteral("OpenGL");
    } else if (lower == QStringLiteral("pcf")) {
      labels << QStringLiteral("PCF");
    } else if (lower == QStringLiteral("pcss")) {
      labels << QStringLiteral("PCSS");
    } else {
      word = lower;
      word[0] = word[0].toUpper();
      labels << word;
    }
  }
  return labels.join(QLatin1Char(' '));
}

QString RenderGraphInspectorWidget::Private::graphEnumText(const char* value) const {
  return humanizeIdentifier(QString::fromLatin1(value));
}

QString RenderGraphInspectorWidget::Private::metadataIdentifierText(QString value) const {
  value.replace(QLatin1Char('_'), QLatin1Char(' '));
  value.replace(QLatin1Char('-'), QLatin1Char(' '));
  value = value.simplified();

  QStringList words = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (QString& word : words) {
    const QString lower = word.toLower();
    if (lower == QStringLiteral("cpu") || lower == QStringLiteral("gpu") ||
        lower == QStringLiteral("bvh")) {
      word = lower.toUpper();
      continue;
    }
    word = humanizeIdentifier(word);
  }
  return words.join(QLatin1Char(' '));
}

QString RenderGraphInspectorWidget::Private::executionStateName(PassExecutionState state) const {
  switch (state) {
  case PassExecutionState::Idle:
    return QStringLiteral("idle");
  case PassExecutionState::Running:
    return QStringLiteral("running");
  case PassExecutionState::Completed:
    return QStringLiteral("completed");
  case PassExecutionState::Failed:
    return QStringLiteral("failed");
  }
  return QStringLiteral("idle");
}

qulonglong RenderGraphInspectorWidget::Private::jsonIntegerValue(const QJsonObject& object,
                                                                 const QString& key) const {
  return static_cast<qulonglong>(object.value(key).toDouble());
}

qulonglong RenderGraphInspectorWidget::Private::jsonIntegerArraySum(const QJsonArray& array) const {
  qulonglong result = 0;
  for (const QJsonValue& value : array) {
    result += static_cast<qulonglong>(value.toDouble());
  }
  return result;
}

QString
RenderGraphInspectorWidget::Private::jsonIntegerObjectSummary(const QJsonObject& object) const {
  QStringList values;
  for (auto it = object.begin(); it != object.end(); ++it) {
    values.push_back(QStringLiteral("%1 %2")
                       .arg(humanizeIdentifier(it.key()))
                       .arg(static_cast<qulonglong>(it.value().toDouble())));
  }
  values.sort();
  return values.join(QStringLiteral("/"));
}

QString RenderGraphInspectorWidget::Private::percentage(double numerator,
                                                        double denominator) const {
  const double ratio = denominator == 0.0 ? 0.0 : numerator / denominator;
  return QStringLiteral("%1%").arg(ratio * 100.0, 0, 'f', 2);
}

QString RenderGraphInspectorWidget::Private::average(double numerator, double denominator) const {
  const double ratio = denominator == 0.0 ? 0.0 : numerator / denominator;
  return QStringLiteral("%1").arg(ratio, 0, 'f', 2);
}

QString RenderGraphInspectorWidget::Private::intersectionScenePayloadSummary(
  const QJsonObject& batching) const {
  const std::vector<std::pair<QString, qulonglong>> counts = {
    {QStringLiteral("tri"),
     jsonIntegerValue(batching, QStringLiteral("intersectionSceneTriangles"))},
    {QStringLiteral("sphere"),
     jsonIntegerValue(batching, QStringLiteral("intersectionSceneSpheres"))},
    {QStringLiteral("plane"),
     jsonIntegerValue(batching, QStringLiteral("intersectionScenePlanes"))},
    {QStringLiteral("rect"),
     jsonIntegerValue(batching, QStringLiteral("intersectionSceneRectangles"))},
    {QStringLiteral("disk"), jsonIntegerValue(batching, QStringLiteral("intersectionSceneDisks"))},
    {QStringLiteral("open cyl"),
     jsonIntegerValue(batching, QStringLiteral("intersectionSceneOpenCylinders"))},
    {QStringLiteral("xform"),
     jsonIntegerValue(batching, QStringLiteral("intersectionSceneTransforms"))},
    {QStringLiteral("unsupported"),
     jsonIntegerValue(batching, QStringLiteral("intersectionSceneUnsupportedPrimitives"))}};
  const bool hasPayloadCount =
    std::any_of(counts.begin(), counts.end(), [](const auto& count) { return count.second > 0; });
  if (!hasPayloadCount) {
    return QString();
  }

  QStringList parts;
  parts.reserve(static_cast<int>(counts.size()));
  for (const auto& [label, count] : counts) {
    parts << QStringLiteral("%1 %2").arg(label).arg(count);
  }
  return parts.join(QStringLiteral(", "));
}

QString RenderGraphInspectorWidget::Private::passStateText(const RenderPassNode& pass) const {
  if (!pass.state)
    return QStringLiteral("-");

  const QJsonObject state = pass.state->toJson();
  if (state.isEmpty())
    return QStringLiteral("-");
  return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}

void RenderGraphInspectorWidget::Private::addDetailRow(DetailRows& rows, const QString& name,
                                                       const QString& value) const {
  rows.push_back({name, dashIfEmpty(value)});
}

void RenderGraphInspectorWidget::Private::addDetailStringMetadataRow(DetailRows& rows,
                                                                     const QString& name,
                                                                     const QJsonObject& metadata,
                                                                     const QString& key,
                                                                     bool humanize) const {
  if (!metadata.contains(key))
    return;

  QString value = metadata.value(key).toString();
  if (humanize)
    value = metadataIdentifierText(value);
  addDetailRow(rows, name, value);
}

void RenderGraphInspectorWidget::Private::addDetailBoolMetadataRow(DetailRows& rows,
                                                                   const QString& name,
                                                                   const QJsonObject& metadata,
                                                                   const QString& key) const {
  if (!metadata.contains(key))
    return;

  addDetailRow(rows, name,
               metadata.value(key).toBool() ? QStringLiteral("yes") : QStringLiteral("no"));
}

void RenderGraphInspectorWidget::Private::addDetailIntegerMetadataRow(DetailRows& rows,
                                                                      const QString& name,
                                                                      const QJsonObject& metadata,
                                                                      const QString& key) const {
  if (!metadata.contains(key))
    return;

  addDetailRow(rows, name, QString::number(jsonIntegerValue(metadata, key)));
}

void RenderGraphInspectorWidget::Private::addDetailIntegerObjectMetadataRow(
  DetailRows& rows, const QString& name, const QJsonObject& metadata, const QString& key) const {
  if (!metadata.contains(key))
    return;

  const QString value = jsonIntegerObjectSummary(metadata.value(key).toObject());
  if (!value.isEmpty())
    addDetailRow(rows, name, value);
}

void RenderGraphInspectorWidget::Private::addDetailMillisecondsMetadataRow(
  DetailRows& rows, const QString& name, const QJsonObject& metadata, const QString& key) const {
  if (!metadata.contains(key))
    return;

  addDetailRow(rows, name,
               QStringLiteral("%1 ms").arg(metadata.value(key).toDouble() * 1000.0, 0, 'f', 3));
}

void RenderGraphInspectorWidget::Private::addIntersectionBackendDetailRows(
  DetailRows& rows, const QJsonObject& batching) const {
  if (batching.isEmpty())
    return;

  addDetailStringMetadataRow(rows, QStringLiteral("Intersection backend request"), batching,
                             QStringLiteral("intersectionBackendRequest"), true);
  addDetailStringMetadataRow(rows, QStringLiteral("Intersection backend"), batching,
                             QStringLiteral("intersectionBackend"), true);
  addDetailStringMetadataRow(rows, QStringLiteral("Intersection backend availability"), batching,
                             QStringLiteral("intersectionBackendAvailability"), true);
  addDetailStringMetadataRow(rows, QStringLiteral("Intersection backend platform"), batching,
                             QStringLiteral("intersectionBackendPlatform"), true);
  addDetailStringMetadataRow(rows, QStringLiteral("Intersection backend execution path"), batching,
                             QStringLiteral("intersectionBackendExecutionPath"), true);
  addDetailStringMetadataRow(rows, QStringLiteral("Closest-hit execution path"), batching,
                             QStringLiteral("intersectionBackendClosestHitExecutionPath"), true);
  addDetailStringMetadataRow(rows, QStringLiteral("Any-hit execution path"), batching,
                             QStringLiteral("intersectionBackendAnyHitExecutionPath"), true);
  addDetailStringMetadataRow(rows, QStringLiteral("Intersection backend fallback"), batching,
                             QStringLiteral("intersectionBackendFallbackReason"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Expected intersection rays"), batching,
                              QStringLiteral("intersectionBackendExpectedRays"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Expected closest-hit rays"), batching,
                              QStringLiteral("intersectionBackendExpectedClosestHitRays"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Expected any-hit rays"), batching,
                              QStringLiteral("intersectionBackendExpectedAnyHitRays"));
  if (batching.value(QStringLiteral("intersectionBackendRequest")).toString() ==
      QStringLiteral("auto")) {
    addDetailIntegerMetadataRow(rows, QStringLiteral("Auto minimum GPU rays"), batching,
                                QStringLiteral("intersectionBackendAutoMinimumGpuRays"));
    addDetailIntegerMetadataRow(
      rows, QStringLiteral("Auto estimated query transfer bytes"), batching,
      QStringLiteral("intersectionBackendAutoEstimatedQueryTransferBytes"));
  }
  addDetailBoolMetadataRow(rows, QStringLiteral("GPU device available"), batching,
                           QStringLiteral("intersectionBackendPlatformGpuDeviceAvailable"));
  addDetailBoolMetadataRow(rows, QStringLiteral("GPU render path available"), batching,
                           QStringLiteral("intersectionBackendPlatformGpuRenderPathAvailable"));
  addDetailBoolMetadataRow(rows, QStringLiteral("Intersection scene compiled"), batching,
                           QStringLiteral("intersectionSceneCompiled"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene primitives"), batching,
                              QStringLiteral("intersectionScenePrimitives"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene BVH nodes"), batching,
                              QStringLiteral("intersectionSceneBvhNodes"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene triangles"), batching,
                              QStringLiteral("intersectionSceneTriangles"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene spheres"), batching,
                              QStringLiteral("intersectionSceneSpheres"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene planes"), batching,
                              QStringLiteral("intersectionScenePlanes"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene rectangles"), batching,
                              QStringLiteral("intersectionSceneRectangles"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene disks"), batching,
                              QStringLiteral("intersectionSceneDisks"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene open cylinders"), batching,
                              QStringLiteral("intersectionSceneOpenCylinders"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene transforms"), batching,
                              QStringLiteral("intersectionSceneTransforms"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene unsupported primitives"),
                              batching, QStringLiteral("intersectionSceneUnsupportedPrimitives"));
  addDetailIntegerObjectMetadataRow(rows, QStringLiteral("Intersection scene unsupported reasons"),
                                    batching,
                                    QStringLiteral("intersectionSceneUnsupportedReasons"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection scene upload bytes"), batching,
                              QStringLiteral("intersectionSceneUploadBytes"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Intersection query transfer bytes"), batching,
                              QStringLiteral("intersectionEstimatedQueryTransferBytes"));
  addDetailBoolMetadataRow(rows, QStringLiteral("Packed closest-hit eligible"), batching,
                           QStringLiteral("intersectionScenePackedClosestHitEligible"));
  addDetailBoolMetadataRow(rows, QStringLiteral("Packed any-hit eligible"), batching,
                           QStringLiteral("intersectionScenePackedAnyHitEligible"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Closest-hit rays submitted"), batching,
                              QStringLiteral("closestHitRaysSubmitted"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Any-hit rays submitted"), batching,
                              QStringLiteral("anyHitRaysSubmitted"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Closest-hit queries"), batching,
                              QStringLiteral("closestHitQueries"));
  addDetailIntegerMetadataRow(rows, QStringLiteral("Any-hit queries"), batching,
                              QStringLiteral("anyHitQueries"));
  const qulonglong closestHitBatchChunks = jsonIntegerArraySum(
    batching.value(QStringLiteral("frontierClosestHitBatchChunksPerDepth")).toArray());
  if (closestHitBatchChunks > 0) {
    const qulonglong closestHitBatchRays = jsonIntegerArraySum(
      batching.value(QStringLiteral("frontierClosestHitBatchRaysPerDepth")).toArray());
    addDetailRow(rows, QStringLiteral("Closest-hit batch average rays"),
                 average(closestHitBatchRays, closestHitBatchChunks));
  }
  const qulonglong directLightAnyHitBatchChunks = jsonIntegerArraySum(
    batching.value(QStringLiteral("directLightAnyHitBatchChunksPerDepth")).toArray());
  if (directLightAnyHitBatchChunks > 0) {
    const qulonglong directLightAnyHitBatchRays = jsonIntegerArraySum(
      batching.value(QStringLiteral("directLightAnyHitBatchRaysPerDepth")).toArray());
    addDetailRow(rows, QStringLiteral("Direct-light any-hit batch average rays"),
                 average(directLightAnyHitBatchRays, directLightAnyHitBatchChunks));
  }
  addDetailBoolMetadataRow(rows, QStringLiteral("Prefers closest-hit batches"), batching,
                           QStringLiteral("intersectionBackendPrefersClosestHitBatch"));
  addDetailBoolMetadataRow(rows, QStringLiteral("Prefers any-hit batches"), batching,
                           QStringLiteral("intersectionBackendPrefersAnyHitBatch"));
  addDetailMillisecondsMetadataRow(rows, QStringLiteral("Backend upload time"), batching,
                                   QStringLiteral("intersectionBackendUploadWorkerSeconds"));
  addDetailMillisecondsMetadataRow(rows, QStringLiteral("Backend kernel time"), batching,
                                   QStringLiteral("intersectionBackendKernelWorkerSeconds"));
  addDetailMillisecondsMetadataRow(rows, QStringLiteral("Backend readback time"), batching,
                                   QStringLiteral("intersectionBackendReadbackWorkerSeconds"));
}

RenderGraphInspectorWidget::DetailRows
RenderGraphInspectorWidget::Private::passDetailRows(const RenderPlan& plan,
                                                    const RenderPassId& passId) const {
  DetailRows rows;
  const RenderPassNode* pass = plan.findPass(passId);
  if (!pass) {
    addDetailRow(rows, QStringLiteral("Pass"), qstr(passId));
    addDetailRow(rows, QStringLiteral("Status"), QStringLiteral("not found"));
    return rows;
  }

  addDetailRow(rows, QStringLiteral("Pass"), qstr(pass->id));
  addDetailRow(rows, QStringLiteral("Name"), displayName(*pass));
  addDetailRow(rows, QStringLiteral("Kind"), displayText(pass->kind));
  addDetailRow(rows, QStringLiteral("Executor"), displayText(pass->executor));
  addDetailRow(rows, QStringLiteral("Enabled"),
               pass->enabled ? QStringLiteral("true") : QStringLiteral("false"));
  const auto stage = plan.executionStageNumber(pass->id);
  addDetailRow(rows, QStringLiteral("Execution stage"),
               stage ? QString::number(*stage) : QStringLiteral("-"));
  const auto order = plan.executionOrderNumber(pass->id);
  addDetailRow(rows, QStringLiteral("Execution order"),
               order ? QString::number(*order) : QStringLiteral("-"));
  addDetailRow(rows, QStringLiteral("Scene selector"), sceneSelectorText(pass->sceneView.selector));
  addDetailRow(rows, QStringLiteral("Scene camera"), cameraText(pass->sceneView.camera));
  addDetailRow(rows, QStringLiteral("Shading profile"),
               shadingProfileText(pass->sceneView.shadingProfile));
  addDetailRow(rows, QStringLiteral("Disabled behavior"), displayText(pass->disabledBehavior));
  QStringList features;
  for (const auto& feature : pass->features)
    features << displayFeatureText(feature);
  addDetailRow(rows, QStringLiteral("Features"), features.join(QStringLiteral(", ")));
  addDetailRow(rows, QStringLiteral("Reads"), resourceReads(plan, pass->reads));
  addDetailRow(rows, QStringLiteral("Writes"), resourceWrites(plan, pass->writes));
  addDetailRow(rows, QStringLiteral("Incoming dependencies"),
               dependencySummary(plan, plan.dependenciesInto(pass->id)));
  addDetailRow(rows, QStringLiteral("Outgoing dependencies"),
               dependencySummary(plan, plan.dependenciesOutOf(pass->id)));
  addDetailRow(rows, QStringLiteral("External side effects"),
               pass->hasExternalSideEffects ? QStringLiteral("true") : QStringLiteral("false"));
  addDetailRow(rows, QStringLiteral("Concurrent"),
               pass->canRunConcurrently ? QStringLiteral("true") : QStringLiteral("false"));
  addDetailRow(rows, QStringLiteral("State"), passStateText(*pass));

  const RenderGraphExecutionTrace* matchedTrace =
    trace && trace->matchesPlan(plan) ? trace.get() : nullptr;
  const RenderPassTrace* passTrace = matchedTrace ? matchedTrace->findPass(pass->id) : nullptr;
  if (!passTrace) {
    addDetailRow(rows, QStringLiteral("Trace"), QStringLiteral("not available"));
    return rows;
  }

  addDetailRow(rows, QStringLiteral("Trace status"), toString(passTrace->status()));
  addDetailRow(rows, QStringLiteral("Trace elapsed"),
               QStringLiteral("%1 ms").arg(passTrace->elapsed().count() / 1000000.0, 0, 'f', 3));
  addDetailRow(rows, QStringLiteral("Trace message"), qstr(passTrace->message()));

  QStringList inputs;
  for (const auto& input : passTrace->inputs())
    inputs << qstr(input.resourceId());
  addDetailRow(rows, QStringLiteral("Trace inputs"), inputs.join(QStringLiteral(", ")));

  QStringList outputs;
  for (const auto& output : passTrace->outputs())
    outputs << qstr(output.resourceId());
  addDetailRow(rows, QStringLiteral("Trace outputs"), outputs.join(QStringLiteral(", ")));

  addIntersectionBackendDetailRows(
    rows, passTrace->metadata().value(QStringLiteral("batching")).toObject());
  return rows;
}

QString RenderGraphInspectorWidget::Private::passTraceLine(const RenderPassNode& pass) const {
  if (!trace)
    return QString();

  const auto* passTrace = trace->findPass(pass.id);
  if (!passTrace)
    return QString();

  QString line = QStringLiteral("%1, %2 ms")
                   .arg(toString(passTrace->status()))
                   .arg(passTrace->elapsed().count() / 1000000.0, 0, 'f', 2);
  const QJsonObject fragments = passTrace->metadata().value(QStringLiteral("fragments")).toObject();
  if (!fragments.isEmpty()) {
    line +=
      QStringLiteral(", shaded %1, writes %2")
        .arg(static_cast<qulonglong>(fragments.value(QStringLiteral("shadedFragments")).toDouble()))
        .arg(static_cast<qulonglong>(fragments.value(QStringLiteral("colorWrites")).toDouble()));
  }
  const QJsonObject scheduling =
    passTrace->metadata().value(QStringLiteral("scheduling")).toObject();
  if (!scheduling.isEmpty()) {
    line += QStringLiteral(", queue %1 %2")
              .arg(static_cast<qulonglong>(
                scheduling.value(QStringLiteral("resolvedQueueSize")).toDouble()))
              .arg(scheduling.value(QStringLiteral("decision")).toString());
  }
  const QJsonObject batching = passTrace->metadata().value(QStringLiteral("batching")).toObject();
  if (!batching.isEmpty()) {
    line +=
      QStringLiteral(", samples %1, %2")
        .arg(static_cast<qulonglong>(batching.value(QStringLiteral("samplesSubmitted")).toDouble()))
        .arg(humanizeIdentifier(batching.value(QStringLiteral("executionMode")).toString()));
    const auto compatibilitySamples = static_cast<qulonglong>(
      batching.value(QStringLiteral("compatibilityShadeSamples")).toDouble());
    if (compatibilitySamples > 0) {
      line += QStringLiteral(", compatibility shade %1").arg(compatibilitySamples);
    }
    const auto unsupportedPathMaterials = static_cast<qulonglong>(
      batching.value(QStringLiteral("unsupportedPathMaterialSamples")).toDouble());
    if (unsupportedPathMaterials > 0) {
      line += QStringLiteral(", unsupported path materials %1").arg(unsupportedPathMaterials);
    }
    const auto emitterHits =
      static_cast<qulonglong>(batching.value(QStringLiteral("emitterHitSamples")).toDouble());
    if (emitterHits > 0) {
      const auto misWeightedEmitterHits = static_cast<qulonglong>(
        batching.value(QStringLiteral("misWeightedEmitterHitSamples")).toDouble());
      line += QStringLiteral(", emitters %1").arg(emitterHits);
      if (misWeightedEmitterHits > 0) {
        line += QStringLiteral(" (%1 MIS)").arg(misWeightedEmitterHits);
      }
    }
    const auto directLightSamples =
      static_cast<qulonglong>(batching.value(QStringLiteral("directLightSamples")).toDouble());
    if (directLightSamples > 0) {
      const auto directLightContributing = static_cast<qulonglong>(
        batching.value(QStringLiteral("directLightContributingSamples")).toDouble());
      const auto directLightOccluded = static_cast<qulonglong>(
        batching.value(QStringLiteral("directLightOccludedSamples")).toDouble());
      line += QStringLiteral(", direct light %1 (%2 lit, %3 shadowed)")
                .arg(directLightSamples)
                .arg(directLightContributing)
                .arg(directLightOccluded);
    }
    const double directLightLuminance =
      batching.value(QStringLiteral("directLightRadianceLuminanceSum")).toDouble();
    const double emittedLuminance =
      batching.value(QStringLiteral("emittedRadianceLuminanceSum")).toDouble();
    const double missLuminance =
      batching.value(QStringLiteral("missRadianceLuminanceSum")).toDouble();
    const double ambientLuminance =
      batching.value(QStringLiteral("ambientRadianceLuminanceSum")).toDouble();
    const double compatibilityLuminance =
      batching.value(QStringLiteral("compatibilityShadeRadianceLuminanceSum")).toDouble();
    if (directLightLuminance > 0.0 || emittedLuminance > 0.0 || missLuminance > 0.0 ||
        ambientLuminance > 0.0 || compatibilityLuminance > 0.0) {
      line += QStringLiteral(", luminance direct %1, emitted %2, miss %3, ambient %4, compat %5")
                .arg(directLightLuminance, 0, 'g', 3)
                .arg(emittedLuminance, 0, 'g', 3)
                .arg(missLuminance, 0, 'g', 3)
                .arg(ambientLuminance, 0, 'g', 3)
                .arg(compatibilityLuminance, 0, 'g', 3);
    }
    const QString intersectionBackendRequest =
      batching.value(QStringLiteral("intersectionBackendRequest")).toString();
    const QString intersectionBackend =
      batching.value(QStringLiteral("intersectionBackend")).toString();
    if (!intersectionBackendRequest.isEmpty() || !intersectionBackend.isEmpty()) {
      line += QStringLiteral(", intersection backend ");
      if (!intersectionBackendRequest.isEmpty() &&
          intersectionBackendRequest != intersectionBackend) {
        line += QStringLiteral("%1->%2").arg(intersectionBackendRequest).arg(intersectionBackend);
      } else {
        line += intersectionBackend.isEmpty() ? intersectionBackendRequest : intersectionBackend;
      }
      const QString availability =
        batching.value(QStringLiteral("intersectionBackendAvailability")).toString();
      if (!availability.isEmpty() && availability != QStringLiteral("available")) {
        line += QStringLiteral(" %1").arg(availability);
      }
      const QString platform =
        batching.value(QStringLiteral("intersectionBackendPlatform")).toString();
      if (!platform.isEmpty()) {
        line += QStringLiteral(" (%1)").arg(platform);
      }
      QString executionPath =
        batching.value(QStringLiteral("intersectionBackendExecutionPath")).toString();
      if (!executionPath.isEmpty()) {
        executionPath.replace(QChar('_'), QChar(' '));
        line += QStringLiteral(" via %1").arg(executionPath);
      }
      const qulonglong expectedRays =
        jsonIntegerValue(batching, QStringLiteral("intersectionBackendExpectedRays"));
      if (expectedRays > 0) {
        line += QStringLiteral(", expected %1 intersection rays").arg(expectedRays);
        const qulonglong expectedClosestHitRays =
          jsonIntegerValue(batching, QStringLiteral("intersectionBackendExpectedClosestHitRays"));
        const qulonglong expectedAnyHitRays =
          jsonIntegerValue(batching, QStringLiteral("intersectionBackendExpectedAnyHitRays"));
        if (expectedClosestHitRays > 0 || expectedAnyHitRays > 0) {
          line += QStringLiteral(" (%1 closest-hit, %2 any-hit)")
                    .arg(expectedClosestHitRays)
                    .arg(expectedAnyHitRays);
        }
      }
      const qulonglong autoMinimumGpuRays =
        jsonIntegerValue(batching, QStringLiteral("intersectionBackendAutoMinimumGpuRays"));
      if (intersectionBackendRequest == QStringLiteral("auto") && autoMinimumGpuRays > 0) {
        line += QStringLiteral(", auto GPU threshold %1 rays").arg(autoMinimumGpuRays);
      }
      const qulonglong autoQueryTransferBytes = jsonIntegerValue(
        batching, QStringLiteral("intersectionBackendAutoEstimatedQueryTransferBytes"));
      if (intersectionBackendRequest == QStringLiteral("auto") && autoQueryTransferBytes > 0) {
        line +=
          QStringLiteral(", auto estimates %1 query transfer bytes").arg(autoQueryTransferBytes);
      }
      if (batching.contains(QStringLiteral("intersectionBackendPlatformGpuDeviceAvailable"))) {
        line +=
          QStringLiteral(", GPU device %1")
            .arg(batching.value(QStringLiteral("intersectionBackendPlatformGpuDeviceAvailable"))
                     .toBool()
                   ? QStringLiteral("yes")
                   : QStringLiteral("no"));
      }
      if (batching.contains(QStringLiteral("intersectionBackendPlatformGpuRenderPathAvailable"))) {
        line +=
          QStringLiteral(", GPU render path %1")
            .arg(batching.value(QStringLiteral("intersectionBackendPlatformGpuRenderPathAvailable"))
                     .toBool()
                   ? QStringLiteral("yes")
                   : QStringLiteral("no"));
      }
      const QString fallbackReason =
        batching.value(QStringLiteral("intersectionBackendFallbackReason")).toString();
      if (!fallbackReason.isEmpty()) {
        line += QStringLiteral(" (%1)").arg(fallbackReason);
      }
      if (batching.value(QStringLiteral("intersectionSceneCompiled")).toBool()) {
        line += QStringLiteral(", intersection scene %1 primitives/%2 BVH nodes")
                  .arg(jsonIntegerValue(batching, QStringLiteral("intersectionScenePrimitives")))
                  .arg(jsonIntegerValue(batching, QStringLiteral("intersectionSceneBvhNodes")));
        const QString payloadSummary = intersectionScenePayloadSummary(batching);
        if (!payloadSummary.isEmpty()) {
          line += QStringLiteral(" (%1)").arg(payloadSummary);
        }
        const QString unsupportedReasonSummary = jsonIntegerObjectSummary(
          batching.value(QStringLiteral("intersectionSceneUnsupportedReasons")).toObject());
        if (!unsupportedReasonSummary.isEmpty()) {
          line += QStringLiteral(", unsupported reasons %1").arg(unsupportedReasonSummary);
        }
        const qulonglong uploadBytes =
          jsonIntegerValue(batching, QStringLiteral("intersectionSceneUploadBytes"));
        if (uploadBytes > 0) {
          line += QStringLiteral(", %1 upload bytes").arg(uploadBytes);
        }
        if (batching.value(QStringLiteral("intersectionSceneTriangleClosestHitEligible"))
              .toBool()) {
          line += QStringLiteral(", triangle kernel eligible");
        }
        if (batching.value(QStringLiteral("intersectionSceneBasicHitEligible")).toBool()) {
          line += QStringLiteral(", basic hit kernel eligible");
        }
        if (batching.value(QStringLiteral("intersectionScenePackedClosestHitEligible")).toBool()) {
          line += QStringLiteral(", packed closest-hit eligible");
        }
        if (batching.value(QStringLiteral("intersectionScenePackedAnyHitEligible")).toBool()) {
          line += QStringLiteral(", packed any-hit eligible");
        }
        const qulonglong queryTransferBytes =
          jsonIntegerValue(batching, QStringLiteral("intersectionEstimatedQueryTransferBytes"));
        if (queryTransferBytes > 0) {
          line += QStringLiteral(", ~%1 query transfer bytes").arg(queryTransferBytes);
        }
        const qulonglong closestHitRays =
          jsonIntegerValue(batching, QStringLiteral("closestHitRaysSubmitted"));
        const qulonglong anyHitRays =
          jsonIntegerValue(batching, QStringLiteral("anyHitRaysSubmitted"));
        if (closestHitRays > 0 || anyHitRays > 0) {
          line +=
            QStringLiteral(", %1 closest-hit/%2 any-hit rays").arg(closestHitRays).arg(anyHitRays);
        }
        const bool prefersClosestHitBatch =
          batching.value(QStringLiteral("intersectionBackendPrefersClosestHitBatch")).toBool();
        const bool prefersAnyHitBatch =
          batching.value(QStringLiteral("intersectionBackendPrefersAnyHitBatch")).toBool();
        if (prefersClosestHitBatch || prefersAnyHitBatch) {
          line += QStringLiteral(", batch preference closest-hit %1/any-hit %2")
                    .arg(prefersClosestHitBatch ? QStringLiteral("yes") : QStringLiteral("no"))
                    .arg(prefersAnyHitBatch ? QStringLiteral("yes") : QStringLiteral("no"));
        }
        const double uploadMs =
          batching.value(QStringLiteral("intersectionBackendUploadWorkerSeconds")).toDouble() *
          1000.0;
        const double kernelMs =
          batching.value(QStringLiteral("intersectionBackendKernelWorkerSeconds")).toDouble() *
          1000.0;
        const double readbackMs =
          batching.value(QStringLiteral("intersectionBackendReadbackWorkerSeconds")).toDouble() *
          1000.0;
        if (uploadMs > 0.0 || kernelMs > 0.0 || readbackMs > 0.0) {
          line += QStringLiteral(", backend time upload %1 ms/kernel %2 ms/readback %3 ms")
                    .arg(uploadMs, 0, 'f', 3)
                    .arg(kernelMs, 0, 'f', 3)
                    .arg(readbackMs, 0, 'f', 3);
        }
      }
    }
    const qulonglong frontierHits =
      jsonIntegerArraySum(batching.value(QStringLiteral("frontierRayHitsPerDepth")).toArray());
    const qulonglong frontierMisses =
      jsonIntegerArraySum(batching.value(QStringLiteral("frontierRayMissesPerDepth")).toArray());
    if (frontierHits > 0 || frontierMisses > 0) {
      line += QStringLiteral(", frontier %1 hit/%2 miss").arg(frontierHits).arg(frontierMisses);
    }
    const qulonglong packetChunks =
      jsonIntegerArraySum(batching.value(QStringLiteral("frontierPacketChunksPerDepth")).toArray());
    const qulonglong packetRays =
      jsonIntegerArraySum(batching.value(QStringLiteral("frontierPacketRaysPerDepth")).toArray());
    const qulonglong closestHitBatchChunks = jsonIntegerArraySum(
      batching.value(QStringLiteral("frontierClosestHitBatchChunksPerDepth")).toArray());
    const qulonglong closestHitBatchRays = jsonIntegerArraySum(
      batching.value(QStringLiteral("frontierClosestHitBatchRaysPerDepth")).toArray());
    const qulonglong directLightAnyHitBatchChunks = jsonIntegerArraySum(
      batching.value(QStringLiteral("directLightAnyHitBatchChunksPerDepth")).toArray());
    const qulonglong directLightAnyHitBatchRays = jsonIntegerArraySum(
      batching.value(QStringLiteral("directLightAnyHitBatchRaysPerDepth")).toArray());
    const qulonglong ray4PacketChunks = jsonIntegerArraySum(
      batching.value(QStringLiteral("frontierRay4PacketChunksPerDepth")).toArray());
    const qulonglong ray8PacketChunks = jsonIntegerArraySum(
      batching.value(QStringLiteral("frontierRay8PacketChunksPerDepth")).toArray());
    const qulonglong scalarRays =
      jsonIntegerArraySum(batching.value(QStringLiteral("frontierScalarRaysPerDepth")).toArray());
    const qulonglong packetScalarFallbackRays = jsonIntegerArraySum(
      batching.value(QStringLiteral("frontierPacketScalarFallbackRaysPerDepth")).toArray());
    const qulonglong packetRefinedRays = jsonIntegerArraySum(
      batching.value(QStringLiteral("frontierPacketRefinedRaysPerDepth")).toArray());
    if (packetChunks > 0 || scalarRays > 0) {
      const double packetLaneCapacity =
        static_cast<double>(ray8PacketChunks) * 8.0 + static_cast<double>(ray4PacketChunks) * 4.0;
      line +=
        QStringLiteral(", packets %1 rays/%2 chunks (Ray8 %3, Ray4 %4), fill %5, scalar tail %6, "
                       "scalar %7/fallback %8 (%9)/refined %10")
          .arg(packetRays)
          .arg(packetChunks)
          .arg(ray8PacketChunks)
          .arg(ray4PacketChunks)
          .arg(percentage(packetRays, packetLaneCapacity))
          .arg(percentage(scalarRays, static_cast<double>(packetRays + scalarRays)))
          .arg(scalarRays)
          .arg(packetScalarFallbackRays)
          .arg(percentage(packetScalarFallbackRays, packetRays))
          .arg(packetRefinedRays);
      const QString refinedByMaterial = jsonIntegerObjectSummary(
        batching.value(QStringLiteral("frontierPacketRefinedRaysByMaterial")).toObject());
      const QString fallbackByReason = jsonIntegerObjectSummary(
        batching.value(QStringLiteral("frontierPacketScalarFallbackRaysByReason")).toObject());
      if (!fallbackByReason.isEmpty()) {
        line += QStringLiteral(" fallback (%1)").arg(fallbackByReason);
      }
      if (!refinedByMaterial.isEmpty()) {
        line += QStringLiteral(" refined (%1)").arg(refinedByMaterial);
      }
    }
    if (closestHitBatchChunks > 0) {
      line += QStringLiteral(", closest-hit batches %1 rays/%2 chunks, avg %3")
                .arg(closestHitBatchRays)
                .arg(closestHitBatchChunks)
                .arg(average(closestHitBatchRays, closestHitBatchChunks));
    }
    if (directLightAnyHitBatchChunks > 0) {
      line += QStringLiteral(", direct-light any-hit batches %1 rays/%2 chunks, avg %3")
                .arg(directLightAnyHitBatchRays)
                .arg(directLightAnyHitBatchChunks)
                .arg(average(directLightAnyHitBatchRays, directLightAnyHitBatchChunks));
    }
  }
  const QJsonObject depthPrepass =
    passTrace->metadata().value(QStringLiteral("depthPrepass")).toObject();
  if (!depthPrepass.isEmpty()) {
    line +=
      QStringLiteral(", prepass %1").arg(depthPrepass.value(QStringLiteral("decision")).toString());
  }
  return line;
}

const RenderGraphResourceSnapshot* RenderGraphInspectorWidget::Private::firstSnapshotForResource(
  const RenderResourceId& resourceId) const {
  if (!trace)
    return nullptr;

  const auto outputs = trace->outputSnapshotsForResource(resourceId);
  if (!outputs.empty())
    return outputs.front();

  const auto inputs = trace->inputSnapshotsForResource(resourceId);
  return inputs.empty() ? nullptr : inputs.front();
}

QString RenderGraphInspectorWidget::Private::resourceTraceLine(
  const RenderResourceDescriptor& resource) const {
  const auto* snapshot = firstSnapshotForResource(resource.id);
  if (!snapshot)
    return QString();

  if (snapshot->cacheMetadata().cacheable()) {
    return QStringLiteral("cache: %1").arg(toString(snapshot->cacheMetadata().status()));
  }
  if (snapshot->hasColorPreview())
    return QStringLiteral("trace: color");
  if (snapshot->hasDepthPreview())
    return QStringLiteral("trace: depth");
  return QStringLiteral("trace: metadata");
}

QString
RenderGraphInspectorWidget::Private::sceneSelectorText(const SceneSelector& selector) const {
  return qstr(selector.displayText());
}

QString RenderGraphInspectorWidget::Private::cameraText(
  const std::optional<RenderCameraRef>& camera) const {
  if (!camera)
    return QStringLiteral("-");

  return qstr(camera->displayText());
}

QString RenderGraphInspectorWidget::Private::shadingProfileText(
  const std::optional<ShadingProfileRef>& shadingProfile) const {
  if (!shadingProfile)
    return QStringLiteral("-");

  return qstr(shadingProfile->displayText());
}

QStringList
RenderGraphInspectorWidget::Private::passSceneViewLines(const RenderPassNode& pass) const {
  QStringList parts;
  if (hasFeature(pass.features, "selector_override")) {
    parts << QStringLiteral("routed selector");
  }
  if (!pass.sceneView.selector.selectsWholeFrame()) {
    parts << QStringLiteral("selector %1").arg(sceneSelectorText(pass.sceneView.selector));
  }
  if (pass.sceneView.camera) {
    parts << QStringLiteral("camera %1").arg(cameraText(pass.sceneView.camera));
  }
  if (pass.sceneView.shadingProfile) {
    parts << QStringLiteral("shading %1").arg(shadingProfileText(pass.sceneView.shadingProfile));
  }
  return parts;
}

QString
RenderGraphInspectorWidget::Private::sizeText(const RenderResourceDescriptor& resource) const {
  return QStringLiteral("%1x%2, %3 sample(s)")
    .arg(resource.width)
    .arg(resource.height)
    .arg(resource.sampleCount);
}

QString RenderGraphInspectorWidget::Private::resourceFeatures(
  const RenderResourceDescriptor& resource) const {
  QStringList values;
  for (const auto& feature : resource.features)
    values << displayFeatureText(feature);
  return dashIfEmpty(values.join(", "));
}

QString
RenderGraphInspectorWidget::Private::resourceReads(const RenderPlan& plan,
                                                   const std::vector<ResourceRead>& reads) const {
  QStringList values;
  for (const auto& read : reads)
    values << displayResourceName(plan, read.resource);
  return dashIfEmpty(values.join(", "));
}

QString RenderGraphInspectorWidget::Private::resourceWrites(
  const RenderPlan& plan, const std::vector<ResourceWrite>& writes) const {
  QStringList values;
  for (const auto& write : writes)
    values << displayResourceName(plan, write.resource);
  return dashIfEmpty(values.join(", "));
}

QString RenderGraphInspectorWidget::Private::dependencySummary(
  const RenderPlan& plan, const std::vector<RenderPassDependency>& dependencies) const {
  QStringList values;
  for (const auto& dependency : dependencies) {
    values << QStringLiteral("%1 -> %2 via %3")
                .arg(displayName(*dependency.producer))
                .arg(displayName(*dependency.consumer))
                .arg(displayResourceName(plan, dependency.resource));
  }
  return dashIfEmpty(values.join(QStringLiteral(", ")));
}

QString RenderGraphInspectorWidget::Private::passTooltip(const RenderPlan& plan,
                                                         const RenderPassNode& pass,
                                                         const QString& executionMessage) const {
  QStringList lines;
  if (!executionMessage.isEmpty()) {
    lines << executionMessage;
  }
  lines << QStringLiteral("Double-click to enable or disable this pass");
  if (hasFeature(pass.features, "selector_override")) {
    lines << QStringLiteral("Selector route: compiler-generated branch for %1")
               .arg(sceneSelectorText(pass.sceneView.selector));
  }
  lines << QStringLiteral("Scene selector: %1").arg(sceneSelectorText(pass.sceneView.selector));
  lines << QStringLiteral("Scene camera: %1").arg(cameraText(pass.sceneView.camera));
  lines
    << QStringLiteral("Shading profile: %1").arg(shadingProfileText(pass.sceneView.shadingProfile));
  lines << QStringLiteral("Pass ID: %1").arg(qstr(pass.id));
  lines << QStringLiteral("Reads: %1").arg(resourceReads(plan, pass.reads));
  lines << QStringLiteral("Writes: %1").arg(resourceWrites(plan, pass.writes));
  lines << QStringLiteral("Incoming dependencies: %1")
             .arg(dependencySummary(plan, plan.dependenciesInto(pass.id)));
  lines << QStringLiteral("Outgoing dependencies: %1")
             .arg(dependencySummary(plan, plan.dependenciesOutOf(pass.id)));
  return lines.join(QStringLiteral("\n"));
}

QString
RenderGraphInspectorWidget::Private::resourceProducer(const RenderPlan& plan,
                                                      const RenderResourceId& resource) const {
  const RenderPassNode* producer = plan.producerOf(resource);
  return producer ? displayName(*producer) : QStringLiteral("-");
}

QString
RenderGraphInspectorWidget::Private::resourceConsumers(const RenderPlan& plan,
                                                       const RenderResourceId& resource) const {
  QStringList values;
  for (const RenderPassNode* consumer : plan.consumersOf(resource))
    values << displayName(*consumer);
  return dashIfEmpty(values.join(", "));
}

QString RenderGraphInspectorWidget::Private::resourceTooltip(
  const RenderPlan& plan, const RenderResourceDescriptor& resource) const {
  QStringList lines;
  lines << QStringLiteral("Resource ID: %1").arg(qstr(resource.id));
  if (hasFeature(resource.features, "selector_override")) {
    lines << QStringLiteral("Selector route: compiler-generated branch resource");
  }
  lines << QStringLiteral("Producer: %1").arg(resourceProducer(plan, resource.id));
  lines << QStringLiteral("Consumers: %1").arg(resourceConsumers(plan, resource.id));
  lines << QStringLiteral("Features: %1").arg(resourceFeatures(resource));
  lines << QStringLiteral("Format: %1").arg(displayText(resource.format));
  lines << QStringLiteral("Lifetime: %1").arg(displayText(resource.lifetime));
  lines << QStringLiteral("Size: %1").arg(sizeText(resource));
  return lines.join(QStringLiteral("\n"));
}

std::map<RenderPassId, QPointF>
RenderGraphInspectorWidget::Private::passPositions(const RenderPlan& plan) const {
  std::map<RenderPassId, QPointF> result;

  const auto stages = plan.executionStages();
  for (std::size_t stageIndex = 0; stageIndex != stages.size(); ++stageIndex) {
    for (std::size_t row = 0; row != stages[stageIndex].size(); ++row) {
      const RenderPassNode* pass = stages[stageIndex][row];
      result[pass->id] = QPointF(OriginX + stageIndex * ColumnGap, OriginY + row * RowGap);
    }
  }

  return result;
}

QPointF RenderGraphInspectorWidget::Private::passCenter(const QPointF& topLeft) const {
  return topLeft + QPointF(PassWidth / 2.0, PassHeight / 2.0);
}

QPointF RenderGraphInspectorWidget::Private::resourcePosition(
  const RenderPlan& plan, const RenderResourceDescriptor& resource,
  const std::map<RenderPassId, QPointF>& passes, int fallbackRow) const {
  const RenderPassNode* producer = plan.producerOf(resource.id);
  const auto consumers = plan.consumersOf(resource.id);

  std::vector<QPointF> anchors;
  if (producer) {
    const auto producerPosition = passes.find(producer->id);
    if (producerPosition != passes.end())
      anchors.push_back(passCenter(producerPosition->second));
  }
  for (const RenderPassNode* consumer : consumers) {
    const auto consumerPosition = passes.find(consumer->id);
    if (consumerPosition != passes.end())
      anchors.push_back(passCenter(consumerPosition->second));
  }

  if (anchors.empty()) {
    return QPointF(OriginX, OriginY + fallbackRow * RowGap);
  }

  const double averageX =
    std::accumulate(anchors.begin(), anchors.end(), 0.0,
                    [](double value, const QPointF& point) { return value + point.x(); }) /
    anchors.size();
  const double averageY =
    std::accumulate(anchors.begin(), anchors.end(), 0.0,
                    [](double value, const QPointF& point) { return value + point.y(); }) /
    anchors.size();

  if (producer && consumers.empty()) {
    return QPointF(averageX + PassWidth / 2.0 + 40.0, averageY - ResourceHeight / 2.0);
  }
  if (!producer && !consumers.empty()) {
    return QPointF(averageX - PassWidth / 2.0 - ResourceWidth - 40.0,
                   averageY - ResourceHeight / 2.0);
  }

  return QPointF(averageX - ResourceWidth / 2.0, averageY - ResourceHeight / 2.0);
}

QGraphicsRectItem* RenderGraphInspectorWidget::Private::addNode(
  QGraphicsScene& scene, const QRectF& rect, const QString& kind, const QString& id,
  const QStringList& lines, const QPen& pen, const QBrush& brush) const {
  auto* item = scene.addRect(rect, pen, brush);
  item->setData(GraphItemKindRole, kind);
  item->setData(GraphItemIdRole, id);
  item->setFlag(QGraphicsItem::ItemIsSelectable);

  double y = rect.top() + 9.0;
  for (int i = 0; i != lines.size(); ++i) {
    QFont font;
    if (i == 0)
      font.setBold(true);
    font.setPointSize(i == 0 ? 9 : 8);
    QFontMetrics metrics(font);
    const QString elided = metrics.elidedText(lines[i], Qt::ElideRight,
                                              static_cast<int>(rect.width() - 2.0 * NodeTextInset));
    auto* text = scene.addSimpleText(elided);
    text->setParentItem(item);
    text->setData(GraphItemKindRole, kind);
    text->setData(GraphItemIdRole, id);
    text->setFont(font);
    text->setToolTip(lines[i]);
    text->setPos(rect.left() + NodeTextInset, y);
    y += text->boundingRect().height() + 2.0;
  }

  return item;
}

void RenderGraphInspectorWidget::Private::addEdge(QGraphicsScene& scene, const QPointF& from,
                                                  const QPointF& to) const {
  QPen pen(QColor(90, 100, 110));
  pen.setWidthF(1.4);
  auto* line = scene.addLine(from.x(), from.y(), to.x(), to.y(), pen);
  line->setZValue(-10.0);
}

QGraphicsItem* RenderGraphInspectorWidget::Private::graphNodeItem(QGraphicsItem* item) const {
  while (item && item->data(GraphItemKindRole).toString().isEmpty())
    item = item->parentItem();
  return item;
}

RenderGraphInspectorWidget::RenderGraphInspectorWidget(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  auto layout = new QVBoxLayout(this);

  p->liveExecutionTimer = new QTimer(this);
  p->liveExecutionTimer->setInterval(50);
  connect(p->liveExecutionTimer, SIGNAL(timeout()), this, SLOT(promotePendingExecutionStates()));

  p->validationStatus = new QLabel(this);
  p->validationStatus->setObjectName("renderGraphValidationStatus");

  auto header = new QHBoxLayout();
  header->addWidget(p->validationStatus, 1);

  p->exportText = new QToolButton(this);
  p->exportText->setObjectName("renderGraphExportText");
  p->exportText->setText(tr("Text"));
  p->exportText->setToolTip(tr("Export the effective render graph as text"));
  connect(p->exportText, SIGNAL(clicked()), this, SLOT(exportTextGraph()));
  header->addWidget(p->exportText);

  p->exportDot = new QToolButton(this);
  p->exportDot->setObjectName("renderGraphExportDot");
  p->exportDot->setText(tr("DOT"));
  p->exportDot->setToolTip(tr("Export the effective render graph as DOT"));
  connect(p->exportDot, SIGNAL(clicked()), this, SLOT(exportDotGraph()));
  header->addWidget(p->exportDot);

  p->exportJson = new QToolButton(this);
  p->exportJson->setObjectName("renderGraphExportJson");
  p->exportJson->setText(tr("JSON"));
  p->exportJson->setToolTip(tr("Export the effective render graph as JSON"));
  connect(p->exportJson, SIGNAL(clicked()), this, SLOT(exportJsonGraph()));
  header->addWidget(p->exportJson);

  auto tabs = new QTabWidget(this);

  p->graph = new QGraphicsView(tabs);
  p->graph->setObjectName("renderGraphView");
  p->graph->setRenderHint(QPainter::Antialiasing);
  p->graph->setDragMode(QGraphicsView::ScrollHandDrag);
  p->graph->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  p->graphScene = new QGraphicsScene(p->graph);
  p->graphScene->setObjectName("renderGraphScene");
  p->graphScene->installEventFilter(this);
  p->graph->setScene(p->graphScene);

  p->passes = new QTreeWidget(tabs);
  p->passes->setObjectName("renderGraphPasses");
  p->passes->setRootIsDecorated(false);
  p->passes->setAlternatingRowColors(true);
  p->passes->setHeaderLabels({tr("Enabled"), tr("Order"), tr("Stage"), tr("Pass"), tr("Trace"),
                              tr("Kind"), tr("Executor"), tr("Selector"), tr("Camera"),
                              tr("Shading"), tr("Reads"), tr("Writes"), tr("Disabled behavior")});
  p->passes->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  p->passes->header()->setStretchLastSection(true);
  connect(p->passes, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this,
          SLOT(passItemChanged(QTreeWidgetItem*, int)));
  connect(p->passes, SIGNAL(itemSelectionChanged()), this, SLOT(passSelectionChanged()));

  p->groups = new QTreeWidget(tabs);
  p->groups->setObjectName("renderGraphGroups");
  p->groups->setRootIsDecorated(false);
  p->groups->setAlternatingRowColors(true);
  p->groups->setHeaderLabels({tr("Enabled"), tr("Scope"), tr("Value")});
  p->groups->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  p->groups->header()->setStretchLastSection(true);
  connect(p->groups, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this,
          SLOT(groupItemChanged(QTreeWidgetItem*, int)));

  p->resources = new QTreeWidget(tabs);
  p->resources->setObjectName("renderGraphResources");
  p->resources->setRootIsDecorated(false);
  p->resources->setAlternatingRowColors(true);
  p->resources->setHeaderLabels({tr("Resource"), tr("Producer"), tr("Consumers"), tr("Type"),
                                 tr("Features"), tr("Format"), tr("Domain"), tr("Lifetime"),
                                 tr("Size")});
  p->resources->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  p->resources->header()->setStretchLastSection(true);
  connect(p->resources, SIGNAL(itemSelectionChanged()), this, SLOT(resourceSelectionChanged()));

  tabs->addTab(p->graph, tr("Graph"));
  tabs->addTab(p->passes, tr("Passes"));
  tabs->addTab(p->groups, tr("Groups"));
  tabs->addTab(p->resources, tr("Resources"));

  layout->addLayout(header);
  layout->addWidget(tabs, 1);
  setLayout(layout);

  updateValidationStatus();
}

RenderGraphInspectorWidget::~RenderGraphInspectorWidget() {
  p->liveExecutionTimer->stop();
  p->graphScene->removeEventFilter(this);
}

QSize RenderGraphInspectorWidget::sizeHint() const {
  return QSize(720, 220);
}

void RenderGraphInspectorWidget::setPlan(const RenderPlan& plan) {
  p->plan = plan;
  p->compileError.clear();
  if (p->trace && !p->trace->matchesPlan(effectivePlan()))
    p->trace.reset();
  p->liveExecutionTimer->stop();
  p->pendingExecutionStarts.clear();
  p->executionStates.clear();
  p->executionMessages.clear();

  const auto ids = p->plan.passIds();
  if (ids.find(p->selectedPassId) == ids.end()) {
    if (p->hasSelection && p->selectedResourceId.empty())
      p->hasSelection = false;
    p->selectedPassId = p->plan.passes().empty() ? RenderPassId() : p->plan.passes().front().id;
  }
  if (!p->plan.hasResource(p->selectedResourceId)) {
    if (p->hasSelection && !p->selectedResourceId.empty())
      p->hasSelection = false;
    p->selectedResourceId.clear();
  }
  for (auto it = p->overrides.disabledPasses.begin(); it != p->overrides.disabledPasses.end();) {
    if (ids.find(*it) == ids.end()) {
      it = p->overrides.disabledPasses.erase(it);
    } else {
      ++it;
    }
  }
  const auto kinds = p->plan.passKinds();
  for (auto it = p->overrides.disabledPassKinds.begin();
       it != p->overrides.disabledPassKinds.end();) {
    if (kinds.find(*it) == kinds.end()) {
      it = p->overrides.disabledPassKinds.erase(it);
    } else {
      ++it;
    }
  }
  const auto executors = p->plan.passExecutors();
  for (auto it = p->overrides.disabledExecutors.begin();
       it != p->overrides.disabledExecutors.end();) {
    if (executors.find(*it) == executors.end()) {
      it = p->overrides.disabledExecutors.erase(it);
    } else {
      ++it;
    }
  }
  const auto features = p->plan.passFeatures();
  for (auto it = p->overrides.disabledFeatures.begin();
       it != p->overrides.disabledFeatures.end();) {
    if (features.find(*it) == features.end()) {
      it = p->overrides.disabledFeatures.erase(it);
    } else {
      ++it;
    }
  }

  rebuildAllViews();
}

void RenderGraphInspectorWidget::setError(const QString& message) {
  p->plan = RenderPlan();
  p->compileError = message;
  p->trace.reset();
  p->liveExecutionTimer->stop();
  p->pendingExecutionStarts.clear();
  p->executionStates.clear();
  p->executionMessages.clear();
  p->selectedPassId.clear();
  p->selectedResourceId.clear();
  p->hasSelection = false;
  p->overrides = RenderGraphOverrides();
  rebuildAllViews();
}

RenderGraphOverrides RenderGraphInspectorWidget::overrides() const {
  return p->overrides;
}

RenderPlan RenderGraphInspectorWidget::effectivePlan() const {
  return p->plan.withOverrides(p->overrides);
}

bool RenderGraphInspectorWidget::effectivePlanValid() const {
  if (!p->compileError.isEmpty())
    return false;
  return effectivePlan().validate().valid();
}

RenderGraphInspectorWidget::DetailRows
RenderGraphInspectorWidget::passDetailRows(const QString& passId) const {
  return p->passDetailRows(effectivePlan(), passId.toStdString());
}

void RenderGraphInspectorWidget::setExecutionTrace(
  std::shared_ptr<const RenderGraphExecutionTrace> trace) {
  p->trace = trace && trace->matchesPlan(effectivePlan()) ? std::move(trace) : nullptr;
  rebuildGraph();
  rebuildPasses();

  if (p->hasSelection && !p->selectedResourceId.empty()) {
    emit selectedResourceTraceChanged(qstr(p->selectedResourceId));
  } else if (p->hasSelection && !p->selectedPassId.empty()) {
    emit selectedPassTraceChanged(qstr(p->selectedPassId));
  }
}

void RenderGraphInspectorWidget::clearExecutionState() {
  if (p->pendingExecutionStarts.empty() && p->executionStates.empty() &&
      p->executionMessages.empty())
    return;

  p->liveExecutionTimer->stop();
  p->pendingExecutionStarts.clear();
  p->executionStates.clear();
  p->executionMessages.clear();
  rebuildGraph();
}

void RenderGraphInspectorWidget::passExecutionStarted(const QString& passId) {
  const RenderPassId id = passId.toStdString();
  p->pendingExecutionStarts[id] = std::chrono::steady_clock::now();
  p->executionMessages.erase(id);
  if (!p->liveExecutionTimer->isActive())
    p->liveExecutionTimer->start();
}

void RenderGraphInspectorWidget::passExecutionFinished(const QString& passId) {
  const RenderPassId id = passId.toStdString();
  if (p->pendingExecutionStarts.erase(id) != 0) {
    if (p->pendingExecutionStarts.empty())
      p->liveExecutionTimer->stop();
    return;
  }

  p->executionStates[id] = PassExecutionState::Completed;
  p->executionMessages.erase(id);
  rebuildGraph();
}

void RenderGraphInspectorWidget::passExecutionFailed(const QString& passId,
                                                     const QString& message) {
  const RenderPassId id = passId.toStdString();
  p->pendingExecutionStarts.erase(id);
  if (p->pendingExecutionStarts.empty())
    p->liveExecutionTimer->stop();
  p->executionStates[id] = PassExecutionState::Failed;
  p->executionMessages[id] = message;
  rebuildGraph();
}

void RenderGraphInspectorWidget::setActiveExecutionPasses(const QStringList& passIds) {
  std::set<RenderPassId> active;
  for (const auto& passId : passIds)
    active.insert(passId.toStdString());

  const auto now = std::chrono::steady_clock::now();
  bool changed = false;
  for (const auto& id : active) {
    const auto stateIt = p->executionStates.find(id);
    if (stateIt != p->executionStates.end() &&
        stateIt->second == PassExecutionState::Running) {
      continue;
    }
    if (p->pendingExecutionStarts.find(id) == p->pendingExecutionStarts.end()) {
      p->pendingExecutionStarts[id] = now;
      changed = true;
    }
    p->executionMessages.erase(id);
  }

  for (auto it = p->pendingExecutionStarts.begin(); it != p->pendingExecutionStarts.end();) {
    if (active.find(it->first) == active.end()) {
      it = p->pendingExecutionStarts.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }

  for (auto it = p->executionStates.begin(); it != p->executionStates.end();) {
    if (it->second == PassExecutionState::Running && active.find(it->first) == active.end()) {
      it = p->executionStates.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }

  if (p->pendingExecutionStarts.empty())
    p->liveExecutionTimer->stop();
  else if (!p->liveExecutionTimer->isActive())
    p->liveExecutionTimer->start();

  if (changed)
    rebuildGraph();
}

void RenderGraphInspectorWidget::passItemChanged(QTreeWidgetItem* item, int column) {
  if (p->updating || column != 0 || !item)
    return;

  const auto passId = item->data(0, Qt::UserRole).toString().toStdString();
  setPassEnabledOverride(passId, item->checkState(0) == Qt::Checked, false);
}

void RenderGraphInspectorWidget::groupItemChanged(QTreeWidgetItem* item, int column) {
  if (p->updating || column != 0 || !item)
    return;

  const QString scope = item->data(0, GroupScopeRole).toString();
  const bool enabled = item->checkState(0) == Qt::Checked;
  if (scope == QStringLiteral("kind")) {
    const auto value = static_cast<RenderPassKind>(item->data(0, GroupValueRole).toInt());
    if (enabled) {
      p->overrides.disabledPassKinds.erase(value);
    } else {
      p->overrides.disabledPassKinds.insert(value);
    }
  } else if (scope == QStringLiteral("executor")) {
    const auto value = static_cast<RenderExecutorKind>(item->data(0, GroupValueRole).toInt());
    if (enabled) {
      p->overrides.disabledExecutors.erase(value);
    } else {
      p->overrides.disabledExecutors.insert(value);
    }
  } else if (scope == QStringLiteral("feature")) {
    const auto value = item->data(0, GroupValueRole).toString().toStdString();
    if (enabled) {
      p->overrides.disabledFeatures.erase(value);
    } else {
      p->overrides.disabledFeatures.insert(value);
    }
  }

  if (p->trace && !p->trace->matchesPlan(effectivePlan()))
    p->trace.reset();
  rebuildGraph();
  rebuildPasses();
  rebuildResources();
  updateValidationStatus();
  emit overridesChanged();
}

void RenderGraphInspectorWidget::passSelectionChanged() {
  if (p->updating || !p->passes->currentItem())
    return;

  selectPass(p->passes->currentItem()->data(0, Qt::UserRole).toString().toStdString());
}

void RenderGraphInspectorWidget::resourceSelectionChanged() {
  if (p->updating || !p->resources->currentItem())
    return;

  selectResource(p->resources->currentItem()->data(0, Qt::UserRole).toString().toStdString());
}

void RenderGraphInspectorWidget::promotePendingExecutionStates() {
  const auto now = std::chrono::steady_clock::now();
  bool changed = false;
  for (auto it = p->pendingExecutionStarts.begin(); it != p->pendingExecutionStarts.end();) {
    if (now - it->second < LiveExecutionDelay) {
      ++it;
      continue;
    }

    p->executionStates[it->first] = PassExecutionState::Running;
    it = p->pendingExecutionStarts.erase(it);
    changed = true;
  }

  if (p->pendingExecutionStarts.empty())
    p->liveExecutionTimer->stop();
  if (changed)
    rebuildGraph();
}

void RenderGraphInspectorWidget::exportTextGraph() {
  emit graphExportRequested(QStringLiteral("text"),
                            QByteArray::fromStdString(effectivePlan().toText()));
}

void RenderGraphInspectorWidget::exportDotGraph() {
  emit graphExportRequested(QStringLiteral("dot"),
                            QByteArray::fromStdString(effectivePlan().toDot()));
}

void RenderGraphInspectorWidget::exportJsonGraph() {
  emit graphExportRequested(
    QStringLiteral("json"),
    QJsonDocument(effectivePlan().toJson()).toJson(QJsonDocument::Indented));
}

bool RenderGraphInspectorWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == p->graphScene && event->type() == QEvent::GraphicsSceneMousePress) {
    auto* mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
    QGraphicsItem* item =
      p->graphNodeItem(p->graphScene->itemAt(mouseEvent->scenePos(), QTransform()));
    if (item && item->data(GraphItemKindRole).toString() == QStringLiteral("pass")) {
      selectPass(item->data(GraphItemIdRole).toString().toStdString());
    } else if (item && item->data(GraphItemKindRole).toString() == QStringLiteral("resource")) {
      selectResource(item->data(GraphItemIdRole).toString().toStdString());
    }
  }

  if (watched == p->graphScene && event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
    auto* mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
    QGraphicsItem* item =
      p->graphNodeItem(p->graphScene->itemAt(mouseEvent->scenePos(), QTransform()));
    if (item && item->data(GraphItemKindRole).toString() == QStringLiteral("pass")) {
      const RenderPassId passId = item->data(GraphItemIdRole).toString().toStdString();
      const RenderPlan plan = effectivePlan();
      const RenderPassNode* pass = plan.findPass(passId);
      setPassEnabledOverride(passId, pass ? !pass->enabled : true);
      return true;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void RenderGraphInspectorWidget::rebuildAllViews() {
  rebuildGraph();
  rebuildPasses();
  rebuildGroups();
  rebuildResources();
  updateValidationStatus();
}

void RenderGraphInspectorWidget::selectPass(const RenderPassId& passId) {
  p->hasSelection = true;
  p->selectedResourceId.clear();
  if (p->selectedPassId == passId) {
    rebuildGraph();
    emit passSelected(qstr(passId));
    return;
  }

  p->selectedPassId = passId;
  for (QGraphicsItem* item : p->graphScene->items()) {
    if (!item->parentItem()) {
      const QString kind = item->data(GraphItemKindRole).toString();
      item->setSelected(kind == QStringLiteral("pass") &&
                        item->data(GraphItemIdRole).toString().toStdString() == passId);
    }
  }
  p->updating = true;
  for (int row = 0; row != p->passes->topLevelItemCount(); ++row) {
    QTreeWidgetItem* item = p->passes->topLevelItem(row);
    item->setSelected(item->data(0, Qt::UserRole).toString().toStdString() == passId);
  }
  p->resources->clearSelection();
  p->updating = false;
  rebuildGraph();
  emit passSelected(qstr(passId));
}

void RenderGraphInspectorWidget::selectResource(const RenderResourceId& resourceId) {
  p->hasSelection = true;
  p->selectedPassId.clear();
  p->selectedResourceId = resourceId;
  for (QGraphicsItem* item : p->graphScene->items()) {
    if (!item->parentItem()) {
      const QString kind = item->data(GraphItemKindRole).toString();
      item->setSelected(kind == QStringLiteral("resource") &&
                        item->data(GraphItemIdRole).toString().toStdString() == resourceId);
    }
  }
  p->updating = true;
  p->passes->clearSelection();
  for (int row = 0; row != p->resources->topLevelItemCount(); ++row) {
    QTreeWidgetItem* item = p->resources->topLevelItem(row);
    item->setSelected(item->data(0, Qt::UserRole).toString().toStdString() == resourceId);
  }
  p->updating = false;
  rebuildGraph();
  emit resourceSelected(qstr(resourceId));
}

void RenderGraphInspectorWidget::setPassEnabledOverride(const RenderPassId& passId, bool enabled,
                                                        bool rebuildPassRows) {
  p->liveExecutionTimer->stop();
  p->pendingExecutionStarts.clear();
  p->executionStates.clear();
  p->executionMessages.clear();
  if (enabled) {
    p->overrides.disabledPasses.erase(passId);
  } else {
    p->overrides.disabledPasses.insert(passId);
  }
  if (p->trace && !p->trace->matchesPlan(effectivePlan()))
    p->trace.reset();

  rebuildGraph();
  if (rebuildPassRows)
    rebuildPasses();
  rebuildGroups();
  rebuildResources();
  updateValidationStatus();
  emit overridesChanged();
}

void RenderGraphInspectorWidget::rebuildGraph() {
  p->graphScene->clear();

  const RenderPlan plan = effectivePlan();
  const RenderGraphExecutionTrace* trace =
    p->trace && p->trace->matchesPlan(plan) ? p->trace.get() : nullptr;
  const auto passLocations = p->passPositions(plan);
  std::map<RenderResourceId, QPointF> resourceLocations;

  for (std::size_t index = 0; index != plan.resources().size(); ++index) {
    const auto& resource = plan.resources()[index];
    resourceLocations.emplace(
      resource.id, p->resourcePosition(plan, resource, passLocations, static_cast<int>(index)));
  }

  for (const auto& resource : plan.resources()) {
    const auto resourceIt = resourceLocations.find(resource.id);
    if (resourceIt == resourceLocations.end())
      continue;

    const QPointF resourcePosition = resourceIt->second;
    const QPointF resourceLeft = resourcePosition + QPointF(0.0, ResourceHeight / 2.0);
    const QPointF resourceRight = resourcePosition + QPointF(ResourceWidth, ResourceHeight / 2.0);

    const RenderPassNode* producer = plan.producerOf(resource.id);
    if (producer) {
      const auto producerIt = passLocations.find(producer->id);
      if (producerIt != passLocations.end()) {
        p->addEdge(*p->graphScene, producerIt->second + QPointF(PassWidth, PassHeight / 2.0),
                   resourceLeft);
      }
    }

    for (const RenderPassNode* consumer : plan.consumersOf(resource.id)) {
      const auto consumerIt = passLocations.find(consumer->id);
      if (consumerIt != passLocations.end()) {
        p->addEdge(*p->graphScene, resourceRight,
                   consumerIt->second + QPointF(0.0, PassHeight / 2.0));
      }
    }
  }

  for (const auto& resource : plan.resources()) {
    const auto location = resourceLocations.find(resource.id);
    if (location == resourceLocations.end())
      continue;

    QPen resourcePen(QColor(80, 95, 110));
    resourcePen.setWidthF(resource.id == p->selectedResourceId ? 2.5 : 1.2);
    QStringList lines{p->displayName(resource),
                      p->displayText(resource.type) + QStringLiteral("/") +
                        p->displayText(resource.format),
                      p->displayText(resource.lifetime), p->sizeText(resource)};
    if (hasFeature(resource.features, "selector_override"))
      lines << tr("routed selector");
    const QString traceLine = trace ? p->resourceTraceLine(resource) : QString();
    if (!traceLine.isEmpty())
      lines << traceLine;
    p->addNode(
       *p->graphScene, QRectF(location->second, QSizeF(ResourceWidth, ResourceHeight)),
       QStringLiteral("resource"), qstr(resource.id), lines, resourcePen,
       QBrush(resource.id == p->selectedResourceId ? QColor(226, 237, 247) : QColor(235, 241, 246)))
      ->setToolTip(p->resourceTooltip(plan, resource));
  }

  for (const auto& pass : plan.passes()) {
    const auto location = passLocations.find(pass.id);
    if (location == passLocations.end())
      continue;

    PassExecutionState executionState = PassExecutionState::Idle;
    const auto executionStateIt = p->executionStates.find(pass.id);
    if (executionStateIt != p->executionStates.end())
      executionState = executionStateIt->second;

    QPen pen(pass.enabled ? QColor(35, 75, 115) : QColor(130, 130, 130));
    QBrush brush(pass.enabled ? QColor(222, 235, 248) : QColor(235, 235, 235));
    if (pass.enabled && executionState == PassExecutionState::Running) {
      pen = QPen(QColor(180, 125, 20));
      brush = QBrush(QColor(255, 244, 204));
    } else if (pass.enabled && executionState == PassExecutionState::Completed) {
      pen = QPen(QColor(50, 120, 80));
      brush = QBrush(QColor(224, 242, 232));
    } else if (executionState == PassExecutionState::Failed) {
      pen = QPen(QColor(170, 60, 60));
      brush = QBrush(QColor(248, 226, 226));
    }
    pen.setWidthF(pass.id == p->selectedPassId ? 2.5 : 1.5);
    if (!pass.enabled)
      pen.setStyle(Qt::DashLine);

    QStringList lines{p->displayName(pass),
                      p->displayText(pass.kind) + QStringLiteral("/") +
                        p->displayText(pass.executor),
                      pass.enabled ? tr("enabled") : tr("disabled")};
    lines << p->passSceneViewLines(pass);
    const auto stage = plan.executionStageNumber(pass.id);
    const auto order = plan.executionOrderNumber(pass.id);
    if (stage && order) {
      lines << tr("stage %1, order %2").arg(*stage).arg(*order);
    }
    const QString traceLine = trace ? p->passTraceLine(pass) : QString();
    if (!traceLine.isEmpty())
      lines << traceLine;
    QGraphicsRectItem* item =
      p->addNode(*p->graphScene, QRectF(location->second, QSizeF(PassWidth, PassHeight)),
                 QStringLiteral("pass"), qstr(pass.id), lines, pen, brush);
    item->setData(GraphItemExecutionStateRole, p->executionStateName(executionState));
    if (pass.id == p->selectedPassId)
      item->setSelected(true);
    const auto messageIt = p->executionMessages.find(pass.id);
    item->setToolTip(p->passTooltip(
      plan, pass, messageIt == p->executionMessages.end() ? QString() : messageIt->second));
  }

  const QRectF bounds = p->graphScene->itemsBoundingRect().adjusted(-40.0, -40.0, 40.0, 40.0);
  p->graphScene->setSceneRect(bounds.isValid() ? bounds : QRectF(0.0, 0.0, 400.0, 240.0));
}

void RenderGraphInspectorWidget::rebuildPasses() {
  p->updating = true;
  p->passes->clear();

  const RenderPlan plan = effectivePlan();
  for (const auto& pass : plan.passes()) {
    auto item = new QTreeWidgetItem(p->passes);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, pass.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(0, Qt::UserRole, qstr(pass.id));
    const auto order = plan.executionOrderNumber(pass.id);
    item->setText(1, order ? QString::number(*order) : QStringLiteral("-"));
    const auto stage = plan.executionStageNumber(pass.id);
    item->setText(2, stage ? QString::number(*stage) : QStringLiteral("-"));
    item->setText(3, p->displayName(pass));
    item->setToolTip(3, qstr(pass.id));
    item->setText(4, p->passTraceLine(pass));
    item->setText(5, p->displayText(pass.kind));
    item->setText(6, p->displayText(pass.executor));
    item->setText(7, p->sceneSelectorText(pass.sceneView.selector));
    item->setText(8, p->cameraText(pass.sceneView.camera));
    item->setText(9, p->shadingProfileText(pass.sceneView.shadingProfile));
    item->setText(10, p->resourceReads(plan, pass.reads));
    item->setText(11, p->resourceWrites(plan, pass.writes));
    item->setText(12, p->displayText(pass.disabledBehavior));
    if (pass.id == p->selectedPassId)
      item->setSelected(true);
  }

  p->passes->resizeColumnToContents(0);
  p->updating = false;
}

void RenderGraphInspectorWidget::rebuildGroups() {
  p->updating = true;
  p->groups->clear();

  const RenderPlan plan = effectivePlan();

  for (const auto kind : plan.passKinds()) {
    auto item = new QTreeWidgetItem(p->groups);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, p->overrides.disabledPassKinds.count(kind) == 0 ? Qt::Checked
                                                                           : Qt::Unchecked);
    item->setData(0, GroupScopeRole, QStringLiteral("kind"));
    item->setData(0, GroupValueRole, static_cast<int>(kind));
    item->setText(1, tr("Kind"));
    item->setText(2, p->displayText(kind));
  }

  for (const auto executor : plan.passExecutors()) {
    auto item = new QTreeWidgetItem(p->groups);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, p->overrides.disabledExecutors.count(executor) == 0 ? Qt::Checked
                                                                               : Qt::Unchecked);
    item->setData(0, GroupScopeRole, QStringLiteral("executor"));
    item->setData(0, GroupValueRole, static_cast<int>(executor));
    item->setText(1, tr("Executor"));
    item->setText(2, p->displayText(executor));
  }

  for (const auto& feature : plan.passFeatures()) {
    auto item = new QTreeWidgetItem(p->groups);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, p->overrides.disabledFeatures.count(feature) == 0 ? Qt::Checked
                                                                             : Qt::Unchecked);
    item->setData(0, GroupScopeRole, QStringLiteral("feature"));
    item->setData(0, GroupValueRole, qstr(feature));
    item->setText(1, tr("Feature"));
    item->setText(2, p->displayFeatureText(feature));
    item->setToolTip(2, qstr(feature));
  }

  p->groups->resizeColumnToContents(0);
  p->updating = false;
}

void RenderGraphInspectorWidget::rebuildResources() {
  p->updating = true;
  p->resources->clear();

  const RenderPlan plan = effectivePlan();
  for (const auto& resource : plan.resources()) {
    auto item = new QTreeWidgetItem(p->resources);
    item->setData(0, Qt::UserRole, qstr(resource.id));
    item->setText(0, p->displayName(resource));
    item->setToolTip(0, qstr(resource.id));
    item->setText(1, p->resourceProducer(plan, resource.id));
    item->setText(2, p->resourceConsumers(plan, resource.id));
    item->setText(3, p->displayText(resource.type));
    item->setText(4, p->resourceFeatures(resource));
    item->setText(5, p->displayText(resource.format));
    item->setText(6, p->displayText(resource.domain));
    item->setText(7, p->displayText(resource.lifetime));
    item->setText(8, p->sizeText(resource));
    if (resource.id == p->selectedResourceId)
      item->setSelected(true);
  }
  p->updating = false;
}

void RenderGraphInspectorWidget::updateValidationStatus() {
  if (!p->compileError.isEmpty()) {
    p->validationStatus->setText(tr("Graph compile failed: %1").arg(p->compileError));
    return;
  }

  const RenderPlan plan = effectivePlan();
  const auto validation = plan.validate();
  if (validation.valid()) {
    p->validationStatus->setText(tr("Valid plan: %1 pass(es), %2 resource(s)")
                                   .arg(static_cast<qulonglong>(plan.passes().size()))
                                   .arg(static_cast<qulonglong>(plan.resources().size())));
    return;
  }

  const auto& first = validation.errors().front();
  p->validationStatus->setText(tr("Invalid plan: %1").arg(QString::fromStdString(first.message)));
}
