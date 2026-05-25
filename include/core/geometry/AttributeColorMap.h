#pragma once

#include "core/Color.h"
#include "core/geometry/Curve.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace core {

  /**
    * Deterministic color mapping for curve attributes.
    *
    * Importers can use scalar maps for ordered values such as feed rate or
    * temperature, and categorical maps for labels such as route type or phase.
    * A missing or incompatible attribute returns `std::nullopt`, leaving the
    * caller's default material/color in place.
    */
  class AttributeColorMap {
  public:
    enum class Mode {
      Scalar,
      Categorical
    };

    inline static AttributeColorMap scalar(std::string attributeName, double minimum,
                                           double maximum, const Colord& lowColor,
                                           const Colord& highColor) {
      AttributeColorMap map(std::move(attributeName), Mode::Scalar);
      map.setScalarRange(minimum, maximum);
      map.setScalarColors(lowColor, highColor);
      return map;
    }

    inline static AttributeColorMap categorical(std::string attributeName) {
      return AttributeColorMap(std::move(attributeName), Mode::Categorical);
    }

    inline explicit AttributeColorMap(std::string attributeName,
                                      Mode mode = Mode::Categorical)
        : m_attributeName(std::move(attributeName)),
          m_mode(mode) {
    }

    [[nodiscard]] inline const std::string& attributeName() const noexcept {
      return m_attributeName;
    }

    [[nodiscard]] inline Mode mode() const noexcept {
      return m_mode;
    }

    inline void setScalarRange(double minimum, double maximum) {
      m_minimum = minimum;
      m_maximum = maximum;
    }

    inline void setScalarColors(const Colord& lowColor, const Colord& highColor) {
      m_lowColor = lowColor;
      m_highColor = highColor;
    }

    inline void setCategoryColor(const Curve::AttributeValue& value, const Colord& color) {
      m_categoryColors[categoryKey(value)] = color;
    }

    inline void setCategoryPalette(const std::vector<Colord>& palette) {
      m_categoryPalette = palette;
    }

    [[nodiscard]] inline std::optional<Colord> colorFor(
      const Curve::AttributeMap& attributes) const {
      const auto it = attributes.find(m_attributeName);
      if (it == attributes.end())
        return std::nullopt;
      return colorFor(it->second);
    }

    [[nodiscard]] inline std::optional<Colord> colorFor(const Curve::AttributeValue& value) const {
      if (m_mode == Mode::Scalar)
        return scalarColor(value);
      return categoricalColor(value);
    }

  private:
    [[nodiscard]] inline static std::optional<double> numericValue(
      const Curve::AttributeValue& value) {
      if (const auto* number = std::get_if<double>(&value))
        return *number;
      if (const auto* number = std::get_if<int>(&value))
        return static_cast<double>(*number);
      return std::nullopt;
    }

    [[nodiscard]] inline std::optional<Colord> scalarColor(
      const Curve::AttributeValue& value) const {
      const auto number = numericValue(value);
      if (!number)
        return std::nullopt;

      double t = 0.0;
      if (m_maximum != m_minimum)
        t = (*number - m_minimum) / (m_maximum - m_minimum);
      t = std::clamp(t, 0.0, 1.0);
      return m_lowColor * (1.0 - t) + m_highColor * t;
    }

    [[nodiscard]] inline std::optional<Colord> categoricalColor(
      const Curve::AttributeValue& value) const {
      const std::string key = categoryKey(value);
      const auto configured = m_categoryColors.find(key);
      if (configured != m_categoryColors.end())
        return configured->second;

      if (m_categoryPalette.empty())
        return std::nullopt;

      return m_categoryPalette[deterministicHash(key) % m_categoryPalette.size()];
    }

    [[nodiscard]] inline static std::string categoryKey(const Curve::AttributeValue& value) {
      return std::visit(
        [](const auto& typedValue) {
          std::ostringstream out;
          using Value = std::decay_t<decltype(typedValue)>;
          if constexpr (std::is_same_v<Value, bool>) {
            out << "bool:" << (typedValue ? "true" : "false");
          } else if constexpr (std::is_same_v<Value, int>) {
            out << "int:" << typedValue;
          } else if constexpr (std::is_same_v<Value, double>) {
            out << "double:" << typedValue;
          } else if constexpr (std::is_same_v<Value, std::string>) {
            out << "string:" << typedValue;
          } else {
            out << "vector3:" << typedValue.x() << ',' << typedValue.y() << ','
                << typedValue.z();
          }
          return out.str();
        },
        value);
    }

    [[nodiscard]] inline static std::uint64_t deterministicHash(const std::string& key) {
      std::uint64_t hash = 14695981039346656037ull;
      for (unsigned char c : key) {
        hash ^= c;
        hash *= 1099511628211ull;
      }
      return hash;
    }

    std::string m_attributeName;
    Mode m_mode;
    double m_minimum{0.0};
    double m_maximum{1.0};
    Colord m_lowColor{Colord::blue()};
    Colord m_highColor{Colord::red()};
    std::map<std::string, Colord> m_categoryColors;
    std::vector<Colord> m_categoryPalette{Colord::red(),   Colord::green(), Colord::blue(),
                                          Colord(1.0, 0.7, 0.0), Colord(0.6, 0.0, 0.8),
                                          Colord(0.0, 0.7, 0.7)};
  };

} // namespace core
