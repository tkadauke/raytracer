#include "core/formats/ldraw/LDrawFileResolver.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
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
  return normalizedFilename(filename);
}
