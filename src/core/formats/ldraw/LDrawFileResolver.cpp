#include "core/formats/ldraw/LDrawFileResolver.h"

#include "core/formats/AssetResolver.h"

#include <algorithm>
#include <cctype>
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
  transform(filename.begin(), filename.end(), filename.begin(),
            [](unsigned char c) { return static_cast<char>(tolower(c)); });
  return filename;
}

LDrawFilesystemResolver::LDrawFilesystemResolver(vector<string> searchDirectories)
    : m_searchDirectories(std::move(searchDirectories)) {
}

void LDrawFilesystemResolver::addSearchDirectory(const string& directory) {
  m_searchDirectories.push_back(directory);
}

unique_ptr<istream> LDrawFilesystemResolver::open(const string& filename) const {
  try {
    auto input = make_unique<ifstream>(assetResolver().resolve(filename).path);
    if (*input)
      return input;
  } catch (const core::AssetResolutionError&) {
  }
  return nullptr;
}

string LDrawFilesystemResolver::resolvePath(const string& filename) const {
  try {
    return assetResolver().resolve(filename).path.string();
  } catch (const core::AssetResolutionError&) {
    return "";
  }
}

string LDrawFilesystemResolver::cacheKey(const string& filename) const {
  try {
    return assetResolver().resolve(filename).identity;
  } catch (const core::AssetResolutionError&) {
    return normalizedFilename(filename);
  }
}

vector<string> LDrawFilesystemResolver::searchRoots(const string&) const {
  vector<string> roots;
  const auto resolver = assetResolver();
  for (const auto& root : resolver.searchRoots())
    roots.push_back(root.string());
  return roots;
}

core::AssetResolver LDrawFilesystemResolver::assetResolver() const {
  namespace fs = std::filesystem;

  vector<fs::path> roots;
  roots.emplace_back(".");
  for (const auto& directory : m_searchDirectories)
    roots.emplace_back(directory);
  return core::AssetResolver(std::move(roots), core::AssetCaseSensitivity::CaseInsensitive);
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
