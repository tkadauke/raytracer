#include "widgets/world/RenderGraphInspectorWidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QStringList>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <string>

using namespace engine::graph;

namespace {
  QString qstr(const std::string& value) {
    return QString::fromStdString(value);
  }

  QString dashIfEmpty(const QString& value) {
    return value.isEmpty() ? QStringLiteral("-") : value;
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
}

struct RenderGraphInspectorWidget::Private {
  RenderPlan plan;
  RenderGraphOverrides overrides;
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

  p->passes = new QTreeWidget(tabs);
  p->passes->setObjectName("renderGraphPasses");
  p->passes->setRootIsDecorated(false);
  p->passes->setAlternatingRowColors(true);
  p->passes->setHeaderLabels({tr("Enabled"), tr("Pass"), tr("Kind"), tr("Executor"), tr("Reads"),
                              tr("Writes"), tr("Disabled behavior")});
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
  p->resources->setHeaderLabels(
    {tr("Resource"), tr("Type"), tr("Format"), tr("Domain"), tr("Lifetime"), tr("Size")});
  p->resources->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  p->resources->header()->setStretchLastSection(true);

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

  const auto ids = passIds(p->plan);
  for (auto it = p->overrides.disabledPasses.begin(); it != p->overrides.disabledPasses.end();) {
    if (ids.find(*it) == ids.end()) {
      it = p->overrides.disabledPasses.erase(it);
    } else {
      ++it;
    }
  }

  rebuildPasses();
  rebuildDependencies();
  rebuildResources();
  updateValidationStatus();
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

void RenderGraphInspectorWidget::passItemChanged(QTreeWidgetItem* item, int column) {
  if (p->updating || column != 0 || !item)
    return;

  const auto passId = item->data(0, Qt::UserRole).toString().toStdString();
  if (item->checkState(0) == Qt::Checked) {
    p->overrides.disabledPasses.erase(passId);
  } else {
    p->overrides.disabledPasses.insert(passId);
  }

  updateValidationStatus();
  emit overridesChanged();
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
    item->setText(1, qstr(pass.id));
    item->setText(2, toString(pass.kind));
    item->setText(3, toString(pass.executor));
    item->setText(4, resourceReads(pass.reads));
    item->setText(5, resourceWrites(pass.writes));
    item->setText(6, toString(pass.disabledBehavior));
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

  for (const auto& resource : p->plan.resources()) {
    auto item = new QTreeWidgetItem(p->resources);
    item->setText(0, qstr(resource.id));
    item->setText(1, toString(resource.type));
    item->setText(2, toString(resource.format));
    item->setText(3, toString(resource.domain));
    item->setText(4, toString(resource.lifetime));
    item->setText(5, sizeText(resource));
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
