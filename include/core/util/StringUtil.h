#pragma once

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <optional>
#include <string>

namespace core::util {

  /**
    * Strictly parses @p text as a `double` using `strtod`: the entire string
    * must be consumed, non-empty, and free of range/conversion errors.
    * Returns `std::nullopt` on any parse failure.
    */
  inline std::optional<double> tryParseStrictDouble(const std::string& text) {
    if (text.empty()) {
      return std::nullopt;
    }
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
      return std::nullopt;
    }
    return value;
  }

  /**
    * Strictly parses @p text as a `long` using `strtol` in the given
    * @p base: the entire string must be consumed, non-empty, and free of
    * range/conversion errors. Returns `std::nullopt` on any parse failure.
    */
  inline std::optional<long> tryParseStrictLong(const std::string& text, int base = 10) {
    if (text.empty()) {
      return std::nullopt;
    }
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text.c_str(), &end, base);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
      return std::nullopt;
    }
    return value;
  }

  inline std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char ch) { return std::isspace(ch); });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
                        return std::isspace(ch);
                      }).base();
    return first < last ? std::string(first, last) : std::string{};
  }

  inline bool startsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), value.begin());
  }

  inline std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
  }

  inline std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
  }

  inline std::string trimRight(std::string value) {
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char ch) { return std::isspace(ch); }).base();
    return std::string(value.begin(), last);
  }

  inline void mergeLabel(std::string& target, const std::string& source) {
    if (source.empty()) {
      return;
    }
    if (target.empty()) {
      target = source;
      return;
    }
    if (target != source) {
      target = "mixed";
    }
  }

}
