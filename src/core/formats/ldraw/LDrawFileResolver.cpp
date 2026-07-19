#include "core/formats/ldraw/LDrawFileResolver.h"

#include "core/formats/AssetResolver.h"
#include "core/util/StringUtil.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

using namespace std;

string LDrawFileResolver::cacheKey(const string& filename) const {
  return normalizedFilename(filename);
}

vector<string> LDrawFileResolver::searchRoots(const string&) const {
  return {};
}

string LDrawFileResolver::resolvePath(const string&) const {
  return "";
}

string LDrawFileResolver::normalizedFilename(string filename) {
  replace(filename.begin(), filename.end(), '\\', '/');
  return core::util::lowercase(std::move(filename));
}

LDrawFilesystemResolver::LDrawFilesystemResolver(vector<string> searchDirectories)
    : m_searchDirectories(std::move(searchDirectories)) {
}

void LDrawFilesystemResolver::addSearchDirectory(const string& directory) {
  m_searchDirectories.push_back(directory);
  clearCaches();
}

unique_ptr<istream> LDrawFilesystemResolver::open(const string& filename) const {
  const auto& resolved = resolvedAssetFor(filename);
  if (!resolved)
    return nullptr;

  auto input = make_unique<ifstream>(resolved->path);
  if (*input)
    return input;
  return nullptr;
}

string LDrawFilesystemResolver::resolvePath(const string& filename) const {
  const auto& resolved = resolvedAssetFor(filename);
  return resolved ? resolved->path.string() : string();
}

string LDrawFilesystemResolver::cacheKey(const string& filename) const {
  const auto& resolved = resolvedAssetFor(filename);
  if (resolved)
    return resolved->identity;
  return normalizedFilename(filename);
}

vector<string> LDrawFilesystemResolver::searchRoots(const string&) const {
  vector<string> roots;
  for (const auto& root : assetResolver().searchRoots())
    roots.push_back(root.string());
  return roots;
}

const core::AssetResolver& LDrawFilesystemResolver::assetResolver() const {
  if (!m_assetResolver) {
    namespace fs = std::filesystem;

    vector<fs::path> roots;
    roots.emplace_back(".");
    for (const auto& directory : m_searchDirectories)
      roots.emplace_back(directory);
    m_assetResolver.emplace(std::move(roots), core::AssetCaseSensitivity::CaseInsensitive);
  }

  return *m_assetResolver;
}

const optional<core::ResolvedAsset>&
LDrawFilesystemResolver::resolvedAssetFor(const string& filename) const {
  ++m_cacheStats.resolutionRequests;
  const string key = normalizedFilename(filename);
  auto cached = m_resolvedAssets.find(key);
  if (cached != m_resolvedAssets.end())
    return cached->second;

  ++m_cacheStats.resolutionMisses;
  try {
    cached = m_resolvedAssets.emplace(key, assetResolver().resolve(filename)).first;
  } catch (const core::AssetResolutionError&) {
    cached = m_resolvedAssets.emplace(key, nullopt).first;
  }
  return cached->second;
}

LDrawFilesystemResolver::CacheStats LDrawFilesystemResolver::cacheStats() const {
  return m_cacheStats;
}

void LDrawFilesystemResolver::resetCacheStats() const {
  m_cacheStats = CacheStats();
}

void LDrawFilesystemResolver::clearCaches() {
  m_assetResolver.reset();
  m_resolvedAssets.clear();
}

LDrawMpdFileResolver::LDrawMpdFileResolver(const LDrawDocument& document,
                                           shared_ptr<const LDrawFileResolver> fallback)
    : m_fallback(std::move(fallback)) {
  for (const auto& file : document.files) {
    if (!file.filename.empty())
      m_files[normalizedFilename(file.filename)] = file.sourceText;
  }
}

unique_ptr<istream> LDrawMpdFileResolver::open(const string& filename) const {
  const auto local = m_files.find(normalizedFilename(filename));
  if (local != m_files.end())
    return make_unique<istringstream>(local->second);

  if (m_fallback)
    return m_fallback->open(filename);

  return nullptr;
}

string LDrawMpdFileResolver::cacheKey(const string& filename) const {
  const string localKey = normalizedFilename(filename);
  if (m_files.find(localKey) != m_files.end())
    return "mpd:" + localKey;
  if (m_fallback)
    return m_fallback->cacheKey(filename);
  return localKey;
}

string LDrawMpdFileResolver::resolvePath(const string& filename) const {
  if (m_fallback)
    return m_fallback->resolvePath(filename);
  return "";
}

vector<string> LDrawMpdFileResolver::searchRoots(const string& filename) const {
  if (m_fallback)
    return m_fallback->searchRoots(filename);
  return {};
}
