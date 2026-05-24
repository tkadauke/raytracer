#pragma once

#include "engine/graph/RenderPlan.h"

#include <QWidget>

#include <memory>

class QTreeWidgetItem;

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

signals:
  /**
    * Emitted after a pass checkbox changes the stored overrides.
    */
  void overridesChanged();

private slots:
  void passItemChanged(QTreeWidgetItem* item, int column);

private:
  bool eventFilter(QObject* watched, QEvent* event) override;

  void rebuildAllViews();
  void rebuildGraph();
  void rebuildPasses();
  void rebuildDependencies();
  void rebuildResources();
  void updateValidationStatus();
  void setPassEnabledOverride(const engine::graph::RenderPassId& passId, bool enabled);

  struct Private;
  std::unique_ptr<Private> p;
};
