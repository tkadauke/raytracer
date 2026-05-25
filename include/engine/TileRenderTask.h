#pragma once

#include "core/math/Rect.h"
#include "render/TilePlan.h"

#include <QRunnable>
#include <QThreadPool>

#include <atomic>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>

namespace engine {
  class TileRenderTask : public QRunnable {
  public:
    TileRenderTask(const Recti& rect, std::function<void()> work);

    void run() override;

    std::atomic<bool> active;
    std::atomic<bool> completed;
    Recti rect;

  private:
    std::function<void()> m_work;
  };

  using TileWork = std::function<void(const Recti&, std::size_t)>;

  void dispatchTileTasks(const render::TilePlan& tilePlan, QThreadPool& threadPool,
                         std::list<std::shared_ptr<TileRenderTask>>& tasks, const TileWork& work);
}
