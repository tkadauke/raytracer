#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace world {
  inline std::string normalizedElement(std::string element) {
    element.erase(std::remove_if(element.begin(), element.end(),
                                 [](unsigned char ch) { return std::isspace(ch) != 0; }),
                  element.end());
    if (element.empty())
      return "X";

    element[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(element[0])));
    for (size_t i = 1; i < element.size(); ++i)
      element[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(element[i])));
    return element;
  }
}
