#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "core/Color.h"

class Element;
class Group;

/**
  * Step-selection modes for importer-authored Group metadata.
  *
  * The evaluator reads generic metadata from Group, not importer-specific
  * fields. `stepIndex` has highest precedence, `layerIndex` is used when no
  * step index exists, and `startTime` / `endTime` are used when neither index
  * exists. Groups with none of those fields are static context.
  */
enum class StepVisibilityMode {
  /**
    * Show only groups whose `stepIndex` or `layerIndex` equals the requested
    * step. Time ranges must contain the requested value.
    */
  OnlyStep,
  /**
    * Show indexed groups whose index is less than or equal to the requested
    * step. Time ranges match when they have started by the requested value.
    */
  Cumulative,
  /**
    * Show all step, layer, time-range, and static groups, subject to explicit
    * group visibility and ancestor visibility.
    */
  All,
  /**
    * Show indexed groups and time ranges overlapping the inclusive step range.
    */
  Range,
};

/**
  * Visual role for a group under step playback.
  */
enum class StepVisualRole {
  Hidden,
  Normal,
  Active,
  Previous,
};

/**
  * Optional style controls for domain-neutral step playback renders.
  *
  * Styling changes material choice during runtime scene conversion only. It
  * does not mutate the editable scene or saved group visibility. `activeStep`
  * is the rendercli `--step` value; `highlightActive` maps to
  * `--step_highlight`, and `ghostPrevious` maps to `--step_ghost_previous`.
  */
struct StepPlaybackStyle {
  std::optional<int> activeStep;
  std::optional<double> activeTime;
  bool highlightActive{false};
  bool ghostPrevious{false};
  Colord activeColor{1.0, 0.86, 0.08};
  Colord ghostColor{0.42, 0.46, 0.52};

  bool enabled() const;
};

/**
  * Immutable step-selection request used by StepVisibilityEvaluator.
  *
  * Groups with no valid step, layer, or time metadata are treated as static
  * hierarchy and remain eligible in step-filtered modes. A group's explicit
  * visible flag is still composed with the evaluated step visibility, and
  * effective visibility additionally requires every ancestor group to be
  * visible under the same selection.
  */
class StepVisibilitySelection {
public:
  static StepVisibilitySelection onlyStep(int step);
  static StepVisibilitySelection cumulativeThrough(int step);
  static StepVisibilitySelection atTime(double time);
  static StepVisibilitySelection all();
  static StepVisibilitySelection range(int firstStep, int lastStep);

  StepVisibilityMode mode() const;
  std::optional<int> firstStep() const;
  std::optional<int> lastStep() const;
  std::optional<double> time() const;

private:
  StepVisibilitySelection(StepVisibilityMode mode,
                          std::optional<int> firstStep,
                          std::optional<int> lastStep,
                          std::optional<double> time = std::nullopt);

  StepVisibilityMode m_mode;
  std::optional<int> m_firstStep;
  std::optional<int> m_lastStep;
  std::optional<double> m_time;
};

/**
  * Evaluates which world Groups are visible for a requested step selection.
  *
  * This class is intentionally domain-neutral: assembly steps, print layers,
  * simulation frames, and time-sliced imports all use the same Group metadata
  * helpers and the same visibility rules.
  */
class StepVisibilityEvaluator {
public:
  using GroupCallback = std::function<void(const Group& group,
                                           bool directlyVisible,
                                           bool effectivelyVisible)>;

  explicit StepVisibilityEvaluator(StepVisibilitySelection selection);

  /**
    * @returns true when @p group is explicitly visible and matches the step
    * selection. Ancestor groups are not considered.
    */
  bool visible(const Group& group) const;

  /**
    * @returns the direct visual role for @p group under @p style. Ancestor
    * groups are not considered.
    */
  StepVisualRole visualRole(const Group& group, const StepPlaybackStyle& style) const;

  /**
    * @returns true when @p group and every ancestor Group are visible under
    * the step selection.
    */
  bool effectivelyVisible(const Group& group) const;

  /**
    * Traverses the subtree rooted at @p root and calls @p callback for each
    * Group with both direct and ancestor-composed visibility.
    */
  void forEachGroup(const Element& root, const GroupCallback& callback) const;

  /**
    * @returns all Groups under @p root whose effective visibility is true.
    */
  std::vector<const Group*> visibleGroups(const Element& root) const;

private:
  bool stepMatches(const Group& group) const;
  bool stepIndexMatches(int stepIndex) const;
  bool timeRangeMatches(const Group& group) const;
  bool timeRangeContains(const Group& group, double time) const;
  void forEachGroup(const Element& root,
                    bool ancestorsVisible,
                    const GroupCallback& callback) const;

  StepVisibilitySelection m_selection;
};
