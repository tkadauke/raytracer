#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace core {

  enum class AssetCaseSensitivity {
    Exact,
    CaseInsensitive
  };

  struct ResolvedAsset {
    std::filesystem::path path;
    std::string identity;
  };

  class AssetResolutionError : public std::runtime_error {
  public:
    AssetResolutionError(std::string requestedPath,
                         std::vector<std::filesystem::path> searchedRoots);

    [[nodiscard]] const std::string& requestedPath() const;
    [[nodiscard]] const std::vector<std::filesystem::path>& searchedRoots() const;

  private:
    std::string m_requestedPath;
    std::vector<std::filesystem::path> m_searchedRoots;
  };

  class AssetResolver {
  public:
    explicit AssetResolver(
      std::vector<std::filesystem::path> searchRoots = {},
      AssetCaseSensitivity caseSensitivity = AssetCaseSensitivity::Exact);

    void addSearchRoot(std::filesystem::path root);
    void setSearchRoots(std::vector<std::filesystem::path> roots);
    [[nodiscard]] const std::vector<std::filesystem::path>& searchRoots() const;

    void setCaseSensitivity(AssetCaseSensitivity caseSensitivity);
    [[nodiscard]] AssetCaseSensitivity caseSensitivity() const;

    [[nodiscard]] ResolvedAsset resolve(const std::string& requestedPath,
                                        const std::filesystem::path& currentFile = {}) const;
    [[nodiscard]] std::vector<std::filesystem::path> searchedRoots(
      const std::filesystem::path& currentFile = {}) const;

  private:
    [[nodiscard]] ResolvedAsset resolvedAssetForPath(const std::filesystem::path& path) const;

    std::vector<std::filesystem::path> m_searchRoots;
    AssetCaseSensitivity m_caseSensitivity{AssetCaseSensitivity::Exact};
  };

}
