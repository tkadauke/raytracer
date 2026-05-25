#pragma once

#include "core/math/Vector.h"

#include <map>
#include <string>
#include <variant>

namespace core {

  /**
    * Shared metadata storage for curve-like geometry.
    *
    * Curves are intentionally non-polymorphic data models. Concrete curve
    * types such as Polyline own their geometry while this base class provides
    * a common, typed attribute map for whole-curve metadata. Importers for
    * G-code, molecules, trajectories, road routes, GPS traces, or similar
    * path-like data should store data that applies to the entire path here:
    * source file names, route IDs, chain IDs, units, timestamps, visibility
    * flags, and any other metadata that is not tied to one segment.
    *
    * Segment-varying data belongs on the concrete curve type. For polylines,
    * use `Polyline::setSegmentAttribute()` so renderers can color individual
    * spans by scalar values such as feed rate or categorical labels such as
    * route type. `render::Curve` can consume those segment attributes through
    * `AttributeColorMap` when producing ribbon, tube, or overlay output.
    */
  class Curve {
  public:
    using AttributeValue = std::variant<bool, int, double, std::string, Vector3d>;
    using AttributeMap = std::map<std::string, AttributeValue>;

    inline Curve() = default;

    [[nodiscard]] inline const AttributeMap& attributes() const noexcept {
      return m_attributes;
    }

    [[nodiscard]] inline bool hasAttribute(const std::string& name) const {
      return m_attributes.find(name) != m_attributes.end();
    }

    [[nodiscard]] inline const AttributeValue* attribute(const std::string& name) const {
      const auto it = m_attributes.find(name);
      if (it == m_attributes.end())
        return nullptr;
      return &it->second;
    }

    template<class T>
    [[nodiscard]] inline const T* attributeAs(const std::string& name) const {
      const auto* value = attribute(name);
      if (value == nullptr)
        return nullptr;
      return std::get_if<T>(value);
    }

    inline void setAttribute(const std::string& name, const AttributeValue& value) {
      m_attributes[name] = value;
    }

    inline bool removeAttribute(const std::string& name) {
      return m_attributes.erase(name) != 0;
    }

    inline void clearAttributes() {
      m_attributes.clear();
    }

  private:
    AttributeMap m_attributes;
  };

} // namespace core
