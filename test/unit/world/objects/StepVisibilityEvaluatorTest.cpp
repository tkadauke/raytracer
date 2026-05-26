#include <gtest/gtest.h>

#include "world/objects/Group.h"
#include "world/objects/Scene.h"
#include "world/objects/StepVisibilityEvaluator.h"

#include <QJsonObject>
#include <vector>

namespace StepVisibilityEvaluatorTest {
  namespace {
    std::vector<int> effectivelyVisibleSteps(const Scene& scene,
                                             const StepVisibilityEvaluator& evaluator) {
      std::vector<int> steps;
      evaluator.forEachGroup(scene, [&](const Group& group, bool, bool effectivelyVisible) {
        if (effectivelyVisible && group.stepIndex())
          steps.push_back(*group.stepIndex());
      });
      return steps;
    }
  }

  TEST(StepVisibilityEvaluator, ShouldShowOnlyRequestedSparseStep) {
    Scene scene;

    auto* stepTwo = new Group;
    stepTwo->setStepIndex(2);
    scene.addChild(stepTwo);

    auto* stepFive = new Group;
    stepFive->setStepIndex(5);
    scene.addChild(stepFive);

    auto* stepNine = new Group;
    stepNine->setStepIndex(9);
    scene.addChild(stepNine);

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::onlyStep(5));

    EXPECT_FALSE(evaluator.visible(*stepTwo));
    EXPECT_TRUE(evaluator.visible(*stepFive));
    EXPECT_FALSE(evaluator.visible(*stepNine));
    EXPECT_EQ(std::vector<int>{5}, effectivelyVisibleSteps(scene, evaluator));
  }

  TEST(StepVisibilityEvaluator, ShouldShowCumulativeStepsThroughRequestedStep) {
    Scene scene;

    auto* stepTwo = new Group;
    stepTwo->setStepIndex(2);
    scene.addChild(stepTwo);

    auto* stepFive = new Group;
    stepFive->setStepIndex(5);
    scene.addChild(stepFive);

    auto* stepNine = new Group;
    stepNine->setStepIndex(9);
    scene.addChild(stepNine);

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::cumulativeThrough(6));

    EXPECT_TRUE(evaluator.visible(*stepTwo));
    EXPECT_TRUE(evaluator.visible(*stepFive));
    EXPECT_FALSE(evaluator.visible(*stepNine));
    EXPECT_EQ((std::vector<int>{2, 5}), effectivelyVisibleSteps(scene, evaluator));
  }

  TEST(StepVisibilityEvaluator, ShouldShowAllStepGroupsInAllMode) {
    Scene scene;

    auto* explicitHidden = new Group;
    explicitHidden->setStepIndex(2);
    explicitHidden->hide();
    scene.addChild(explicitHidden);

    auto* visible = new Group;
    visible->setStepIndex(9);
    scene.addChild(visible);

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::all());

    EXPECT_FALSE(evaluator.visible(*explicitHidden));
    EXPECT_TRUE(evaluator.visible(*visible));
    EXPECT_EQ(std::vector<int>{9}, effectivelyVisibleSteps(scene, evaluator));
  }

  TEST(StepVisibilityEvaluator, ShouldShowStepsInsideInclusiveRange) {
    Scene scene;

    auto* before = new Group;
    before->setStepIndex(1);
    scene.addChild(before);

    auto* first = new Group;
    first->setStepIndex(3);
    scene.addChild(first);

    auto* last = new Group;
    last->setStepIndex(8);
    scene.addChild(last);

    auto* after = new Group;
    after->setStepIndex(13);
    scene.addChild(after);

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::range(8, 3));

    EXPECT_FALSE(evaluator.visible(*before));
    EXPECT_TRUE(evaluator.visible(*first));
    EXPECT_TRUE(evaluator.visible(*last));
    EXPECT_FALSE(evaluator.visible(*after));
    EXPECT_EQ((std::vector<int>{3, 8}), effectivelyVisibleSteps(scene, evaluator));
  }

  TEST(StepVisibilityEvaluator, ShouldTreatMissingAndMalformedStepMetadataAsStatic) {
    Scene scene;

    auto* staticGroup = new Group;
    scene.addChild(staticGroup);

    auto* malformedStep = new Group;
    malformedStep->setMetadata(QJsonObject{{GroupMetadata::stepIndexKey(), 1.5}});
    scene.addChild(malformedStep);

    auto* hiddenStaticGroup = new Group;
    hiddenStaticGroup->hide();
    scene.addChild(hiddenStaticGroup);

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::onlyStep(12));

    EXPECT_TRUE(evaluator.visible(*staticGroup));
    EXPECT_TRUE(evaluator.visible(*malformedStep));
    EXPECT_FALSE(evaluator.visible(*hiddenStaticGroup));

    const auto visibleGroups = evaluator.visibleGroups(scene);
    EXPECT_EQ(2u, visibleGroups.size());
    EXPECT_EQ(staticGroup, visibleGroups[0]);
    EXPECT_EQ(malformedStep, visibleGroups[1]);
  }

  TEST(StepVisibilityEvaluator, ShouldUseLayerIndexWhenStepIndexIsMissing) {
    Scene scene;

    auto* layerTwo = new Group;
    layerTwo->setLayerIndex(2);
    scene.addChild(layerTwo);

    auto* layerFive = new Group;
    layerFive->setLayerIndex(5);
    scene.addChild(layerFive);

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::onlyStep(5));

    EXPECT_FALSE(evaluator.visible(*layerTwo));
    EXPECT_TRUE(evaluator.visible(*layerFive));
    EXPECT_EQ(std::vector<int>{}, effectivelyVisibleSteps(scene, evaluator));
  }

  TEST(StepVisibilityEvaluator, ShouldUseTimeIntervalsWhenIndexMetadataIsMissing) {
    Scene scene;

    auto* early = new Group;
    early->setTimeRange(1.0, 3.0);
    scene.addChild(early);

    auto* active = new Group;
    active->setTimeRange(4.0, 6.0);
    scene.addChild(active);

    auto* openEnded = new Group;
    openEnded->setTimeRange(7.0, std::nullopt);
    scene.addChild(openEnded);

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::onlyStep(5));

    EXPECT_FALSE(evaluator.visible(*early));
    EXPECT_TRUE(evaluator.visible(*active));
    EXPECT_FALSE(evaluator.visible(*openEnded));
  }

  TEST(StepVisibilityEvaluator, ShouldMatchOpenEndedTimeIntervalsAtTime) {
    Group startsAtFive;
    startsAtFive.setTimeRange(5.0, std::nullopt);
    Group endsAtFive;
    endsAtFive.setTimeRange(std::nullopt, 5.0);

    const StepVisibilityEvaluator atFour(StepVisibilitySelection::atTime(4.0));
    const StepVisibilityEvaluator atSix(StepVisibilitySelection::atTime(6.0));

    EXPECT_FALSE(atFour.visible(startsAtFive));
    EXPECT_TRUE(atSix.visible(startsAtFive));
    EXPECT_TRUE(atFour.visible(endsAtFive));
    EXPECT_FALSE(atSix.visible(endsAtFive));
  }

  TEST(StepVisibilityEvaluator, ShouldComposeNestedGroupVisibility) {
    Scene scene;

    auto* staticParent = new Group;
    scene.addChild(staticParent);

    auto* matchingChild = new Group;
    matchingChild->setStepIndex(5);
    staticParent->addChild(matchingChild);

    auto* matchingGrandchild = new Group;
    matchingGrandchild->setStepIndex(5);
    matchingChild->addChild(matchingGrandchild);

    auto* hiddenAncestor = new Group;
    hiddenAncestor->hide();
    scene.addChild(hiddenAncestor);

    auto* childUnderHiddenAncestor = new Group;
    childUnderHiddenAncestor->setStepIndex(5);
    hiddenAncestor->addChild(childUnderHiddenAncestor);

    auto* filteredAncestor = new Group;
    filteredAncestor->setStepIndex(2);
    scene.addChild(filteredAncestor);

    auto* childUnderFilteredAncestor = new Group;
    childUnderFilteredAncestor->setStepIndex(5);
    filteredAncestor->addChild(childUnderFilteredAncestor);

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::onlyStep(5));

    EXPECT_TRUE(evaluator.effectivelyVisible(*staticParent));
    EXPECT_TRUE(evaluator.effectivelyVisible(*matchingChild));
    EXPECT_TRUE(evaluator.effectivelyVisible(*matchingGrandchild));
    EXPECT_FALSE(evaluator.effectivelyVisible(*hiddenAncestor));
    EXPECT_FALSE(evaluator.effectivelyVisible(*childUnderHiddenAncestor));
    EXPECT_FALSE(evaluator.effectivelyVisible(*filteredAncestor));
    EXPECT_FALSE(evaluator.effectivelyVisible(*childUnderFilteredAncestor));
    EXPECT_TRUE(evaluator.visible(*childUnderFilteredAncestor));

    const auto visibleGroups = evaluator.visibleGroups(scene);
    ASSERT_EQ(3u, visibleGroups.size());
    EXPECT_EQ(staticParent, visibleGroups[0]);
    EXPECT_EQ(matchingChild, visibleGroups[1]);
    EXPECT_EQ(matchingGrandchild, visibleGroups[2]);
  }

  TEST(StepVisibilityEvaluator, ShouldClassifyActiveAndPreviousPlaybackRoles) {
    Group previous;
    previous.setStepIndex(2);
    Group active;
    active.setStepIndex(5);
    Group future;
    future.setStepIndex(8);
    Group staticGroup;

    StepPlaybackStyle style;
    style.activeStep = 5;
    style.highlightActive = true;
    style.ghostPrevious = true;

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::cumulativeThrough(5));

    EXPECT_EQ(StepVisualRole::Previous, evaluator.visualRole(previous, style));
    EXPECT_EQ(StepVisualRole::Active, evaluator.visualRole(active, style));
    EXPECT_EQ(StepVisualRole::Hidden, evaluator.visualRole(future, style));
    EXPECT_EQ(StepVisualRole::Normal, evaluator.visualRole(staticGroup, style));
  }

  TEST(StepVisibilityEvaluator, ShouldClassifyActiveAndPreviousTimePlaybackRoles) {
    Group previous;
    previous.setTimeRange(1.0, 2.0);
    Group active;
    active.setTimeRange(4.0, 6.0);
    Group future;
    future.setTimeRange(8.0, 9.0);

    StepPlaybackStyle style;
    style.activeTime = 5.0;
    style.highlightActive = true;
    style.ghostPrevious = true;

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::atTime(5.0));

    EXPECT_EQ(StepVisualRole::Previous, evaluator.visualRole(previous, style));
    EXPECT_EQ(StepVisualRole::Active, evaluator.visualRole(active, style));
    EXPECT_EQ(StepVisualRole::Hidden, evaluator.visualRole(future, style));
  }

  TEST(StepVisibilityEvaluator, ShouldLeaveRolesNormalWhenPlaybackStyleIsDisabled) {
    Group group;
    group.setStepIndex(5);

    StepPlaybackStyle style;
    style.highlightActive = true;
    style.ghostPrevious = true;

    const StepVisibilityEvaluator evaluator(StepVisibilitySelection::all());

    EXPECT_EQ(StepVisualRole::Normal, evaluator.visualRole(group, style));
  }
}
