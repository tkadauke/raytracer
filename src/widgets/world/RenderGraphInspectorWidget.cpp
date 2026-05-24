#include "widgets/world/RenderGraphInspectorWidget.h"

#include "core/Buffer.h"
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
#include <QImage>
#include <QLabel>
#include <QLayoutItem>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QScrollArea>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
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
  constexpr double PassWidth = 190.0;
  constexpr double PassHeight = 74.0;
  constexpr double ResourceWidth = 150.0;
  constexpr double ResourceHeight = 44.0;
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

  QImage colorPreviewImage(const Buffer<Colord>& buffer) {
    QImage image(buffer.width(), buffer.height(), QImage::Format_RGB32);
    for (int y = 0; y != buffer.height(); ++y) {
      for (int x = 0; x != buffer.width(); ++x) {
        image.setPixel(x, y, qRgb(buffer[y][x].rInt(), buffer[y][x].gInt(), buffer[y][x].bInt()));
      }
    }
    return image;
  }

  QString snapshotTitle(const RenderGraphResourceSnapshot& snapshot) {
    const auto& descriptor = snapshot.descriptor();
    return QStringLiteral("%1 (%2, %3x%4)")
      .arg(qstr(snapshot.resourceId()))
      .arg(toString(descriptor.type))
      .arg(descriptor.width)
      .arg(descriptor.height);
  }

  void clearLayout(QLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
      delete item->widget();
      delete item;
    }
  }

  QLabel* addText(QVBoxLayout& layout, const QString& text, bool bold = false) {
    auto* label = new QLabel(text);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (bold) {
      QFont font = label->font();
      font.setBold(true);
      label->setFont(font);
    }
    layout.addWidget(label);
    return label;
  }

  void addImage(QVBoxLayout& layout, const Buffer<Colord>& buffer) {
    auto* image = new QLabel();
    image->setObjectName("renderGraphTraceImage");
    image->setPixmap(QPixmap::fromImage(colorPreviewImage(buffer)));
    image->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout.addWidget(image);
  }

  void addSnapshot(QVBoxLayout& layout, const RenderGraphResourceSnapshot& snapshot) {
    addText(layout, snapshotTitle(snapshot), true);
    if (snapshot.hasColorPreview()) {
      addImage(layout, snapshot.colorPreview());
    } else {
      addText(layout, dashIfEmpty(qstr(snapshot.unavailableReason())));
    }
  }

  void addDiff(QVBoxLayout& layout, const RenderGraphResourceDiff& diff) {
    addText(layout,
            qstr(diff.inputResourceId()) + QStringLiteral(" -> ") + qstr(diff.outputResourceId()),
            true);
    if (!diff.hasPreview()) {
      addText(layout, dashIfEmpty(qstr(diff.unavailableReason())));
      return;
    }

    addText(layout, QStringLiteral("Boosted difference"));
    addImage(layout, diff.boostedPreview());
    addText(layout, QStringLiteral("Absolute difference"));
    addImage(layout, diff.absolutePreview());
  }

  void addMetadataRow(QTreeWidget& metadata, const QString& field, const QString& value) {
    auto* item = new QTreeWidgetItem(&metadata);
    item->setText(0, field);
    item->setText(1, dashIfEmpty(value));
  }
}

struct RenderGraphInspectorWidget::Private {
  RenderPlan plan;
  RenderGraphOverrides overrides;
  std::shared_ptr<const RenderGraphExecutionTrace> executionTrace;
  RenderPassId selectedPassId;
  RenderResourceId selectedResourceId;
  bool hasSelection{false};
  std::map<RenderPassId, std::chrono::steady_clock::time_point> pendingExecutionStarts;
  std::map<RenderPassId, PassExecutionState> executionStates;
  std::map<RenderPassId, QString> executionMessages;
  QTimer* liveExecutionTimer{nullptr};
  QGraphicsView* graph{nullptr};
  QGraphicsScene* graphScene{nullptr};
  QTreeWidget* passes{nullptr};
  QTreeWidget* resources{nullptr};
  QLabel* traceTitle{nullptr};
  QTabWidget* traceTabs{nullptr};
  QWidget* traceInputs{nullptr};
  QWidget* traceOutputs{nullptr};
  QWidget* traceDiffs{nullptr};
  QVBoxLayout* traceInputsLayout{nullptr};
  QVBoxLayout* traceOutputsLayout{nullptr};
  QVBoxLayout* traceDiffsLayout{nullptr};
  QTreeWidget* traceMetadata{nullptr};
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

  p->resources = new QTreeWidget(tabs);
  p->resources->setObjectName("renderGraphResources");
  p->resources->setRootIsDecorated(false);
  p->resources->setAlternatingRowColors(true);
  p->resources->setHeaderLabels({tr("Resource"), tr("Producer"), tr("Consumers"), tr("Type"),
                                 tr("Format"), tr("Domain"), tr("Lifetime"), tr("Size")});
  p->resources->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  p->resources->header()->setStretchLastSection(true);
  connect(p->resources, SIGNAL(itemSelectionChanged()), this, SLOT(resourceSelectionChanged()));

  auto trace = new QWidget(tabs);
  auto traceLayout = new QVBoxLayout(trace);
  p->traceTitle = new QLabel(trace);
  p->traceTitle->setObjectName("renderGraphTraceTitle");
  p->traceTitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
  p->traceTabs = new QTabWidget(trace);
  p->traceTabs->setObjectName("renderGraphTraceTabs");

  auto makeTraceScroll = [trace](const char* objectName, QVBoxLayout** contentLayout) {
    auto* area = new QScrollArea(trace);
    area->setWidgetResizable(true);
    auto* content = new QWidget(area);
    content->setObjectName(objectName);
    auto* itemLayout = new QVBoxLayout(content);
    itemLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    content->setLayout(itemLayout);
    area->setWidget(content);
    *contentLayout = itemLayout;
    return area;
  };

  QScrollArea* traceInputsArea = makeTraceScroll("renderGraphTraceInputs", &p->traceInputsLayout);
  QScrollArea* traceOutputsArea =
    makeTraceScroll("renderGraphTraceOutputs", &p->traceOutputsLayout);
  QScrollArea* traceDiffsArea =
    makeTraceScroll("renderGraphTraceDifferences", &p->traceDiffsLayout);
  p->traceInputs = traceInputsArea->widget();
  p->traceOutputs = traceOutputsArea->widget();
  p->traceDiffs = traceDiffsArea->widget();

  p->traceMetadata = new QTreeWidget(trace);
  p->traceMetadata->setObjectName("renderGraphTraceMetadata");
  p->traceMetadata->setRootIsDecorated(false);
  p->traceMetadata->setAlternatingRowColors(true);
  p->traceMetadata->setHeaderLabels({tr("Field"), tr("Value")});
  p->traceMetadata->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  p->traceMetadata->header()->setStretchLastSection(true);

  p->traceTabs->addTab(traceInputsArea, tr("Input"));
  p->traceTabs->addTab(traceOutputsArea, tr("Output"));
  p->traceTabs->addTab(traceDiffsArea, tr("Difference"));
  p->traceTabs->addTab(p->traceMetadata, tr("Metadata"));
  traceLayout->addWidget(p->traceTitle);
  traceLayout->addWidget(p->traceTabs, 1);
  trace->setLayout(traceLayout);

  tabs->addTab(p->graph, tr("Graph"));
  tabs->addTab(p->passes, tr("Passes"));
  tabs->addTab(p->resources, tr("Resources"));
  tabs->addTab(trace, tr("Trace"));

  layout->addWidget(p->validationStatus);
  layout->addWidget(tabs, 1);
  setLayout(layout);

  rebuildTrace();
  updateValidationStatus();
}

RenderGraphInspectorWidget::~RenderGraphInspectorWidget() = default;

QSize RenderGraphInspectorWidget::sizeHint() const {
  return QSize(720, 220);
}

void RenderGraphInspectorWidget::setPlan(const RenderPlan& plan) {
  p->plan = plan;
  p->executionTrace.reset();
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
  if (trace && !trace->matchesPlan(effectivePlan())) {
    trace.reset();
  }
  p->executionTrace = std::move(trace);
  if (p->executionTrace && p->selectedResourceId.empty() &&
      !p->executionTrace->findPass(p->selectedPassId)) {
    p->selectedPassId = p->executionTrace->passes().empty()
                          ? RenderPassId()
                          : p->executionTrace->passes().front().passId();
  }
  rebuildTrace();
  if (p->hasSelection && !p->selectedResourceId.empty()) {
    emit selectedResourceTraceChanged(qstr(p->selectedResourceId));
  } else if (p->hasSelection && !p->selectedPassId.empty()) {
    emit selectedPassTraceChanged(qstr(p->selectedPassId));
  }
}

void RenderGraphInspectorWidget::clearExecutionState() {
  if (p->pendingExecutionStarts.empty() && p->executionStates.empty() &&
      p->executionMessages.empty() && !p->executionTrace)
    return;

  p->executionTrace.reset();
  p->liveExecutionTimer->stop();
  p->pendingExecutionStarts.clear();
  p->executionStates.clear();
  p->executionMessages.clear();
  rebuildGraph();
  rebuildTrace();
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
  rebuildResources();
  rebuildTrace();
  updateValidationStatus();
}

void RenderGraphInspectorWidget::selectPass(const RenderPassId& passId) {
  p->hasSelection = true;
  p->selectedResourceId.clear();
  if (p->selectedPassId == passId) {
    rebuildGraph();
    rebuildTrace();
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
  rebuildTrace();
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
  rebuildTrace();
  emit resourceSelected(qstr(resourceId));
}

void RenderGraphInspectorWidget::setPassEnabledOverride(const RenderPassId& passId, bool enabled) {
  p->executionTrace.reset();
  p->liveExecutionTimer->stop();
  p->pendingExecutionStarts.clear();
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

    QPen resourcePen(QColor(80, 95, 110));
    resourcePen.setWidthF(resource.id == p->selectedResourceId ? 2.5 : 1.2);
    addNode(
      *p->graphScene, QRectF(location->second, QSizeF(ResourceWidth, ResourceHeight)),
      QStringLiteral("resource"), qstr(resource.id),
      {qstr(resource.id), qstr(toString(resource.type))}, resourcePen,
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

    QGraphicsRectItem* item = addNode(
      *p->graphScene, QRectF(location->second, QSizeF(PassWidth, PassHeight)),
      QStringLiteral("pass"), qstr(pass.id),
      {qstr(pass.id), qstr(toString(pass.kind)) + QStringLiteral("/") + toString(pass.executor),
       pass.enabled ? tr("enabled") : tr("disabled")},
      pen, brush);
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

void RenderGraphInspectorWidget::rebuildTrace() {
  clearLayout(p->traceInputsLayout);
  clearLayout(p->traceOutputsLayout);
  clearLayout(p->traceDiffsLayout);
  p->traceMetadata->clear();

  if (p->selectedPassId.empty()) {
    p->traceTitle->setText(tr("No pass selected"));
    addText(*p->traceInputsLayout, tr("No pass selected"));
    addText(*p->traceOutputsLayout, tr("No pass selected"));
    addText(*p->traceDiffsLayout, tr("No pass selected"));
    addMetadataRow(*p->traceMetadata, tr("Pass"), tr("No pass selected"));
    return;
  }

  p->traceTitle->setText(tr("Selected pass: %1").arg(qstr(p->selectedPassId)));
  if (!p->executionTrace) {
    addText(*p->traceInputsLayout, tr("No execution trace for this pass"));
    addText(*p->traceOutputsLayout, tr("No execution trace for this pass"));
    addText(*p->traceDiffsLayout, tr("No execution trace for this pass"));
    addMetadataRow(*p->traceMetadata, tr("Pass"), qstr(p->selectedPassId));
    addMetadataRow(*p->traceMetadata, tr("Trace"), tr("not available"));
    return;
  }

  const RenderPassTrace* trace = p->executionTrace->findPass(p->selectedPassId);
  if (!trace) {
    addText(*p->traceInputsLayout, tr("No execution trace for this pass"));
    addText(*p->traceOutputsLayout, tr("No execution trace for this pass"));
    addText(*p->traceDiffsLayout, tr("No execution trace for this pass"));
    addMetadataRow(*p->traceMetadata, tr("Pass"), qstr(p->selectedPassId));
    addMetadataRow(*p->traceMetadata, tr("Trace"), tr("not available"));
    return;
  }

  p->traceTitle->setText(
    tr("Selected pass: %1 (%2)").arg(qstr(trace->passId())).arg(toString(trace->status())));

  if (trace->inputs().empty()) {
    addText(*p->traceInputsLayout, tr("No input resources"));
  } else {
    for (const auto& input : trace->inputs())
      addSnapshot(*p->traceInputsLayout, input);
  }

  if (trace->outputs().empty()) {
    addText(*p->traceOutputsLayout, tr("No output resources"));
  } else {
    for (const auto& output : trace->outputs())
      addSnapshot(*p->traceOutputsLayout, output);
  }

  if (trace->diffs().empty()) {
    addText(*p->traceDiffsLayout, tr("No color difference for this pass"));
  } else {
    for (const auto& diff : trace->diffs())
      addDiff(*p->traceDiffsLayout, diff);
  }

  p->traceInputsLayout->addStretch(1);
  p->traceOutputsLayout->addStretch(1);
  p->traceDiffsLayout->addStretch(1);

  addMetadataRow(*p->traceMetadata, tr("Pass"), qstr(trace->passId()));
  addMetadataRow(*p->traceMetadata, tr("Status"), toString(trace->status()));
  addMetadataRow(*p->traceMetadata, tr("Elapsed"),
                 tr("%1 ms").arg(trace->elapsed().count() / 1000000.0, 0, 'f', 3));
  addMetadataRow(*p->traceMetadata, tr("Message"), qstr(trace->message()));
  for (const auto& input : trace->inputs()) {
    addMetadataRow(*p->traceMetadata, tr("Input"),
                   input.hasColorPreview() ? qstr(input.resourceId())
                                           : qstr(input.resourceId()) + QStringLiteral(": ") +
                                               qstr(input.unavailableReason()));
  }
  for (const auto& output : trace->outputs()) {
    addMetadataRow(*p->traceMetadata, tr("Output"),
                   output.hasColorPreview() ? qstr(output.resourceId())
                                            : qstr(output.resourceId()) + QStringLiteral(": ") +
                                                qstr(output.unavailableReason()));
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
