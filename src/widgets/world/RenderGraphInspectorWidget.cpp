#include "widgets/world/RenderGraphInspectorWidget.h"

#include <QBrush>
#include <QEvent>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
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
  constexpr double PassWidth = 190.0;
  constexpr double PassHeight = 74.0;
  constexpr double ResourceWidth = 150.0;
  constexpr double ResourceHeight = 44.0;
  constexpr double ColumnGap = PassWidth + ResourceWidth + 120.0;
  constexpr double RowGap = 120.0;
  constexpr double OriginX = 40.0;
  constexpr double OriginY = 44.0;

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
  std::map<RenderPassId, PassExecutionState> executionStates;
  std::map<RenderPassId, QString> executionMessages;
  QGraphicsView* graph{nullptr};
  QGraphicsScene* graphScene{nullptr};
  QTreeWidget* passes{nullptr};
  QTreeWidget* dependencies{nullptr};
  QTreeWidget* resources{nullptr};
  QLabel* validationStatus{nullptr};
  bool updating{false};
};

RenderGraphInspectorWidget::RenderGraphInspectorWidget(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  auto layout = new QVBoxLayout(this);

  p->validationStatus = new QLabel(this);
  p->validationStatus->setObjectName("renderGraphValidationStatus");

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

  p->dependencies = new QTreeWidget(tabs);
  p->dependencies->setObjectName("renderGraphDependencies");
  p->dependencies->setRootIsDecorated(false);
  p->dependencies->setAlternatingRowColors(true);
  p->dependencies->setHeaderLabels({tr("Producer"), tr("Resource"), tr("Consumer")});
  p->dependencies->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  p->dependencies->header()->setStretchLastSection(true);

  p->resources = new QTreeWidget(tabs);
  p->resources->setObjectName("renderGraphResources");
  p->resources->setRootIsDecorated(false);
  p->resources->setAlternatingRowColors(true);
  p->resources->setHeaderLabels({tr("Resource"), tr("Producer"), tr("Consumers"), tr("Type"),
                                 tr("Format"), tr("Domain"), tr("Lifetime"), tr("Size")});
  p->resources->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  p->resources->header()->setStretchLastSection(true);

  tabs->addTab(p->graph, tr("Graph"));
  tabs->addTab(p->passes, tr("Passes"));
  tabs->addTab(p->dependencies, tr("Dependencies"));
  tabs->addTab(p->resources, tr("Resources"));

  layout->addWidget(p->validationStatus);
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
  p->executionStates.clear();
  p->executionMessages.clear();

  const auto ids = passIds(p->plan);
  for (auto it = p->overrides.disabledPasses.begin(); it != p->overrides.disabledPasses.end();) {
    if (ids.find(*it) == ids.end()) {
      it = p->overrides.disabledPasses.erase(it);
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

void RenderGraphInspectorWidget::clearExecutionState() {
  if (p->executionStates.empty() && p->executionMessages.empty())
    return;

  p->executionStates.clear();
  p->executionMessages.clear();
  rebuildGraph();
}

void RenderGraphInspectorWidget::passExecutionStarted(const QString& passId) {
  p->executionStates[passId.toStdString()] = PassExecutionState::Running;
  p->executionMessages.erase(passId.toStdString());
  rebuildGraph();
}

void RenderGraphInspectorWidget::passExecutionFinished(const QString& passId) {
  p->executionStates[passId.toStdString()] = PassExecutionState::Completed;
  p->executionMessages.erase(passId.toStdString());
  rebuildGraph();
}

void RenderGraphInspectorWidget::passExecutionFailed(const QString& passId,
                                                     const QString& message) {
  p->executionStates[passId.toStdString()] = PassExecutionState::Failed;
  p->executionMessages[passId.toStdString()] = message;
  rebuildGraph();
}

void RenderGraphInspectorWidget::passItemChanged(QTreeWidgetItem* item, int column) {
  if (p->updating || column != 0 || !item)
    return;

  const auto passId = item->data(0, Qt::UserRole).toString().toStdString();
  setPassEnabledOverride(passId, item->checkState(0) == Qt::Checked);
}

bool RenderGraphInspectorWidget::eventFilter(QObject* watched, QEvent* event) {
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
  rebuildDependencies();
  rebuildResources();
  updateValidationStatus();
}

void RenderGraphInspectorWidget::setPassEnabledOverride(const RenderPassId& passId, bool enabled) {
  p->executionStates.clear();
  p->executionMessages.clear();
  if (enabled) {
    p->overrides.disabledPasses.erase(passId);
  } else {
    p->overrides.disabledPasses.insert(passId);
  }

  rebuildAllViews();
  emit overridesChanged();
}

void RenderGraphInspectorWidget::rebuildGraph() {
  p->graphScene->clear();

  const RenderPlan plan = effectivePlan();
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

    addNode(*p->graphScene, QRectF(location->second, QSizeF(ResourceWidth, ResourceHeight)),
            QStringLiteral("resource"), qstr(resource.id),
            {qstr(resource.id), qstr(toString(resource.type))}, QPen(QColor(80, 95, 110)),
            QBrush(QColor(235, 241, 246)));
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
    pen.setWidthF(1.5);
    if (!pass.enabled)
      pen.setStyle(Qt::DashLine);

    QGraphicsRectItem* item = addNode(
      *p->graphScene, QRectF(location->second, QSizeF(PassWidth, PassHeight)),
      QStringLiteral("pass"), qstr(pass.id),
      {qstr(pass.id), qstr(toString(pass.kind)) + QStringLiteral("/") + toString(pass.executor),
       pass.enabled ? tr("enabled") : tr("disabled")},
      pen, brush);
    item->setData(GraphItemExecutionStateRole, executionStateName(executionState));
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
  }

  p->passes->resizeColumnToContents(0);
  p->updating = false;
}

void RenderGraphInspectorWidget::rebuildDependencies() {
  p->dependencies->clear();

  const RenderPlan plan = effectivePlan();
  for (const auto& dependency : plan.dependencies()) {
    auto item = new QTreeWidgetItem(p->dependencies);
    item->setText(0, qstr(dependency.producer->id));
    item->setText(1, qstr(dependency.resource));
    item->setText(2, qstr(dependency.consumer->id));
  }
}

void RenderGraphInspectorWidget::rebuildResources() {
  p->resources->clear();

  const RenderPlan plan = effectivePlan();
  for (const auto& resource : plan.resources()) {
    auto item = new QTreeWidgetItem(p->resources);
    item->setText(0, qstr(resource.id));
    item->setText(1, resourceProducer(plan, resource.id));
    item->setText(2, resourceConsumers(plan, resource.id));
    item->setText(3, toString(resource.type));
    item->setText(4, toString(resource.format));
    item->setText(5, toString(resource.domain));
    item->setText(6, toString(resource.lifetime));
    item->setText(7, sizeText(resource));
  }
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
