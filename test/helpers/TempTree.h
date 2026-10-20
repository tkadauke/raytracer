#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace test {
  /// Scratch directory that lives for the duration of one test and is removed on destruction.
  /// The directory name embeds @p prefix and the gtest random seed so concurrent runs don't clash.
  class TempTree {
  public:
    explicit TempTree(const std::string& prefix)
        : m_root(std::filesystem::temp_directory_path() /
                 std::filesystem::path(prefix +
                                       std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                                       "-" + std::to_string(++s_nextId))) {
      std::filesystem::create_directories(m_root);
      std::error_code error;
      const std::filesystem::path canonical = std::filesystem::weakly_canonical(m_root, error);
      if (!error)
        m_root = canonical;
    }

    TempTree(const TempTree&) = delete;
    TempTree& operator=(const TempTree&) = delete;

    ~TempTree() {
      std::error_code error;
      std::filesystem::remove_all(m_root, error);
    }

    [[nodiscard]] std::filesystem::path root() const {
      return m_root;
    }

    /// Writes @p contents to @p relativePath below the root, creating parent directories.
    std::filesystem::path write(const std::filesystem::path& relativePath,
                                const std::string& contents = "") const {
      const std::filesystem::path path = m_root / relativePath;
      std::filesystem::create_directories(path.parent_path());
      std::ofstream output(path);
      output << contents;
      return path;
    }

  private:
    std::filesystem::path m_root;
    static inline int s_nextId = 0;
  };
}
