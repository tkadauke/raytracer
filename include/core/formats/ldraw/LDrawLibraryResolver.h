#pragma once

#include "core/formats/ldraw/LDrawDocument.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class LDrawLibraryResolver {
public:
  using DocumentPtr = std::shared_ptr<const LDrawDocument>;

  explicit LDrawLibraryResolver(std::filesystem::path libraryRoot = {});

  void setLibraryRoot(std::filesystem::path libraryRoot);
  [[nodiscard]] const std::filesystem::path& libraryRoot() const;

  DocumentPtr load(const std::filesystem::path& path);
  DocumentPtr loadWithSubfiles(const std::filesystem::path& path);
  DocumentPtr resolve(const LDrawDocument& currentDocument, const std::string& filename);

  [[nodiscard]] std::vector<std::filesystem::path> searchRoots(
    const std::filesystem::path& currentFile) const;
  [[nodiscard]] std::size_t cacheSize() const;
  void clearCache();

private:
  using MutableDocumentPtr = std::shared_ptr<LDrawDocument>;

  MutableDocumentPtr loadMutable(const std::filesystem::path& path);
  MutableDocumentPtr loadWithSubfiles(const std::filesystem::path& path,
                                      std::vector<std::filesystem::path>& stack);
  std::filesystem::path resolvePath(const std::filesystem::path& currentFile,
                                    const std::string& filename) const;

  std::filesystem::path m_libraryRoot;
  std::unordered_map<std::string, MutableDocumentPtr> m_cache;
};
