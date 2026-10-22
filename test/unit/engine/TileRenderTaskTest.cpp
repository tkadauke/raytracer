#include <gtest/gtest.h>

#include "engine/TileRenderTask.h"

#include <list>
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

  TEST(TileRenderTaskTest, AppendTileHelpersFilterByStateAndPreserveOrder) {
    std::list<std::shared_ptr<TileRenderTask>> tasks;
    for (int i = 0; i < 3; ++i)
      tasks.push_back(std::make_shared<TileRenderTask>(Recti(i, 0, 1, 1), [] {}));
    auto it = tasks.begin();
    (*it)->active = true;
    ++it;
    (*it)->completed = true;
    ++it;
    (*it)->active = true;
    (*it)->completed = true;

    std::list<Recti> active{Recti(9, 9, 1, 1)};
    appendActiveTiles(tasks, active);
    ASSERT_EQ(3u, active.size());
    EXPECT_EQ(9, active.front().left());
    EXPECT_EQ(2, active.back().left());

    std::list<Recti> completed;
    appendCompletedTiles(tasks, completed);
    ASSERT_EQ(2u, completed.size());
    EXPECT_EQ(1, completed.front().left());
    EXPECT_EQ(2, completed.back().left());
  }
}
