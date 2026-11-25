#pragma once

#include "widgets/world/RenderGraphInspectorWidget.h"

#include <QString>

namespace test::helpers {

  /** Returns the value associated with a named render-graph inspector detail row. */
  inline QString rowValue(const RenderGraphInspectorWidget::DetailRows& rows, const QString& name) {
    for (const auto& row : rows) {
      if (row.first == name)
        return row.second;
    }
    return QString();
  }

}
