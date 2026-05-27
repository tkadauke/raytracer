#pragma once

#include "world/import/ImportDiagnostic.h"
#include "world/import/ImportOptions.h"
#include "world/objects/Group.h"

#include <QJsonObject>

#include <memory>
#include <optional>
#include <vector>

namespace world {
  class SceneImporter;
}

/**
  * Durable authoring object for source-backed generated assets.
  *
  * SourceAsset stores the original source path, optional importer format,
  * importer options, and a caller-defined generated-output cache key. When read
  * from scene JSON it resolves the source through the shared SceneImporter
  * registry and attaches the generated output as transient children. Import
  * failures are captured as diagnostics so rendering an incomplete scene stays
  * non-fatal.
  */
class SourceAsset : public Group {
  Q_OBJECT
  Q_PROPERTY(QString sourcePath READ sourcePath WRITE setSourcePath)
  Q_PROPERTY(QString format READ format WRITE setFormat)
  Q_PROPERTY(
    QString generatedOutputCacheKey READ generatedOutputCacheKey WRITE setGeneratedOutputCacheKey)

public:
  explicit SourceAsset(Element* parent = nullptr);

  [[nodiscard]] const QString& sourcePath() const {
    return m_sourcePath;
  }

  void setSourcePath(const QString& sourcePath) {
    m_sourcePath = sourcePath;
  }

  [[nodiscard]] const QString& format() const {
    return m_format;
  }

  void setFormat(const QString& format) {
    m_format = format;
  }

  [[nodiscard]] const QString& generatedOutputCacheKey() const {
    return m_generatedOutputCacheKey;
  }

  void setGeneratedOutputCacheKey(const QString& cacheKey) {
    m_generatedOutputCacheKey = cacheKey;
  }

  [[nodiscard]] const QJsonObject& importOptions() const {
    return m_importOptions;
  }

  void setImportOptions(const QJsonObject& options);

  [[nodiscard]] const std::vector<world::ImportDiagnostic>& diagnostics() const {
    return m_diagnostics;
  }

  void clearDiagnostics();
  void refreshEditableImportProperties();
  void adoptGeneratedRoot(std::unique_ptr<Element> root);
  void rebuildGeneratedChildren();

  void read(const QJsonObject& json) override;
  void write(QJsonObject& json) override;

  QString propertyDisplayName(const QString& propertyName) const override;
  QString propertyDescription(const QString& propertyName) const override;
  QString propertyGroup(const QString& propertyName) const override;
  QStringList propertyChoices(const QString& propertyName) const override;
  std::optional<QPair<double, double>>
  propertyDoubleRange(const QString& propertyName) const override;
  std::optional<double> propertyDoubleStep(const QString& propertyName) const override;
  QString propertyChoiceDisplayName(const QString& propertyName,
                                    const QString& choice) const override;
  void propertyEdited(const QString& propertyName) override;

private:
  [[nodiscard]] std::unique_ptr<world::SceneImporter> createImporter(const QString& source) const;
  [[nodiscard]] QString resolvedSourcePath() const;
  void addDiagnostic(const world::ImportDiagnostic& diagnostic);
  [[nodiscard]] bool isEditableImportProperty(const QString& propertyName) const;
  [[nodiscard]] const world::ImportOptionSchema*
  editableImportPropertySchema(const QString& propertyName) const;
  [[nodiscard]] QVariant editableImportPropertyValue(const world::ImportOptionSchema& schema,
                                                     const QJsonObject& defines) const;
  [[nodiscard]] QVariant coerceEditableImportPropertyValue(const world::ImportOptionSchema& schema,
                                                           const QVariant& value) const;
  [[nodiscard]] QString editableImportPropertyGroupName() const;
  void removeEditableImportProperties();
  void setEditableImportDefine(const QString& propertyName, const QVariant& value);
  void applyEditableImportPropertyChange(const QString& propertyName);

  QString m_sourcePath;
  QString m_format;
  QString m_generatedOutputCacheKey;
  QJsonObject m_importOptions;
  std::vector<world::ImportDiagnostic> m_diagnostics;
  world::ImportOptionSchemas m_editableImportProperties;
  bool m_blockEditableImportPropertySync{false};
};
