#include "world/objects/SourceAsset.h"

#include "world/import/ImportOptions.h"
#include "world/import/ImportResult.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/Scene.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <memory>
#include <stdexcept>
#include <utility>

namespace {
  QString diagnosticSeverityName(const world::ImportDiagnostic& diagnostic) {
    return diagnostic.isError() ? QStringLiteral("error") : QStringLiteral("warning");
  }

  world::ImportDiagnosticSeverity diagnosticSeverityFromName(const QString& name) {
    return name == QStringLiteral("error") ? world::ImportDiagnosticSeverity::Error
                                           : world::ImportDiagnosticSeverity::Warning;
  }

  QJsonObject diagnosticToJson(const world::ImportDiagnostic& diagnostic) {
    QJsonObject json;
    json["severity"] = diagnosticSeverityName(diagnostic);
    json["message"] = diagnostic.message;
    if (!diagnostic.source.isEmpty())
      json["source"] = diagnostic.source;
    if (diagnostic.line >= 0)
      json["line"] = diagnostic.line;
    if (diagnostic.column >= 0)
      json["column"] = diagnostic.column;
    return json;
  }

  world::ImportDiagnostic diagnosticFromJson(const QJsonObject& json) {
    world::ImportDiagnostic diagnostic;
    diagnostic.severity = diagnosticSeverityFromName(json["severity"].toString());
    diagnostic.message = json["message"].toString();
    diagnostic.source = json["source"].toString();
    diagnostic.line = json.contains("line") ? json["line"].toInt(-1) : -1;
    diagnostic.column = json.contains("column") ? json["column"].toInt(-1) : -1;
    return diagnostic;
  }

  void markGeneratedSubtree(Element& element) {
    element.setGenerated(true);
    for (Element* child : element.childElements()) {
      markGeneratedSubtree(*child);
    }
  }
}

SourceAsset::SourceAsset(Element* parent)
    : Group(parent) {
  setName("Source Asset");
}

void SourceAsset::clearDiagnostics() {
  m_diagnostics.clear();
}

void SourceAsset::addDiagnostic(const world::ImportDiagnostic& diagnostic) {
  m_diagnostics.push_back(diagnostic);
}

QString SourceAsset::resolvedSourcePath() const {
  if (m_sourcePath.isEmpty() || !QFileInfo(m_sourcePath).isRelative())
    return m_sourcePath;

  const Element* root = this;
  while (root->parent()) {
    root = root->parent();
  }

  const QString sourceFile = root->property("_sourceFile").toString();
  const QString basePath = QFileInfo(sourceFile).absolutePath();
  const QDir baseDir(basePath.isEmpty() ? QDir::currentPath() : basePath);
  return baseDir.filePath(m_sourcePath);
}

void SourceAsset::rebuildGeneratedChildren() {
  for (int i = childElements().size() - 1; i >= 0; --i) {
    if (childElements()[i]->isGenerated()) {
      Element* child = childElements()[i];
      removeChild(i);
      delete child;
    }
  }

  clearDiagnostics();

  const QString source = resolvedSourcePath().trimmed();
  if (source.isEmpty()) {
    addDiagnostic(world::ImportDiagnostic::error("Source asset source path must not be empty"));
    return;
  }

  std::unique_ptr<world::SceneImporter> importer;
  const QString importerFormat = m_format.trimmed();
  if (importerFormat.isEmpty()) {
    importer = world::SceneImporterRegistry::self().createForFile(source);
  } else {
    importer = world::SceneImporterRegistry::self().createByFormat(importerFormat);
  }

  if (!importer) {
    const QString key = importerFormat.isEmpty() ? QFileInfo(source).suffix() : importerFormat;
    addDiagnostic(world::ImportDiagnostic::error(
      QString("No scene importer registered for source asset format: %1").arg(key), source));
    return;
  }

  world::ImportResult result = importer->importFile(source, world::ImportOptions(m_importOptions));
  const QString generatedOutputCacheKey =
    result.source().properties.value("generatedOutputCacheKey").toString();
  if (!generatedOutputCacheKey.isEmpty())
    setGeneratedOutputCacheKey(generatedOutputCacheKey);

  for (const auto& diagnostic : result.diagnostics()) {
    addDiagnostic(diagnostic);
  }
  if (result.failed()) {
    if (m_diagnostics.empty()) {
      addDiagnostic(world::ImportDiagnostic::error("Unable to import source asset", source));
    }
    return;
  }

  adoptImportedRoot(result.takeRoot());
}

void SourceAsset::adoptImportedRoot(std::unique_ptr<Element> root) {
  if (!root)
    return;

  if (auto* importedScene = qobject_cast<Scene*>(root.get())) {
    while (!importedScene->childElements().empty()) {
      Element* child = importedScene->childElements().front();
      markGeneratedSubtree(*child);
      addChild(child);
    }
    return;
  }

  markGeneratedSubtree(*root);
  addChild(std::move(root));
}

void SourceAsset::read(const QJsonObject& json) {
  QJsonObject baseJson = json;
  baseJson.remove("importOptions");
  baseJson.remove("diagnostics");
  Group::read(baseJson);

  const auto optionsValue = json["importOptions"];
  if (!optionsValue.isUndefined()) {
    if (!optionsValue.isObject())
      throw std::invalid_argument("source asset importOptions must be an object");
    m_importOptions = optionsValue.toObject();
  } else {
    m_importOptions = QJsonObject();
  }

  m_diagnostics.clear();
  const auto diagnosticsValue = json["diagnostics"];
  if (!diagnosticsValue.isUndefined()) {
    if (!diagnosticsValue.isArray())
      throw std::invalid_argument("source asset diagnostics must be an array");
    for (const auto& value : diagnosticsValue.toArray()) {
      if (!value.isObject())
        throw std::invalid_argument("source asset diagnostic entries must be objects");
      m_diagnostics.push_back(diagnosticFromJson(value.toObject()));
    }
  }

  rebuildGeneratedChildren();
}

void SourceAsset::write(QJsonObject& json) {
  Group::write(json);

  if (!m_importOptions.isEmpty())
    json["importOptions"] = m_importOptions;

  if (!m_diagnostics.empty()) {
    QJsonArray diagnostics;
    for (const auto& diagnostic : m_diagnostics) {
      diagnostics.append(diagnosticToJson(diagnostic));
    }
    json["diagnostics"] = diagnostics;
  }
}

QString SourceAsset::propertyGroup(const QString& propertyName) const {
  if (propertyName == QStringLiteral("sourcePath") || propertyName == QStringLiteral("format") ||
      propertyName == QStringLiteral("generatedOutputCacheKey")) {
    return QStringLiteral("Source");
  }

  return Group::propertyGroup(propertyName);
}

static bool dummy = ElementFactory::self().registerClass<SourceAsset>("SourceAsset");
