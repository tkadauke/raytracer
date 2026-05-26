#pragma once

#include "core/formats/AssetResolver.h"
#include "core/formats/ldraw/LDrawParser.h"

#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class LDrawFileResolver {
public:
  virtual ~LDrawFileResolver() = default;

  [[nodiscard]] virtual std::unique_ptr<std::istream> open(const std::string& filename) const = 0;

  [[nodiscard]] virtual std::string cacheKey(const std::string& filename) const;
  [[nodiscard]] virtual std::string resolvePath(const std::string& filename) const;
  [[nodiscard]] virtual std::vector<std::string> searchRoots(const std::string& filename) const;

protected:
  [[nodiscard]] static std::string normalizedFilename(std::string filename);
};

class LDrawFilesystemResolver : public LDrawFileResolver {
public:
  struct CacheStats {
    std::size_t resolutionRequests = 0;
    std::size_t resolutionMisses = 0;
  };

  explicit LDrawFilesystemResolver(std::vector<std::string> searchDirectories = {});

  void addSearchDirectory(const std::string& directory);

  [[nodiscard]] std::unique_ptr<std::istream> open(const std::string& filename) const override;
  [[nodiscard]] std::string cacheKey(const std::string& filename) const override;
  [[nodiscard]] std::string resolvePath(const std::string& filename) const override;
  [[nodiscard]] std::vector<std::string> searchRoots(const std::string& filename) const override;
  [[nodiscard]] CacheStats cacheStats() const;
  void resetCacheStats() const;

private:
  [[nodiscard]] const core::AssetResolver& assetResolver() const;
  [[nodiscard]] const std::optional<core::ResolvedAsset>&
  resolvedAssetFor(const std::string& filename) const;
  void clearCaches();

  std::vector<std::string> m_searchDirectories;
  mutable std::optional<core::AssetResolver> m_assetResolver;
  mutable std::unordered_map<std::string, std::optional<core::ResolvedAsset>> m_resolvedAssets;
  mutable CacheStats m_cacheStats;
};

class LDrawMpdFileResolver : public LDrawFileResolver {
public:
  explicit LDrawMpdFileResolver(const LDrawDocument& document,
                                std::shared_ptr<const LDrawFileResolver> fallback = nullptr);

  [[nodiscard]] std::unique_ptr<std::istream> open(const std::string& filename) const override;
  [[nodiscard]] std::string cacheKey(const std::string& filename) const override;
  [[nodiscard]] std::string resolvePath(const std::string& filename) const override;
  [[nodiscard]] std::vector<std::string> searchRoots(const std::string& filename) const override;

private:
  std::map<std::string, std::string> m_files;
  std::shared_ptr<const LDrawFileResolver> m_fallback;
};
