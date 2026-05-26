#pragma once

#include <functional>
#include <optional>
#include <vector>

class Element;
class Group;

/**
  * Step-selection modes for importer-authored Group metadata.
  */
enum class StepVisibilityMode {
  OnlyStep,
  Cumulative,
  All,
  Range,
};

/**
  * Immutable step-selection request used by StepVisibilityEvaluator.
  *
  * Groups with no valid stepIndex metadata are treated as static hierarchy and
  * remain eligible in step-filtered modes. A group's explicit visible flag is
  * still composed with the evaluated step visibility.
  */
class StepVisibilitySelection {
public:
  static StepVisibilitySelection onlyStep(int step);
  static StepVisibilitySelection cumulativeThrough(int step);
  static StepVisibilitySelection all();
  static StepVisibilitySelection range(int firstStep, int lastStep);

  StepVisibilityMode mode() const;
  std::optional<int> firstStep() const;
  std::optional<int> lastStep() const;

private:
  StepVisibilitySelection(StepVisibilityMode mode,
                          std::optional<int> firstStep,
                          std::optional<int> lastStep);

  StepVisibilityMode m_mode;
  std::optional<int> m_firstStep;
  std::optional<int> m_lastStep;
};

/**
  * Evaluates which world Groups are visible for a requested step selection.
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
  void forEachGroup(const Element& root,
                    bool ancestorsVisible,
                    const GroupCallback& callback) const;

  StepVisibilitySelection m_selection;
};
