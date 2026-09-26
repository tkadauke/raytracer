#pragma once

#include "core/Color.h"

#include <QColor>

inline Colord qColorToColord(const QColor& color) {
  return Colord::fromRGB(color.red(), color.green(), color.blue());
}

inline QColor colordToQColor(const Colord& color) {
  return QColor(color.rInt(), color.gInt(), color.bInt());
}
