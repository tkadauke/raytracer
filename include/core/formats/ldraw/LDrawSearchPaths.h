#pragma once

#include <QString>

#include <filesystem>
#include <string>
#include <vector>

// Builds the ordered list of directories an LDrawFileResolver should search:
// the model file's own directory, followed by the standard "parts", "parts/s",
// "p", "p/48", and "models" subdirectories of the LDraw library root.
inline std::vector<std::string> ldrawSearchDirectoriesFor(const QString& modelFilePath,
                                                          const QString& libraryPath) {
  namespace fs = std::filesystem;

  std::vector<std::string> directories;
  const fs::path modelPath(modelFilePath.toStdString());
  if (!modelPath.parent_path().empty()) {
    directories.push_back(modelPath.parent_path().string());
  }

  if (!libraryPath.isEmpty()) {
    const fs::path root(libraryPath.toStdString());
    directories.push_back((root / "parts").string());
    directories.push_back((root / "parts" / "s").string());
    directories.push_back((root / "p").string());
    directories.push_back((root / "p" / "48").string());
    directories.push_back((root / "models").string());
  }

  return directories;
}
