#include "core/formats/ldraw/LDrawFileResolver.h"

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

string LDrawFileResolver::normalizedFilename(string filename) {
  replace(filename.begin(), filename.end(), '\\', '/');
  transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c) {
    return static_cast<char>(tolower(c));
  });
  return filename;
}

LDrawFilesystemResolver::LDrawFilesystemResolver(vector<string> searchDirectories)
    : m_searchDirectories(std::move(searchDirectories)) {
}

void LDrawFilesystemResolver::addSearchDirectory(const string& directory) {
  m_searchDirectories.push_back(directory);
}

unique_ptr<istream> LDrawFilesystemResolver::open(const string& filename) const {
  auto direct = make_unique<ifstream>(filename);
  if (*direct)
    return direct;

  for (const auto& directory : m_searchDirectories) {
    string path = directory;
    if (!path.empty() && path.back() != '/')
      path += '/';
    path += filename;

    auto input = make_unique<ifstream>(path);
    if (*input)
      return input;
  }

  return nullptr;
}

string LDrawFilesystemResolver::cacheKey(const string& filename) const {
  namespace fs = std::filesystem;

  auto keyForExistingPath = [](const fs::path& path) -> string {
    std::error_code error;
    const auto canonical = fs::weakly_canonical(path, error);
    return normalizedFilename(error ? path.lexically_normal().string() : canonical.string());
  };

  fs::path direct(filename);
  std::error_code error;
  if (fs::exists(direct, error))
    return keyForExistingPath(direct);

  for (const auto& directory : m_searchDirectories) {
    fs::path candidate = fs::path(directory) / filename;
    error.clear();
    if (fs::exists(candidate, error))
      return keyForExistingPath(candidate);
  }

  return normalizedFilename(filename);
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
