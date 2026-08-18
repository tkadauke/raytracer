#pragma once

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace core::formats {

  inline std::string formatPathList(const std::vector<std::filesystem::path>& paths) {
    std::ostringstream message;
    for (std::size_t i = 0; i < paths.size(); ++i) {
      if (i > 0)
        message << ", ";
      message << paths[i].string();
    }
    return message.str();
  }

}
