#include "engine/TileRenderTask.h"

#include "core/Exception.h"

#include <utility>

namespace engine {

  TileRenderTask::TileRenderTask(const Recti& rect, std::function<void()> work)
      : active(false),
        completed(false),
        rect(rect),
        m_work(std::move(work)) {
    setAutoDelete(false);
  }

  void TileRenderTask::run() {
    try {
      active.store(true, std::memory_order_release);
      m_work();
    } catch (Exception& e) {
      e.printBacktrace();
    }
    active.store(false, std::memory_order_release);
    completed.store(true, std::memory_order_release);
  }

  void dispatchTileTasks(const render::TilePlan& tilePlan, QThreadPool& threadPool,
                         std::list<std::shared_ptr<TileRenderTask>>& tasks, const TileWork& work) {
    tasks.clear();
    for (int row = 0; row != tilePlan.rows(); ++row) {
      for (int col = 0; col != tilePlan.cols(); ++col) {
        const Recti rect = tilePlan.rect(row, col);
        if (rect.width() <= 0 || rect.height() <= 0)
          continue;
        const std::size_t tileIndex = tilePlan.index(row, col);
        auto task =
          std::make_shared<TileRenderTask>(rect, [&, rect, tileIndex] { work(rect, tileIndex); });

        tasks.push_back(task);
        threadPool.start(task.get());
      }
    }

    threadPool.waitForDone();
  }
}
