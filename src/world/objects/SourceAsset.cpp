#include "world/objects/SourceAsset.h"

#include "world/import/ImportOptions.h"
#include "world/import/ImportResult.h"
#include "world/import/SceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/Material.h"
#include "world/objects/Scene.h"

#include "render/primitives/Primitive.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QByteArray>

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
    : Group(parent),
      m_material(nullptr) {
  setName("Source Asset");
}

void SourceAsset::setImportOptions(const QJsonObject& options) {
  m_importOptions = options;
  refreshEditableImportProperties();
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

std::unique_ptr<world::SceneImporter> SourceAsset::createImporter(const QString& source) const {
  const QString importerFormat = m_format.trimmed();
  if (importerFormat.isEmpty())
    return world::SceneImporterRegistry::self().createForFile(source);

  return world::SceneImporterRegistry::self().createByFormat(importerFormat);
}

void SourceAsset::refreshEditableImportProperties() {
  removeEditableImportProperties();

  const QString source = resolvedSourcePath().trimmed();
  if (source.isEmpty())
    return;

  auto importer = createImporter(source);
  if (!importer)
    return;

  const auto parameters =
    importer->editableSourceParameters(source, world::ImportOptions(m_importOptions));
  QJsonObject parameterValues = editableImportParameterValues();

  m_blockEditableImportPropertySync = true;
  for (const auto& parameter : parameters) {
    if (parameter.name.trimmed().isEmpty())
      continue;
    if (metaObject()->indexOfProperty(parameter.name.toUtf8().constData()) >= 0)
      continue;

    m_editableImportProperties.push_back(parameter);
    setProperty(parameter.name.toUtf8().constData(),
                editableImportPropertyValue(parameter, parameterValues));
  }
  m_blockEditableImportPropertySync = false;
}

void SourceAsset::removeEditableImportProperties() {
  m_blockEditableImportPropertySync = true;
  for (const auto& parameter : m_editableImportProperties) {
    setProperty(parameter.name.toUtf8().constData(), QVariant());
  }
  m_editableImportProperties.clear();
  m_blockEditableImportPropertySync = false;
}

bool SourceAsset::isEditableImportProperty(const QString& propertyName) const {
  return editableImportPropertySchema(propertyName) != nullptr;
}

const world::ImportOptionSchema*
SourceAsset::editableImportPropertySchema(const QString& propertyName) const {
  for (const auto& parameter : m_editableImportProperties) {
    if (parameter.name == propertyName)
      return &parameter;
  }
  return nullptr;
}

QVariant SourceAsset::editableImportPropertyValue(const world::ImportOptionSchema& schema,
                                                  const QJsonObject& parameters) const {
  if (parameters.contains(schema.name))
    return coerceEditableImportPropertyValue(schema, parameters.value(schema.name).toVariant());

  return coerceEditableImportPropertyValue(schema, schema.defaultValue);
}

QVariant SourceAsset::coerceEditableImportPropertyValue(const world::ImportOptionSchema& schema,
                                                        const QVariant& value) const {
  switch (schema.type) {
  case world::ImportOptionType::Boolean:
    return QVariant::fromValue(value.toBool());
  case world::ImportOptionType::Integer:
    return QVariant::fromValue(value.toInt());
  case world::ImportOptionType::Double:
    return QVariant::fromValue(value.toDouble());
  case world::ImportOptionType::String:
  case world::ImportOptionType::FilePath:
  case world::ImportOptionType::DirectoryPath:
  case world::ImportOptionType::Choice:
    return QVariant::fromValue(value.toString());
  }

  return value;
}

void SourceAsset::setEditableImportParameter(const QString& propertyName, const QVariant& value) {
  const auto* schema = editableImportPropertySchema(propertyName);
  if (!schema)
    return;

  QJsonObject parameters = editableImportParameterValues();
  parameters.insert(propertyName,
                    QJsonValue::fromVariant(coerceEditableImportPropertyValue(*schema, value)));
  m_importOptions.insert(editableImportParameterOptionName(), parameters);
}

QString SourceAsset::editableImportPropertyGroupName() const {
  return m_format.compare(QStringLiteral("openscad"), Qt::CaseInsensitive) == 0
           ? QStringLiteral("OpenSCAD Parameters")
           : QStringLiteral("Source Parameters");
}

QString SourceAsset::editableImportParameterOptionName() const {
  if (m_format.compare(QStringLiteral("openscad"), Qt::CaseInsensitive) == 0)
    return QStringLiteral("define");
  if (m_format.trimmed().isEmpty() &&
      QFileInfo(resolvedSourcePath())
          .suffix()
          .compare(QStringLiteral("scad"), Qt::CaseInsensitive) == 0)
    return QStringLiteral("define");

  return QStringLiteral("parameters");
}

QJsonObject SourceAsset::editableImportParameterValues() const {
  return m_importOptions.value(editableImportParameterOptionName()).toObject();
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

  refreshEditableImportProperties();

  std::unique_ptr<world::SceneImporter> importer = createImporter(source);
  if (!importer) {
    const QString importerFormat = m_format.trimmed();
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

  adoptGeneratedRoot(result.takeRoot());
}

void SourceAsset::adoptGeneratedRoot(std::unique_ptr<Element> root) {
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
  for (const auto& parameter : m_editableImportProperties) {
    json.remove(parameter.name);
  }

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

QString SourceAsset::propertyDisplayName(const QString& propertyName) const {
  if (propertyName == QStringLiteral("material"))
    return QStringLiteral("Material");

  if (const auto* schema = editableImportPropertySchema(propertyName)) {
    if (!schema->label.trimmed().isEmpty())
      return schema->label;
  }

  return Group::propertyDisplayName(propertyName);
}

QString SourceAsset::propertyDescription(const QString& propertyName) const {
  if (propertyName == QStringLiteral("material"))
    return QStringLiteral("Material override applied to generated source geometry.");

  if (const auto* schema = editableImportPropertySchema(propertyName))
    return schema->description;

  return Group::propertyDescription(propertyName);
}

QString SourceAsset::propertyGroup(const QString& propertyName) const {
  if (const auto* schema = editableImportPropertySchema(propertyName)) {
    if (!schema->group.trimmed().isEmpty())
      return schema->group;
    return editableImportPropertyGroupName();
  }

  if (propertyName == QStringLiteral("sourcePath") || propertyName == QStringLiteral("format") ||
      propertyName == QStringLiteral("generatedOutputCacheKey")) {
    return QStringLiteral("Source");
  }
  if (propertyName == QStringLiteral("material"))
    return QStringLiteral("Material");

  return Group::propertyGroup(propertyName);
}

QStringList SourceAsset::propertyChoices(const QString& propertyName) const {
  if (const auto* schema = editableImportPropertySchema(propertyName))
    return schema->choices;

  return Group::propertyChoices(propertyName);
}

std::optional<QPair<double, double>>
SourceAsset::propertyDoubleRange(const QString& propertyName) const {
  const auto* schema = editableImportPropertySchema(propertyName);
  if (!schema || !schema->minimum.isValid() || !schema->maximum.isValid())
    return Group::propertyDoubleRange(propertyName);

  return QPair<double, double>(schema->minimum.toDouble(), schema->maximum.toDouble());
}

std::optional<double> SourceAsset::propertyDoubleStep(const QString& propertyName) const {
  const auto* schema = editableImportPropertySchema(propertyName);
  if (!schema || !schema->step.isValid())
    return Group::propertyDoubleStep(propertyName);

  return schema->step.toDouble();
}

QString SourceAsset::propertyChoiceDisplayName(const QString& propertyName,
                                               const QString& choice) const {
  QString text = choice.trimmed();
  if (text.startsWith(QChar('"')) && text.endsWith(QChar('"')) && text.size() >= 2)
    text = text.mid(1, text.size() - 2);
  return Group::propertyChoiceDisplayName(propertyName, text);
}

void SourceAsset::propertyEdited(const QString& propertyName) {
  applyEditableImportPropertyChange(propertyName);
}

std::optional<Element::AnimationPropertyInfo>
SourceAsset::animationPropertyInfo(const QString& propertyName) const {
  if (const auto* schema = editableImportPropertySchema(propertyName)) {
    const QByteArray propertyKey = propertyName.toUtf8();
    const QVariant value = property(propertyKey.constData());
    const QString typeName = value.typeName() != nullptr ? QString::fromLatin1(value.typeName())
                                                         : QStringLiteral("dynamic");
    return AnimationPropertyInfo{animationPropertyTypeForSchema(*schema), typeName, true};
  }

  return Group::animationPropertyInfo(propertyName);
}

bool SourceAsset::setAnimatedProperty(const QString& propertyName, const QVariant& value) {
  const auto* schema = editableImportPropertySchema(propertyName);
  if (!schema)
    return Group::setAnimatedProperty(propertyName, value);

  const QByteArray propertyKey = propertyName.toUtf8();
  setProperty(propertyKey.constData(), coerceEditableImportPropertyValue(*schema, value));
  applyEditableImportPropertyChange(propertyName);
  return true;
}

void SourceAsset::applyMaterialOverride(std::shared_ptr<render::Primitive> primitive) const {
  if (primitive && material())
    primitive->setMaterial(material()->toRaytracerMaterial());
}

void SourceAsset::applyEditableImportPropertyChange(const QString& propertyName) {
  if (m_blockEditableImportPropertySync || !isEditableImportProperty(propertyName))
    return;

  setEditableImportParameter(propertyName, property(propertyName.toUtf8().constData()));
  rebuildGeneratedChildren();
}

Element::AnimationPropertyType
SourceAsset::animationPropertyTypeForSchema(const world::ImportOptionSchema& schema) const {
  switch (schema.type) {
  case world::ImportOptionType::Boolean:
    return AnimationPropertyType::Boolean;
  case world::ImportOptionType::Integer:
    return AnimationPropertyType::Integer;
  case world::ImportOptionType::Double:
    return AnimationPropertyType::Double;
  case world::ImportOptionType::String:
  case world::ImportOptionType::FilePath:
  case world::ImportOptionType::DirectoryPath:
  case world::ImportOptionType::Choice:
    return AnimationPropertyType::String;
  }

  return AnimationPropertyType::Unsupported;
}

static bool dummy = ElementFactory::self().registerClass<SourceAsset>("SourceAsset");
