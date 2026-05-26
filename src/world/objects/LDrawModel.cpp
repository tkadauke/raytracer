#include "world/objects/LDrawModel.h"

#include "core/Exception.h"
#include "core/formats/ldraw/LDrawColorTable.h"
#include "core/formats/ldraw/LDrawFileResolver.h"
#include "core/formats/ldraw/LDrawGeometryCompiler.h"
#include "render/primitives/Composite.h"
#include "world/objects/ElementFactory.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
  std::vector<std::string> searchDirectoriesFor(const QString& modelFilePath,
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
}

LDrawModel::LDrawModel(Element* parent)
    : Surface(parent),
      m_smoothNormals(false) {
  setName("LDraw Model");
}

std::shared_ptr<render::Primitive> LDrawModel::toRaytracerPrimitive() const {
  if (m_filePath.isEmpty()) {
    throw Exception("LDrawModel filePath must not be empty", __FILE__, __LINE__);
  }

  std::ifstream input(m_filePath.toStdString());
  if (!input) {
    std::ostringstream message;
    message << "Unable to read LDraw model '" << m_filePath.toStdString() << "'";
    throw Exception(message.str(), __FILE__, __LINE__);
  }

  LDrawColorTable colors;
  auto resolver = std::make_shared<LDrawFilesystemResolver>(
    searchDirectoriesFor(m_filePath, m_libraryPath));
  const auto normalMode = m_smoothNormals ? LDrawGeometryCompiler::NormalMode::Smooth
                                          : LDrawGeometryCompiler::NormalMode::Flat;
  LDrawGeometryCompiler compiler(resolver, 64, normalMode);
  return compiler.compile(input, colors);
}

static bool dummy = ElementFactory::self().registerClass<LDrawModel>("LDrawModel");
