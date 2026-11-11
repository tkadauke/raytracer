#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

#include <ostream>
#include <string_view>

namespace core::util {
  inline void writeFingerprint(std::ostream& out, std::string_view name, const Colord& color) {
    out << name << '=' << color.r() << ',' << color.g() << ',' << color.b() << ';';
  }

  inline void writeFingerprint(std::ostream& out, std::string_view name, const Vector3d& vector) {
    out << name << '=' << vector.x() << ',' << vector.y() << ',' << vector.z() << ';';
  }
}
