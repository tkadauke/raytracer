#include "world/objects/StepVisibilityEvaluator.h"

#include "world/objects/Element.h"
#include "world/objects/Group.h"

#include <algorithm>

bool StepPlaybackStyle::enabled() const {
  return activeStep.has_value() || activeTime.has_value();
}

StepVisibilitySelection::StepVisibilitySelection(StepVisibilityMode mode,
                                                 std::optional<int> firstStep,
                                                 std::optional<int> lastStep,
                                                 std::optional<double> time)
    : m_mode(mode),
      m_firstStep(firstStep),
      m_lastStep(lastStep),
      m_time(time) {
}

StepVisibilitySelection StepVisibilitySelection::onlyStep(int step) {
  return StepVisibilitySelection(StepVisibilityMode::OnlyStep, step, step);
}

StepVisibilitySelection StepVisibilitySelection::cumulativeThrough(int step) {
  return StepVisibilitySelection(StepVisibilityMode::Cumulative, std::nullopt, step);
}

StepVisibilitySelection StepVisibilitySelection::atTime(double time) {
  return StepVisibilitySelection(StepVisibilityMode::OnlyStep, std::nullopt, std::nullopt, time);
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

std::optional<double> StepVisibilitySelection::time() const {
  return m_time;
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
  const auto layerIndex = group.layerIndex();
  if (!style.enabled() || (!stepIndex && !layerIndex && !group.startTime() && !group.endTime()))
    return stepMatches(group) ? StepVisualRole::Normal : StepVisualRole::Hidden;

  if (style.activeStep) {
    const auto activeIndex = *style.activeStep;
    const auto groupIndex = stepIndex ? stepIndex : layerIndex;
    if (groupIndex) {
      if (*groupIndex == activeIndex)
        return style.highlightActive ? StepVisualRole::Active : StepVisualRole::Normal;

      if (*groupIndex < activeIndex && style.ghostPrevious)
        return StepVisualRole::Previous;

      return StepVisualRole::Hidden;
    }

    if (timeRangeContains(group, activeIndex))
      return style.highlightActive ? StepVisualRole::Active : StepVisualRole::Normal;

    const auto endTime = group.endTime().value_or(group.startTime().value_or(activeIndex));
    if (endTime < activeIndex && style.ghostPrevious)
      return StepVisualRole::Previous;
  }

  if (style.activeTime) {
    if (timeRangeContains(group, *style.activeTime))
      return style.highlightActive ? StepVisualRole::Active : StepVisualRole::Normal;

    const auto endTime = group.endTime().value_or(group.startTime().value_or(*style.activeTime));
    if (endTime < *style.activeTime && style.ghostPrevious)
      return StepVisualRole::Previous;
  }

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
    return group.layerIndex() ? stepIndexMatches(*group.layerIndex()) : timeRangeMatches(group);

  return stepIndexMatches(*stepIndex);
}

bool StepVisibilityEvaluator::stepIndexMatches(int stepIndex) const {
  if (m_selection.time())
    return stepIndex == static_cast<int>(*m_selection.time());

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

bool StepVisibilityEvaluator::timeRangeMatches(const Group& group) const {
  if (!group.startTime() && !group.endTime())
    return true;

  if (m_selection.time())
    return timeRangeContains(group, *m_selection.time());

  if (m_selection.firstStep() && m_selection.lastStep()) {
    const double first = *m_selection.firstStep();
    const double last = *m_selection.lastStep();
    const double start = group.startTime().value_or(group.endTime().value_or(first));
    const double end = group.endTime().value_or(start);
    return end >= first && start <= last;
  }

  if (m_selection.lastStep()) {
    const double step = *m_selection.lastStep();
    if (m_selection.mode() == StepVisibilityMode::Cumulative)
      return group.startTime().value_or(group.endTime().value_or(step)) <= step;
    return timeRangeContains(group, step);
  }

  return m_selection.mode() == StepVisibilityMode::All;
}

bool StepVisibilityEvaluator::timeRangeContains(const Group& group, double time) const {
  const auto start = group.startTime();
  const auto end = group.endTime();
  if (!start && !end)
    return true;

  return (!start || *start <= time) && (!end || time <= *end);
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
