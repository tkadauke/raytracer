#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <QWidget>

#include <memory>

namespace engine::graph {
  class RenderGraphExecutionTrace;
}

/**
  * Large preview surface for inspecting render graph trace images.
  *
  * The Render Graph dock owns graph selection. This widget owns the bigger
  * image preview for the selected pass or resource.
  */
class RenderGraphTracePreviewWidget : public QWidget {
public:
  explicit RenderGraphTracePreviewWidget(QWidget* parent = nullptr);
  ~RenderGraphTracePreviewWidget();

  void showPassTrace(std::shared_ptr<const engine::graph::RenderGraphExecutionTrace> trace,
                     const engine::graph::RenderPassId& passId);
  void showResourceTrace(std::shared_ptr<const engine::graph::RenderGraphExecutionTrace> trace,
                         const engine::graph::RenderResourceId& resourceId);
  void clear();

private:
  void clearContent();

  struct Private;
  std::unique_ptr<Private> p;
};
