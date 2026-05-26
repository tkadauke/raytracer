#include "world/objects/StepVisibilityEvaluator.h"

#include "world/objects/Element.h"
#include "world/objects/Group.h"

#include <algorithm>

bool StepPlaybackStyle::enabled() const {
  return activeStep.has_value();
}

StepVisibilitySelection::StepVisibilitySelection(StepVisibilityMode mode,
                                                 std::optional<int> firstStep,
                                                 std::optional<int> lastStep)
    : m_mode(mode),
      m_firstStep(firstStep),
      m_lastStep(lastStep) {
}

StepVisibilitySelection StepVisibilitySelection::onlyStep(int step) {
  return StepVisibilitySelection(StepVisibilityMode::OnlyStep, step, step);
}

StepVisibilitySelection StepVisibilitySelection::cumulativeThrough(int step) {
  return StepVisibilitySelection(StepVisibilityMode::Cumulative, std::nullopt, step);
}

StepVisibilitySelection StepVisibilitySelection::all() {
  return StepVisibilitySelection(StepVisibilityMode::All, std::nullopt, std::nullopt);
}

StepVisibilitySelection StepVisibilitySelection::range(int firstStep, int lastStep) {
  if (lastStep < firstStep)
    std::swap(firstStep, lastStep);

  return StepVisibilitySelection(StepVisibilityMode::Range, firstStep, lastStep);
}

StepVisibilityMode StepVisibilitySelection::mode() const {
  return m_mode;
}

std::optional<int> StepVisibilitySelection::firstStep() const {
  return m_firstStep;
}

std::optional<int> StepVisibilitySelection::lastStep() const {
  return m_lastStep;
}

StepVisibilityEvaluator::StepVisibilityEvaluator(StepVisibilitySelection selection)
    : m_selection(selection) {
}

bool StepVisibilityEvaluator::visible(const Group& group) const {
  return group.visible() && stepMatches(group);
}

StepVisualRole StepVisibilityEvaluator::visualRole(const Group& group,
                                                   const StepPlaybackStyle& style) const {
  if (!group.visible())
    return StepVisualRole::Hidden;

  const auto stepIndex = group.stepIndex();
  if (!style.enabled() || !stepIndex)
    return stepMatches(group) ? StepVisualRole::Normal : StepVisualRole::Hidden;

  if (*stepIndex == *style.activeStep)
    return style.highlightActive ? StepVisualRole::Active : StepVisualRole::Normal;

  if (*stepIndex < *style.activeStep && style.ghostPrevious)
    return StepVisualRole::Previous;

  return StepVisualRole::Hidden;
}

bool StepVisibilityEvaluator::effectivelyVisible(const Group& group) const {
  if (!visible(group))
    return false;

  for (Element* ancestor = group.parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
    if (auto ancestorGroup = dynamic_cast<Group*>(ancestor)) {
      if (!visible(*ancestorGroup))
        return false;
    }
  }

  return true;
}

void StepVisibilityEvaluator::forEachGroup(const Element& root,
                                           const GroupCallback& callback) const {
  forEachGroup(root, true, callback);
}

std::vector<const Group*> StepVisibilityEvaluator::visibleGroups(const Element& root) const {
  std::vector<const Group*> result;
  forEachGroup(root, [&](const Group& group, bool, bool effectivelyVisible) {
    if (effectivelyVisible)
      result.push_back(&group);
  });
  return result;
}

bool StepVisibilityEvaluator::stepMatches(const Group& group) const {
  const auto stepIndex = group.stepIndex();
  if (!stepIndex)
    return true;

  return stepIndexMatches(*stepIndex);
}

bool StepVisibilityEvaluator::stepIndexMatches(int stepIndex) const {
  switch (m_selection.mode()) {
  case StepVisibilityMode::OnlyStep:
    return m_selection.firstStep() && stepIndex == *m_selection.firstStep();
  case StepVisibilityMode::Cumulative:
    return m_selection.lastStep() && stepIndex <= *m_selection.lastStep();
  case StepVisibilityMode::All:
    return true;
  case StepVisibilityMode::Range:
    return m_selection.firstStep() && m_selection.lastStep() &&
           stepIndex >= *m_selection.firstStep() &&
           stepIndex <= *m_selection.lastStep();
  }

  return false;
}

void StepVisibilityEvaluator::forEachGroup(const Element& root,
                                           bool ancestorsVisible,
                                           const GroupCallback& callback) const {
  bool childrenAncestorsVisible = ancestorsVisible;

  if (auto group = dynamic_cast<const Group*>(&root)) {
    const bool directlyVisible = visible(*group);
    const bool effectivelyVisible = ancestorsVisible && directlyVisible;
    callback(*group, directlyVisible, effectivelyVisible);
    childrenAncestorsVisible = effectivelyVisible;
  }

  for (const auto& child : root.childElements()) {
    forEachGroup(*child, childrenAncestorsVisible, callback);
  }
}
