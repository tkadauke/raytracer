#pragma once

#include "core/Exception.h"
#include "core/math/Rect.h"
#include "render/TilePlan.h"

#include <QRunnable>
#include <QThreadPool>

#include <atomic>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <utility>

namespace engine {
  class TileRenderTask : public QRunnable {
  public:
    TileRenderTask(const Recti& rect, std::function<void()> work)
        : active(false),
          rect(rect),
          m_work(std::move(work)) {
      setAutoDelete(false);
    }

    void run() override {
      try {
        active = true;
        m_work();
      } catch (Exception& e) {
        e.printBacktrace();
      }
      active = false;
    }

    std::atomic<bool> active;
    Recti rect;

  private:
    std::function<void()> m_work;
  };

  template<class WorkFn>
  void dispatchTileTasks(const render::TilePlan& tilePlan, QThreadPool& threadPool,
                         std::list<std::shared_ptr<TileRenderTask>>& tasks, WorkFn&& work) {
    tasks.clear();
    for (int row = 0; row != tilePlan.rows(); ++row) {
      for (int col = 0; col != tilePlan.cols(); ++col) {
        const Recti rect = tilePlan.rect(row, col);
        if (rect.width() <= 0 || rect.height() <= 0)
          continue;
        const std::size_t tileIndex = tilePlan.index(row, col);
        auto task = std::make_shared<TileRenderTask>(rect, [&, rect, tileIndex] {
          work(rect, tileIndex);
        });

        tasks.push_back(task);
        threadPool.start(task.get());
      }
    }

    threadPool.waitForDone();
  }
}
