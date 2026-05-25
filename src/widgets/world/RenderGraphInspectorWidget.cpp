#include "widgets/world/RenderGraphInspectorWidget.h"

#include "engine/graph/RenderGraphExecutionTrace.h"

#include <QBrush>
#include <QEvent>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
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
#include <set>
#include <string>
#include <vector>

using namespace engine::graph;

namespace {
  constexpr int GraphItemKindRole = 0;
  constexpr int GraphItemIdRole = 1;
  constexpr int GraphItemExecutionStateRole = 2;
  constexpr int GroupScopeRole = Qt::UserRole;
  constexpr int GroupValueRole = Qt::UserRole + 1;
  constexpr double PassWidth = 190.0;
  constexpr double PassHeight = 88.0;
  constexpr double ResourceWidth = 150.0;
  constexpr double ResourceHeight = 58.0;
  constexpr double ColumnGap = PassWidth + ResourceWidth + 120.0;
  constexpr double RowGap = 120.0;
  constexpr double OriginX = 40.0;
  constexpr double OriginY = 44.0;
  constexpr auto LiveExecutionDelay = std::chrono::milliseconds(500);

  QString qstr(const std::string& value) {
    return QString::fromStdString(value);
  }

  QString dashIfEmpty(const QString& value) {
    return value.isEmpty() ? QStringLiteral("-") : value;
  }

  enum class PassExecutionState { Idle, Running, Completed, Failed };

  QString executionStateName(PassExecutionState state) {
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

  QString passTraceLine(const RenderGraphExecutionTrace* trace, const RenderPassNode& pass) {
    if (!trace)
      return QString();

    const auto* passTrace = trace->findPass(pass.id);
    if (!passTrace)
      return QString();

    return QStringLiteral("%1, %2 ms")
      .arg(toString(passTrace->status()))
      .arg(passTrace->elapsed().count() / 1000000.0, 0, 'f', 2);
  }

  const RenderGraphResourceSnapshot*
  firstSnapshotForResource(const RenderGraphExecutionTrace* trace,
                           const RenderResourceId& resourceId) {
    if (!trace)
      return nullptr;

    const auto outputs = trace->outputSnapshotsForResource(resourceId);
    if (!outputs.empty())
      return outputs.front();

    const auto inputs = trace->inputSnapshotsForResource(resourceId);
    return inputs.empty() ? nullptr : inputs.front();
  }

  QString resourceTraceLine(const RenderGraphExecutionTrace* trace,
                            const RenderResourceDescriptor& resource) {
    const auto* snapshot = firstSnapshotForResource(trace, resource.id);
    if (!snapshot)
      return QString();

    if (snapshot->cacheMetadata().status() != RenderGraphCacheStatus::NotCacheable) {
      return QStringLiteral("cache: %1").arg(toString(snapshot->cacheMetadata().status()));
    }
    if (snapshot->hasColorPreview())
      return QStringLiteral("trace: color");
    if (snapshot->hasDepthPreview())
      return QStringLiteral("trace: depth");
    return QStringLiteral("trace: metadata");
  }

  QString resourceReads(const std::vector<ResourceRead>& reads) {
    QStringList values;
    for (const auto& read : reads)
      values << qstr(read.resource);
    return dashIfEmpty(values.join(", "));
  }

  QString resourceWrites(const std::vector<ResourceWrite>& writes) {
    QStringList values;
    for (const auto& write : writes)
      values << qstr(write.resource);
    return dashIfEmpty(values.join(", "));
  }

  QString resourceProducer(const RenderPlan& plan, const RenderResourceId& resource) {
    const RenderPassNode* producer = plan.producerOf(resource);
    return producer ? qstr(producer->id) : QStringLiteral("-");
  }

  QString resourceConsumers(const RenderPlan& plan, const RenderResourceId& resource) {
    QStringList values;
    for (const RenderPassNode* consumer : plan.consumersOf(resource))
      values << qstr(consumer->id);
    return dashIfEmpty(values.join(", "));
  }

  QString sizeText(const RenderResourceDescriptor& resource) {
    return QStringLiteral("%1x%2, %3 sample(s)")
      .arg(resource.width)
      .arg(resource.height)
      .arg(resource.sampleCount);
  }

  std::set<RenderPassId> passIds(const RenderPlan& plan) {
    std::set<RenderPassId> ids;
    for (const auto& pass : plan.passes())
      ids.insert(pass.id);
    return ids;
  }

  std::set<RenderPassKind> passKinds(const RenderPlan& plan) {
    std::set<RenderPassKind> values;
    for (const auto& pass : plan.passes())
      values.insert(pass.kind);
    return values;
  }

  std::set<RenderExecutorKind> passExecutors(const RenderPlan& plan) {
    std::set<RenderExecutorKind> values;
    for (const auto& pass : plan.passes())
      values.insert(pass.executor);
    return values;
  }

  std::set<RenderFeatureKind> passFeatures(const RenderPlan& plan) {
    std::set<RenderFeatureKind> values;
    for (const auto& pass : plan.passes()) {
      for (const auto& feature : pass.features)
        values.insert(feature);
    }
    return values;
  }

  std::map<RenderPassId, int> executionOrderByPassId(const RenderPlan& plan) {
    std::map<RenderPassId, int> result;
    int order = 1;
    for (const RenderPassNode* pass : plan.executionOrder()) {
      result.emplace(pass->id, order++);
    }
    return result;
  }

  std::map<RenderPassId, int> dependencyRanks(const RenderPlan& plan) {
    std::map<RenderPassId, int> result;
    for (const RenderPassNode* pass : plan.executionOrder()) {
      int rank = 0;
      for (const auto& dependency : plan.dependencies()) {
        if (dependency.consumer->id != pass->id)
          continue;

        const auto producerRank = result.find(dependency.producer->id);
        if (producerRank != result.end())
          rank = std::max(rank, producerRank->second + 1);
      }
      result[pass->id] = rank;
    }
    return result;
  }

  std::map<RenderPassId, QPointF> passPositions(const RenderPlan& plan) {
    const auto ranks = dependencyRanks(plan);
    std::map<int, int> rowsByRank;
    std::map<RenderPassId, QPointF> result;

    for (const RenderPassNode* pass : plan.executionOrder()) {
      const int rank = ranks.at(pass->id);
      const int row = rowsByRank[rank]++;
      result[pass->id] = QPointF(OriginX + rank * ColumnGap, OriginY + row * RowGap);
    }

    return result;
  }

  QPointF passCenter(const QPointF& topLeft) {
    return topLeft + QPointF(PassWidth / 2.0, PassHeight / 2.0);
  }

  QPointF resourcePosition(const RenderPlan& plan, const RenderResourceDescriptor& resource,
                           const std::map<RenderPassId, QPointF>& passes, int fallbackRow) {
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

  QGraphicsRectItem* addNode(QGraphicsScene& scene, const QRectF& rect, const QString& kind,
                             const QString& id, const QStringList& lines, const QPen& pen,
                             const QBrush& brush) {
    auto* item = scene.addRect(rect, pen, brush);
    item->setData(GraphItemKindRole, kind);
    item->setData(GraphItemIdRole, id);
    item->setFlag(QGraphicsItem::ItemIsSelectable);

    double y = rect.top() + 9.0;
    for (int i = 0; i != lines.size(); ++i) {
      auto* text = scene.addSimpleText(lines[i]);
      text->setParentItem(item);
      text->setData(GraphItemKindRole, kind);
      text->setData(GraphItemIdRole, id);
      QFont font = text->font();
      if (i == 0)
        font.setBold(true);
      font.setPointSize(i == 0 ? 9 : 8);
      text->setFont(font);
      text->setPos(rect.left() + 10.0, y);
      y += text->boundingRect().height() + 2.0;
    }

    return item;
  }

  void addEdge(QGraphicsScene& scene, const QPointF& from, const QPointF& to) {
    QPen pen(QColor(90, 100, 110));
    pen.setWidthF(1.4);
    auto* line = scene.addLine(from.x(), from.y(), to.x(), to.y(), pen);
    line->setZValue(-10.0);
  }

  QGraphicsItem* graphNodeItem(QGraphicsItem* item) {
    while (item && item->data(GraphItemKindRole).toString().isEmpty())
      item = item->parentItem();
    return item;
  }

}

struct RenderGraphInspectorWidget::Private {
  RenderPlan plan;
  RenderGraphOverrides overrides;
  std::shared_ptr<const RenderGraphExecutionTrace> trace;
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
};

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
  p->passes->setHeaderLabels({tr("Enabled"), tr("Order"), tr("Pass"), tr("Kind"), tr("Executor"),
                              tr("Reads"), tr("Writes"), tr("Disabled behavior")});
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
                                 tr("Format"), tr("Domain"), tr("Lifetime"), tr("Size")});
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

RenderGraphInspectorWidget::~RenderGraphInspectorWidget() = default;

QSize RenderGraphInspectorWidget::sizeHint() const {
  return QSize(720, 220);
}

void RenderGraphInspectorWidget::setPlan(const RenderPlan& plan) {
  p->plan = plan;
  if (p->trace && !p->trace->matchesPlan(effectivePlan()))
    p->trace.reset();
  p->liveExecutionTimer->stop();
  p->pendingExecutionStarts.clear();
  p->executionStates.clear();
  p->executionMessages.clear();

  const auto ids = passIds(p->plan);
  if (ids.find(p->selectedPassId) == ids.end()) {
    if (p->hasSelection && p->selectedResourceId.empty())
      p->hasSelection = false;
    p->selectedPassId = p->plan.passes().empty() ? RenderPassId() : p->plan.passes().front().id;
  }
  if (p->plan.findResource(p->selectedResourceId) == nullptr) {
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
  const auto kinds = passKinds(p->plan);
  for (auto it = p->overrides.disabledPassKinds.begin();
       it != p->overrides.disabledPassKinds.end();) {
    if (kinds.find(*it) == kinds.end()) {
      it = p->overrides.disabledPassKinds.erase(it);
    } else {
      ++it;
    }
  }
  const auto executors = passExecutors(p->plan);
  for (auto it = p->overrides.disabledExecutors.begin();
       it != p->overrides.disabledExecutors.end();) {
    if (executors.find(*it) == executors.end()) {
      it = p->overrides.disabledExecutors.erase(it);
    } else {
      ++it;
    }
  }
  const auto features = passFeatures(p->plan);
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

RenderGraphOverrides RenderGraphInspectorWidget::overrides() const {
  return p->overrides;
}

RenderPlan RenderGraphInspectorWidget::effectivePlan() const {
  return p->plan.withOverrides(p->overrides);
}

bool RenderGraphInspectorWidget::effectivePlanValid() const {
  return effectivePlan().validate().valid();
}

void RenderGraphInspectorWidget::setExecutionTrace(
  std::shared_ptr<const RenderGraphExecutionTrace> trace) {
  p->trace = trace && trace->matchesPlan(effectivePlan()) ? std::move(trace) : nullptr;
  rebuildGraph();

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

void RenderGraphInspectorWidget::passItemChanged(QTreeWidgetItem* item, int column) {
  if (p->updating || column != 0 || !item)
    return;

  const auto passId = item->data(0, Qt::UserRole).toString().toStdString();
  setPassEnabledOverride(passId, item->checkState(0) == Qt::Checked);
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
  rebuildAllViews();
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
      graphNodeItem(p->graphScene->itemAt(mouseEvent->scenePos(), QTransform()));
    if (item && item->data(GraphItemKindRole).toString() == QStringLiteral("pass")) {
      selectPass(item->data(GraphItemIdRole).toString().toStdString());
    } else if (item && item->data(GraphItemKindRole).toString() == QStringLiteral("resource")) {
      selectResource(item->data(GraphItemIdRole).toString().toStdString());
    }
  }

  if (watched == p->graphScene && event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
    auto* mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
    QGraphicsItem* item =
      graphNodeItem(p->graphScene->itemAt(mouseEvent->scenePos(), QTransform()));
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

void RenderGraphInspectorWidget::setPassEnabledOverride(const RenderPassId& passId, bool enabled) {
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

  rebuildAllViews();
  emit overridesChanged();
}

void RenderGraphInspectorWidget::rebuildGraph() {
  p->graphScene->clear();

  const RenderPlan plan = effectivePlan();
  const RenderGraphExecutionTrace* trace =
    p->trace && p->trace->matchesPlan(plan) ? p->trace.get() : nullptr;
  const auto passLocations = passPositions(plan);
  std::map<RenderResourceId, QPointF> resourceLocations;

  for (std::size_t index = 0; index != plan.resources().size(); ++index) {
    const auto& resource = plan.resources()[index];
    resourceLocations.emplace(
      resource.id, resourcePosition(plan, resource, passLocations, static_cast<int>(index)));
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
        addEdge(*p->graphScene, producerIt->second + QPointF(PassWidth, PassHeight / 2.0),
                resourceLeft);
      }
    }

    for (const RenderPassNode* consumer : plan.consumersOf(resource.id)) {
      const auto consumerIt = passLocations.find(consumer->id);
      if (consumerIt != passLocations.end()) {
        addEdge(*p->graphScene, resourceRight, consumerIt->second + QPointF(0.0, PassHeight / 2.0));
      }
    }
  }

  for (const auto& resource : plan.resources()) {
    const auto location = resourceLocations.find(resource.id);
    if (location == resourceLocations.end())
      continue;

    QPen resourcePen(QColor(80, 95, 110));
    resourcePen.setWidthF(resource.id == p->selectedResourceId ? 2.5 : 1.2);
    QStringList lines{qstr(resource.id), qstr(toString(resource.type))};
    const QString traceLine = resourceTraceLine(trace, resource);
    if (!traceLine.isEmpty())
      lines << traceLine;
    addNode(
      *p->graphScene, QRectF(location->second, QSizeF(ResourceWidth, ResourceHeight)),
      QStringLiteral("resource"), qstr(resource.id), lines, resourcePen,
      QBrush(resource.id == p->selectedResourceId ? QColor(226, 237, 247) : QColor(235, 241, 246)));
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

    QStringList lines{qstr(pass.id),
                      qstr(toString(pass.kind)) + QStringLiteral("/") + toString(pass.executor),
                      pass.enabled ? tr("enabled") : tr("disabled")};
    const QString traceLine = passTraceLine(trace, pass);
    if (!traceLine.isEmpty())
      lines << traceLine;
    QGraphicsRectItem* item =
      addNode(*p->graphScene, QRectF(location->second, QSizeF(PassWidth, PassHeight)),
              QStringLiteral("pass"), qstr(pass.id), lines, pen, brush);
    item->setData(GraphItemExecutionStateRole, executionStateName(executionState));
    if (pass.id == p->selectedPassId)
      item->setSelected(true);
    const auto messageIt = p->executionMessages.find(pass.id);
    item->setToolTip(messageIt == p->executionMessages.end()
                       ? tr("Double-click to enable or disable this pass")
                       : messageIt->second);
  }

  const QRectF bounds = p->graphScene->itemsBoundingRect().adjusted(-40.0, -40.0, 40.0, 40.0);
  p->graphScene->setSceneRect(bounds.isValid() ? bounds : QRectF(0.0, 0.0, 400.0, 240.0));
}

void RenderGraphInspectorWidget::rebuildPasses() {
  p->updating = true;
  p->passes->clear();

  const RenderPlan plan = effectivePlan();
  const auto executionOrder = executionOrderByPassId(plan);
  for (const auto& pass : plan.passes()) {
    auto item = new QTreeWidgetItem(p->passes);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, pass.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(0, Qt::UserRole, qstr(pass.id));
    const auto orderIt = executionOrder.find(pass.id);
    item->setText(1, orderIt == executionOrder.end() ? QStringLiteral("-")
                                                     : QString::number(orderIt->second));
    item->setText(2, qstr(pass.id));
    item->setText(3, toString(pass.kind));
    item->setText(4, toString(pass.executor));
    item->setText(5, resourceReads(pass.reads));
    item->setText(6, resourceWrites(pass.writes));
    item->setText(7, toString(pass.disabledBehavior));
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

  for (const auto kind : passKinds(plan)) {
    auto item = new QTreeWidgetItem(p->groups);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, p->overrides.disabledPassKinds.count(kind) == 0 ? Qt::Checked
                                                                           : Qt::Unchecked);
    item->setData(0, GroupScopeRole, QStringLiteral("kind"));
    item->setData(0, GroupValueRole, static_cast<int>(kind));
    item->setText(1, tr("Kind"));
    item->setText(2, toString(kind));
  }

  for (const auto executor : passExecutors(plan)) {
    auto item = new QTreeWidgetItem(p->groups);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, p->overrides.disabledExecutors.count(executor) == 0 ? Qt::Checked
                                                                               : Qt::Unchecked);
    item->setData(0, GroupScopeRole, QStringLiteral("executor"));
    item->setData(0, GroupValueRole, static_cast<int>(executor));
    item->setText(1, tr("Executor"));
    item->setText(2, toString(executor));
  }

  for (const auto& feature : passFeatures(plan)) {
    auto item = new QTreeWidgetItem(p->groups);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, p->overrides.disabledFeatures.count(feature) == 0 ? Qt::Checked
                                                                             : Qt::Unchecked);
    item->setData(0, GroupScopeRole, QStringLiteral("feature"));
    item->setData(0, GroupValueRole, qstr(feature));
    item->setText(1, tr("Feature"));
    item->setText(2, qstr(feature));
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
    item->setText(0, qstr(resource.id));
    item->setText(1, resourceProducer(plan, resource.id));
    item->setText(2, resourceConsumers(plan, resource.id));
    item->setText(3, toString(resource.type));
    item->setText(4, toString(resource.format));
    item->setText(5, toString(resource.domain));
    item->setText(6, toString(resource.lifetime));
    item->setText(7, sizeText(resource));
    if (resource.id == p->selectedResourceId)
      item->setSelected(true);
  }
  p->updating = false;
}

void RenderGraphInspectorWidget::updateValidationStatus() {
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
