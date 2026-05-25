#pragma once

#include "engine/graph/RenderPlan.h"

#include <QByteArray>
#include <QString>
#include <QWidget>

#include <memory>

class QTreeWidgetItem;

namespace engine::graph {
  class RenderGraphExecutionTrace;
}

/**
  * Compact Qt inspector for a compiled render graph plan.
  *
  * The widget is intentionally a view/editor over `RenderPlan` and
  * `RenderGraphOverrides`; it does not render pixels. Callers compile a plan,
  * pass it to `setPlan()`, then read back `overrides()` or `effectivePlan()`
  * after the user toggles pass checkboxes.
  */
class RenderGraphInspectorWidget : public QWidget {
  Q_OBJECT

public:
  explicit RenderGraphInspectorWidget(QWidget* parent = nullptr);
  ~RenderGraphInspectorWidget();

  QSize sizeHint() const override;

  /**
    * Replaces the compiled plan shown by the inspector.
    *
    * Existing per-pass overrides are preserved only for pass ids that still
    * exist in the new plan.
    */
  void setPlan(const engine::graph::RenderPlan& plan);

  /**
    * @returns the graph overrides represented by the current pass checkboxes.
    */
  engine::graph::RenderGraphOverrides overrides() const;

  /**
    * @returns the currently displayed plan with checkbox overrides applied.
    */
  engine::graph::RenderPlan effectivePlan() const;

  /**
    * @returns true when `effectivePlan().validate()` has no errors.
    */
  bool effectivePlanValid() const;

  /**
    * Notifies the inspector that a completed execution trace is available.
    */
  void setExecutionTrace(std::shared_ptr<const engine::graph::RenderGraphExecutionTrace> trace);

public slots:
  /**
    * Clears live execution styling from the graph view.
    */
  void clearExecutionState();

  /**
    * Marks @p passId as currently executing.
    */
  void passExecutionStarted(const QString& passId);

  /**
    * Marks @p passId as completed for the current render.
    */
  void passExecutionFinished(const QString& passId);

  /**
    * Marks @p passId as failed for the current render.
    */
  void passExecutionFailed(const QString& passId, const QString& message);

signals:
  /**
    * Emitted after a pass checkbox changes the stored overrides.
    */
  void overridesChanged();
  void passSelected(const QString& passId);
  void resourceSelected(const QString& resourceId);
  void selectedPassTraceChanged(const QString& passId);
  void selectedResourceTraceChanged(const QString& resourceId);
  void graphExportRequested(const QString& format, const QByteArray& data);

private slots:
  void passItemChanged(QTreeWidgetItem* item, int column);
  void passSelectionChanged();
  void resourceSelectionChanged();
  void promotePendingExecutionStates();
  void exportTextGraph();
  void exportDotGraph();
  void exportJsonGraph();

private:
  bool eventFilter(QObject* watched, QEvent* event) override;

  void rebuildAllViews();
  void rebuildGraph();
  void rebuildPasses();
  void rebuildResources();
  void updateValidationStatus();
  void selectPass(const engine::graph::RenderPassId& passId);
  void selectResource(const engine::graph::RenderResourceId& resourceId);
  void setPassEnabledOverride(const engine::graph::RenderPassId& passId, bool enabled);

  struct Private;
  std::unique_ptr<Private> p;
};
