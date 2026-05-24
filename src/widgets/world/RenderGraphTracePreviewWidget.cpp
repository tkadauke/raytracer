#include "widgets/world/RenderGraphTracePreviewWidget.h"

#include "core/Buffer.h"
#include "engine/graph/RenderGraphExecutionTrace.h"

#include <QFont>
#include <QImage>
#include <QLabel>
#include <QLayoutItem>
#include <QPixmap>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace engine::graph;

namespace {
  constexpr int MinimumPreviewWidth = 640;

  QString qstr(const std::string& value) {
    return QString::fromStdString(value);
  }

  QString dashIfEmpty(const QString& value) {
    return value.isEmpty() ? QStringLiteral("-") : value;
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

  QPixmap scaledPreviewPixmap(const Buffer<Colord>& buffer) {
    QPixmap pixmap = QPixmap::fromImage(colorPreviewImage(buffer));
    if (pixmap.width() >= MinimumPreviewWidth)
      return pixmap;

    return pixmap.scaledToWidth(MinimumPreviewWidth, Qt::FastTransformation);
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
    image->setObjectName("renderGraphTracePreviewImage");
    image->setPixmap(scaledPreviewPixmap(buffer));
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

  QWidget* makeTraceScroll(QWidget* parent, const char* objectName, QVBoxLayout** contentLayout) {
    auto* area = new QScrollArea(parent);
    area->setWidgetResizable(true);
    auto* content = new QWidget(area);
    content->setObjectName(objectName);
    auto* itemLayout = new QVBoxLayout(content);
    itemLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    content->setLayout(itemLayout);
    area->setWidget(content);
    *contentLayout = itemLayout;
    return area;
  }
}

struct RenderGraphTracePreviewWidget::Private {
  QLabel* title{nullptr};
  QTabWidget* tabs{nullptr};
  QVBoxLayout* inputsLayout{nullptr};
  QVBoxLayout* outputsLayout{nullptr};
  QVBoxLayout* diffsLayout{nullptr};
};

RenderGraphTracePreviewWidget::RenderGraphTracePreviewWidget(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  setObjectName("renderGraphTracePreview");

  auto* layout = new QVBoxLayout(this);
  p->title = new QLabel(this);
  p->title->setObjectName("renderGraphTracePreviewTitle");
  p->title->setTextInteractionFlags(Qt::TextSelectableByMouse);
  QFont font = p->title->font();
  font.setBold(true);
  p->title->setFont(font);

  p->tabs = new QTabWidget(this);
  p->tabs->setObjectName("renderGraphTracePreviewTabs");
  p->tabs->addTab(makeTraceScroll(this, "renderGraphTracePreviewInputs", &p->inputsLayout),
                  tr("Input"));
  p->tabs->addTab(makeTraceScroll(this, "renderGraphTracePreviewOutputs", &p->outputsLayout),
                  tr("Output"));
  p->tabs->addTab(makeTraceScroll(this, "renderGraphTracePreviewDifferences", &p->diffsLayout),
                  tr("Difference"));

  layout->addWidget(p->title);
  layout->addWidget(p->tabs, 1);
  setLayout(layout);
  clear();
}

RenderGraphTracePreviewWidget::~RenderGraphTracePreviewWidget() = default;

void RenderGraphTracePreviewWidget::showPassTrace(
  std::shared_ptr<const RenderGraphExecutionTrace> trace, const RenderPassId& passId) {
  clearContent();
  p->title->setText(tr("Pass: %1").arg(qstr(passId)));

  if (!trace) {
    addText(*p->inputsLayout, tr("No execution trace for this pass"));
    addText(*p->outputsLayout, tr("No execution trace for this pass"));
    addText(*p->diffsLayout, tr("No execution trace for this pass"));
    return;
  }

  const RenderPassTrace* passTrace = trace->findPass(passId);
  if (!passTrace) {
    addText(*p->inputsLayout, tr("No execution trace for this pass"));
    addText(*p->outputsLayout, tr("No execution trace for this pass"));
    addText(*p->diffsLayout, tr("No execution trace for this pass"));
    return;
  }

  p->title->setText(
    tr("Pass: %1 (%2)").arg(qstr(passTrace->passId())).arg(toString(passTrace->status())));
  if (passTrace->inputs().empty()) {
    addText(*p->inputsLayout, tr("No input resources"));
  } else {
    for (const auto& input : passTrace->inputs())
      addSnapshot(*p->inputsLayout, input);
  }

  if (passTrace->outputs().empty()) {
    addText(*p->outputsLayout, tr("No output resources"));
  } else {
    for (const auto& output : passTrace->outputs())
      addSnapshot(*p->outputsLayout, output);
  }

  if (passTrace->diffs().empty()) {
    addText(*p->diffsLayout, tr("No color difference for this pass"));
  } else {
    for (const auto& diff : passTrace->diffs())
      addDiff(*p->diffsLayout, diff);
  }

  p->inputsLayout->addStretch(1);
  p->outputsLayout->addStretch(1);
  p->diffsLayout->addStretch(1);
}

void RenderGraphTracePreviewWidget::showResourceTrace(
  std::shared_ptr<const RenderGraphExecutionTrace> trace, const RenderResourceId& resourceId) {
  clearContent();
  p->title->setText(tr("Resource: %1").arg(qstr(resourceId)));

  if (!trace) {
    addText(*p->inputsLayout, tr("No execution trace for this resource"));
    addText(*p->outputsLayout, tr("No execution trace for this resource"));
    addText(*p->diffsLayout, tr("No execution trace for this resource"));
    return;
  }

  int inputCount = 0;
  int outputCount = 0;
  int diffCount = 0;
  for (const auto& passTrace : trace->passes()) {
    for (const auto& input : passTrace.inputs()) {
      if (input.resourceId() == resourceId) {
        addSnapshot(*p->inputsLayout, input);
        ++inputCount;
      }
    }
    for (const auto& output : passTrace.outputs()) {
      if (output.resourceId() == resourceId) {
        addSnapshot(*p->outputsLayout, output);
        ++outputCount;
      }
    }
    for (const auto& diff : passTrace.diffs()) {
      if (diff.inputResourceId() == resourceId || diff.outputResourceId() == resourceId) {
        addDiff(*p->diffsLayout, diff);
        ++diffCount;
      }
    }
  }

  if (inputCount == 0)
    addText(*p->inputsLayout, tr("No input snapshots for this resource"));
  if (outputCount == 0)
    addText(*p->outputsLayout, tr("No output snapshots for this resource"));
  if (diffCount == 0)
    addText(*p->diffsLayout, tr("No difference preview for this resource"));

  p->inputsLayout->addStretch(1);
  p->outputsLayout->addStretch(1);
  p->diffsLayout->addStretch(1);
}

void RenderGraphTracePreviewWidget::clear() {
  clearContent();
  p->title->setText(tr("Render graph trace"));
  addText(*p->inputsLayout, tr("Select a render graph pass or resource"));
  addText(*p->outputsLayout, tr("Select a render graph pass or resource"));
  addText(*p->diffsLayout, tr("Select a render graph pass or resource"));
}

void RenderGraphTracePreviewWidget::clearContent() {
  clearLayout(p->inputsLayout);
  clearLayout(p->outputsLayout);
  clearLayout(p->diffsLayout);
}
