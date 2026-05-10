#include <gtest/gtest.h>

#include "src/engine/TileRenderTask.h"

#include <memory>

namespace TileRenderTaskTest {
  using namespace engine;

  TEST(TileRenderTaskTest, StartsInactiveAndIncomplete) {
    TileRenderTask task(Recti(1, 2, 3, 4), [] {});

    EXPECT_FALSE(task.active.load());
    EXPECT_FALSE(task.completed.load());
    EXPECT_EQ(1, task.rect.left());
    EXPECT_EQ(2, task.rect.top());
    EXPECT_EQ(3, task.rect.width());
    EXPECT_EQ(4, task.rect.height());
  }

  TEST(TileRenderTaskTest, MarksActiveDuringWorkAndCompletedAfterRun) {
    bool workCalled = false;
    bool activeDuringWork = false;
    std::unique_ptr<TileRenderTask> task;
    task = std::make_unique<TileRenderTask>(Recti(0, 0, 1, 1), [&] {
      workCalled = true;
      activeDuringWork = task->active.load();
      EXPECT_FALSE(task->completed.load());
    });

    task->run();

    EXPECT_TRUE(workCalled);
    EXPECT_TRUE(activeDuringWork);
    EXPECT_FALSE(task->active.load());
    EXPECT_TRUE(task->completed.load());
  }
}
