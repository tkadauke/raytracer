#include "world/import/ImportResult.h"

#include "world/objects/Element.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

#include <QJsonValue>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace world {
  namespace {
    constexpr const char* ProvenanceMetadataKey = "provenance";
    constexpr const char* LineRangeKey = "lineRange";
  }

  bool ImportProvenance::empty() const {
    return sourceFile.isEmpty() && sourceId.isEmpty() && !lineStart && !lineEnd &&
           recordId.isEmpty() && originalUnits.isEmpty() && category.isEmpty();
  }

  QJsonObject ImportProvenance::toJson() const {
    QJsonObject json;
    if (!sourceFile.isEmpty())
      json["sourceFile"] = sourceFile;
    if (!sourceId.isEmpty())
      json["sourceId"] = sourceId;
    if (lineStart || lineEnd) {
      QJsonObject lineRange;
      if (lineStart)
        lineRange["start"] = *lineStart;
      if (lineEnd)
        lineRange["end"] = *lineEnd;
      json[LineRangeKey] = lineRange;
    }
    if (!recordId.isEmpty())
      json["recordId"] = recordId;
    if (!originalUnits.isEmpty())
      json["originalUnits"] = originalUnits;
    if (!category.isEmpty())
      json["category"] = category;
    return json;
  }

  ImportProvenance ImportProvenance::fromJson(const QJsonObject& json) {
    ImportProvenance provenance;
    provenance.sourceFile = json["sourceFile"].toString();
    provenance.sourceId = json["sourceId"].toString();

    const auto lineRangeValue = json[LineRangeKey];
    if (!lineRangeValue.isUndefined()) {
      if (!lineRangeValue.isObject())
        throw std::invalid_argument("import provenance lineRange must be an object");

      const auto lineRange = lineRangeValue.toObject();
      if (lineRange.contains("start"))
        provenance.lineStart = lineRange["start"].toInt();
      if (lineRange.contains("end"))
        provenance.lineEnd = lineRange["end"].toInt();
    }

    provenance.recordId = json["recordId"].toString();
    provenance.originalUnits = json["originalUnits"].toString();

    const auto categoryValue = json["category"];
    if (!categoryValue.isUndefined()) {
      if (!categoryValue.isObject())
        throw std::invalid_argument("import provenance category must be an object");

      provenance.category = categoryValue.toObject();
    }

    return provenance;
  }

  ImportProvenance ImportProvenance::fromSource(const ImportSourceMetadata& source) {
    ImportProvenance provenance;
    provenance.sourceFile = source.sourcePath;
    return provenance;
  }

  QString importProvenanceMetadataKey() {
    return QString::fromLatin1(ProvenanceMetadataKey);
  }

  std::optional<ImportProvenance> importProvenance(const Element& element) {
    const auto value = element.metadataValue(importProvenanceMetadataKey());
    if (value.isUndefined())
      return std::nullopt;

    if (!value.isObject())
      throw std::invalid_argument("import provenance metadata must be an object");

    return ImportProvenance::fromJson(value.toObject());
  }

  void setImportProvenance(Element& element, const ImportProvenance& provenance) {
    if (provenance.empty()) {
      element.setMetadataValue(importProvenanceMetadataKey(), QJsonValue::Undefined);
    } else {
      element.setMetadataValue(importProvenanceMetadataKey(), provenance.toJson());
    }
  }

  ImportResult::ImportResult() = default;

  ImportResult::ImportResult(std::unique_ptr<Element> root, ImportSourceMetadata source)
      : m_root(std::move(root)),
        m_source(std::move(source)) {
  }

  ImportResult::~ImportResult() = default;

  ImportResult::ImportResult(ImportResult&&) noexcept = default;

  ImportResult& ImportResult::operator=(ImportResult&&) noexcept = default;

  ImportResult ImportResult::failed(std::vector<ImportDiagnostic> diagnostics,
                                    ImportSourceMetadata source) {
    ImportResult result;
    result.m_diagnostics = std::move(diagnostics);
    result.m_source = std::move(source);
    return result;
  }

  ImportResult ImportResult::unreadableSource(ImportSourceMetadata source,
                                              const QString& filename) {
    return failed({ImportDiagnostic::error("Unable to read import source", filename)},
                  std::move(source));
  }

  bool ImportResult::succeeded() const {
    return hasRoot() && !hasErrors();
  }

  bool ImportResult::failed() const {
    return !succeeded();
  }

  bool ImportResult::hasRoot() const {
    return static_cast<bool>(m_root);
  }

  bool ImportResult::hasErrors() const {
    return std::any_of(m_diagnostics.begin(), m_diagnostics.end(),
                       [](const ImportDiagnostic& diagnostic) { return diagnostic.isError(); });
  }

  bool ImportResult::hasWarnings() const {
    return std::any_of(m_diagnostics.begin(), m_diagnostics.end(),
                       [](const ImportDiagnostic& diagnostic) { return diagnostic.isWarning(); });
  }

  Element* ImportResult::root() const {
    return m_root.get();
  }

  Scene* ImportResult::sceneRoot() const {
    return qobject_cast<Scene*>(m_root.get());
  }

  Group* ImportResult::groupRoot() const {
    return qobject_cast<Group*>(m_root.get());
  }

  std::unique_ptr<Element> ImportResult::takeRoot() {
    return std::move(m_root);
  }

  void ImportResult::setRoot(std::unique_ptr<Element> root) {
    m_root = std::move(root);
  }

  const std::vector<ImportDiagnostic>& ImportResult::diagnostics() const {
    return m_diagnostics;
  }

  void ImportResult::addDiagnostic(const ImportDiagnostic& diagnostic) {
    m_diagnostics.push_back(diagnostic);
  }

  const ImportSourceMetadata& ImportResult::source() const {
    return m_source;
  }

  void ImportResult::setSource(ImportSourceMetadata source) {
    m_source = std::move(source);
  }

  std::optional<ImportProvenance> ImportResult::rootProvenance() const {
    if (!m_root)
      return std::nullopt;

    return importProvenance(*m_root);
  }

  void ImportResult::setRootProvenance(const ImportProvenance& provenance) {
    if (m_root)
      setImportProvenance(*m_root, provenance);
  }

}
