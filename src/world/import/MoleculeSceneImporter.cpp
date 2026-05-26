#include "world/import/MoleculeSceneImporter.h"

#include "core/formats/molecule/MoleculeParser.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Group.h"
#include "world/objects/Sphere.h"

#include <QFileInfo>
#include <QJsonObject>

#include <fstream>

using namespace std;

namespace world {
  namespace {
    QString qstr(const string& value) {
      return QString::fromStdString(value);
    }

    QString sourceIdForModel(int modelId) {
      return QString("model/%1").arg(modelId);
    }

    QString sourceIdForChain(const molecule::Chain& chain) {
      return QString("%1/chain/%2").arg(sourceIdForModel(chain.modelId), qstr(chain.id));
    }

    QString residueToken(const molecule::Residue& residue) {
      QString token = QString("%1/%2").arg(qstr(residue.name)).arg(residue.sequenceNumber);
      if (!residue.insertionCode.empty())
        token += qstr(residue.insertionCode);
      return token;
    }

    QString sourceIdForResidue(const molecule::Residue& residue) {
      return QString("%1/residue/%2")
        .arg(sourceIdForModel(residue.modelId) + "/chain/" + qstr(residue.chainId),
             residueToken(residue));
    }

    QString sourceIdForAtom(const molecule::Atom& atom) {
      QString residue = QString("%1/%2").arg(qstr(atom.residueName)).arg(atom.residueSequence);
      if (!atom.insertionCode.empty())
        residue += qstr(atom.insertionCode);
      return QString("model/%1/chain/%2/residue/%3/atom/%4")
        .arg(atom.modelId)
        .arg(qstr(atom.chainId), residue)
        .arg(atom.serialNumber);
    }

    QString residueCategory(const molecule::Residue& residue, const molecule::Molecule& molecule) {
      const QString name = qstr(residue.name).toUpper();
      if (name == "HOH" || name == "WAT" || name == "H2O")
        return QStringLiteral("water");

      for (const auto atomIndex : residue.atomIndices) {
        if (molecule.atoms()[atomIndex].hetero)
          return QStringLiteral("ligand");
      }
      return QStringLiteral("polymer");
    }

    ImportProvenance provenanceFor(const ImportSourceMetadata& source, const QString& sourceId,
                                   const QString& recordId, const QString& kind) {
      ImportProvenance provenance = ImportProvenance::fromSource(source);
      provenance.sourceId = sourceId;
      provenance.recordId = recordId;
      provenance.originalUnits = QStringLiteral("angstrom");
      provenance.category = QJsonObject{{"kind", kind}};
      return provenance;
    }

    void applyCommonGroupMetadata(Group& group, const QString& sourceId, const QString& kind) {
      group.setMetadataValue(GroupMetadata::sourceFormatKey(), QStringLiteral("molecule"));
      group.setMetadataValue(GroupMetadata::sourceIdKey(), sourceId);
      group.setMetadataValue(QStringLiteral("molecule.kind"), kind);
    }

    ImportDiagnostic convertDiagnostic(const molecule::Diagnostic& diagnostic,
                                       const QString& filename) {
      if (diagnostic.isError())
        return ImportDiagnostic::error(qstr(diagnostic.message), filename, diagnostic.line);
      return ImportDiagnostic::warning(qstr(diagnostic.message), filename, diagnostic.line);
    }

    QString formatForPath(const QString& filename) {
      const QString suffix = QFileInfo(filename).suffix().toLower();
      if (suffix == "cif" || suffix == "mmcif")
        return QStringLiteral("mmcif");
      return QStringLiteral("pdb");
    }
  }

  std::unique_ptr<Group> MoleculeSceneCompiler::compile(const molecule::Molecule& molecule,
                                                        const ImportSourceMetadata& source,
                                                        double atomRadius) const {
    auto root = std::make_unique<Group>();
    const QString moleculeId = qstr(molecule.metadata().id);
    root->setName(qstr(molecule.metadata().title).isEmpty() ? QStringLiteral("Molecule")
                                                            : qstr(molecule.metadata().title));
    applyCommonGroupMetadata(*root, moleculeId.isEmpty() ? QStringLiteral("molecule") : moleculeId,
                             QStringLiteral("molecule"));
    if (!moleculeId.isEmpty())
      root->setMetadataValue(QStringLiteral("molecule.id"), moleculeId);
    if (!molecule.metadata().title.empty())
      root->setMetadataValue(QStringLiteral("molecule.title"), qstr(molecule.metadata().title));
    setImportProvenance(
      *root, provenanceFor(source, root->metadataValue(GroupMetadata::sourceIdKey()).toString(),
                           moleculeId, QStringLiteral("molecule")));

    for (const auto& model : molecule.models()) {
      auto* modelGroup = new Group;
      modelGroup->setName(QString("Model %1").arg(model.id));
      modelGroup->setLabel(modelGroup->name());
      applyCommonGroupMetadata(*modelGroup, sourceIdForModel(model.id), QStringLiteral("model"));
      modelGroup->setMetadataValue(QStringLiteral("modelId"), model.id);
      setImportProvenance(*modelGroup, provenanceFor(source, sourceIdForModel(model.id),
                                                     QString("MODEL %1").arg(model.id),
                                                     QStringLiteral("model")));
      root->addChild(modelGroup);

      for (const auto chainIndex : model.chainIndices) {
        const auto& chain = molecule.chains()[chainIndex];
        auto* chainGroup = new Group;
        chainGroup->setName(qstr(chain.id).isEmpty() ? QString("Chain <blank>")
                                                     : QString("Chain %1").arg(qstr(chain.id)));
        chainGroup->setLabel(chainGroup->name());
        applyCommonGroupMetadata(*chainGroup, sourceIdForChain(chain), QStringLiteral("chain"));
        chainGroup->setMetadataValue(QStringLiteral("modelId"), chain.modelId);
        chainGroup->setMetadataValue(QStringLiteral("chainId"), qstr(chain.id));
        setImportProvenance(*chainGroup, provenanceFor(source, sourceIdForChain(chain),
                                                       QString("CHAIN %1").arg(qstr(chain.id)),
                                                       QStringLiteral("chain")));
        modelGroup->addChild(chainGroup);

        for (const auto residueIndex : chain.residueIndices) {
          const auto& residue = molecule.residues()[residueIndex];
          auto* residueGroup = new Group;
          residueGroup->setName(
            QString("%1 %2").arg(qstr(residue.name)).arg(residue.sequenceNumber));
          residueGroup->setLabel(residueGroup->name());
          applyCommonGroupMetadata(*residueGroup, sourceIdForResidue(residue),
                                   QStringLiteral("residue"));
          const QString category = residueCategory(residue, molecule);
          residueGroup->setMetadataValue(QStringLiteral("modelId"), residue.modelId);
          residueGroup->setMetadataValue(QStringLiteral("chainId"), qstr(residue.chainId));
          residueGroup->setMetadataValue(QStringLiteral("residueName"), qstr(residue.name));
          residueGroup->setMetadataValue(QStringLiteral("residueIndex"), residue.sequenceNumber);
          residueGroup->setMetadataValue(QStringLiteral("residueInsertionCode"),
                                         qstr(residue.insertionCode));
          residueGroup->setMetadataValue(QStringLiteral("molecule.category"), category);
          auto residueProvenance = provenanceFor(source, sourceIdForResidue(residue),
                                                 residueGroup->name(), QStringLiteral("residue"));
          residueProvenance.category["moleculeCategory"] = category;
          setImportProvenance(*residueGroup, residueProvenance);
          chainGroup->addChild(residueGroup);

          for (const auto atomIndex : residue.atomIndices) {
            const auto& atom = molecule.atoms()[atomIndex];
            auto* atomSphere = new Sphere;
            atomSphere->setName(QString("%1 %2").arg(qstr(atom.name)).arg(atom.serialNumber));
            atomSphere->setRadius(atomRadius);
            atomSphere->setPosition(atom.position);
            atomSphere->setMetadataValue(GroupMetadata::sourceFormatKey(),
                                         QStringLiteral("molecule"));
            atomSphere->setMetadataValue(GroupMetadata::sourceIdKey(), sourceIdForAtom(atom));
            atomSphere->setMetadataValue(QStringLiteral("chainId"), qstr(atom.chainId));
            atomSphere->setMetadataValue(QStringLiteral("residueName"), qstr(atom.residueName));
            atomSphere->setMetadataValue(QStringLiteral("residueIndex"), atom.residueSequence);
            atomSphere->setMetadataValue(QStringLiteral("atomName"), qstr(atom.name));
            atomSphere->setMetadataValue(QStringLiteral("atomSerialNumber"), atom.serialNumber);
            atomSphere->setMetadataValue(QStringLiteral("element"), qstr(atom.element));
            atomSphere->setMetadataValue(QStringLiteral("sourceRecord"), qstr(atom.sourceRecord));
            auto atomProvenance = provenanceFor(source, sourceIdForAtom(atom),
                                                qstr(atom.sourceRecord), QStringLiteral("atom"));
            atomProvenance.lineStart =
              atom.sourceLine > 0 ? optional<int>(atom.sourceLine) : nullopt;
            atomProvenance.lineEnd = atomProvenance.lineStart;
            atomProvenance.category["recordType"] =
              atom.hetero ? QStringLiteral("HETATM") : QStringLiteral("ATOM");
            setImportProvenance(*atomSphere, atomProvenance);
            residueGroup->addChild(atomSphere);
          }
        }
      }
    }

    return root;
  }

  QString MoleculeSceneImporter::name() const {
    return QStringLiteral("molecule");
  }

  QStringList MoleculeSceneImporter::supportedExtensions() const {
    return {QStringLiteral("pdb"), QStringLiteral("cif"), QStringLiteral("mmcif")};
  }

  ImportOptionSchemas MoleculeSceneImporter::optionSchema() const {
    return {{"atomRadius",
             ImportOptionType::Double,
             "Atom radius",
             "Radius used for generated atom sphere surfaces.",
             0.25,
             false,
             {}}};
  }

  ImportResult MoleculeSceneImporter::importFile(const QString& filename,
                                                 const ImportOptions& options) const {
    ImportSourceMetadata source;
    source.importerName = name();
    source.formatName = QStringLiteral("Molecule");
    source.sourcePath = filename;
    source.properties = QJsonObject{{"format", formatForPath(filename)}};

    ifstream input(filename.toStdString());
    if (!input) {
      return ImportResult::failed(
        {ImportDiagnostic::error(QStringLiteral("Unable to read import source"), filename)},
        source);
    }

    const auto parsed =
      molecule::MoleculeParser().parse(input, source.properties["format"].toString().toStdString());
    vector<ImportDiagnostic> diagnostics;
    for (const auto& diagnostic : parsed.diagnostics())
      diagnostics.push_back(convertDiagnostic(diagnostic, filename));

    if (parsed.hasErrors()) {
      return ImportResult::failed(std::move(diagnostics), source);
    }

    const double atomRadius = options.value("atomRadius", 0.25).toDouble();
    ImportResult result(MoleculeSceneCompiler().compile(parsed.molecule(), source, atomRadius),
                        source);
    for (const auto& diagnostic : diagnostics)
      result.addDiagnostic(diagnostic);
    return result;
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::MoleculeSceneImporter>("molecule");
