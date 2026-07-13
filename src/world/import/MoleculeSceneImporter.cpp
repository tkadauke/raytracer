#include "world/import/MoleculeSceneImporter.h"

#include "core/formats/molecule/MoleculeParser.h"
#include "core/util/QStringUtil.h"
#include "MoleculeBondGeometry.h"
#include "MoleculeNormalization.h"
#include "world/import/MoleculeSceneBuilder.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/ConstantColorTexture.h"
#include "world/objects/Curve.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Group.h"
#include "world/objects/PhongMaterial.h"
#include "world/objects/Sphere.h"

#include <QFileInfo>
#include <QJsonObject>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <vector>

using namespace std;

namespace world {
  namespace {
    string trimmedAtomName(string value) {
      value.erase(value.begin(), find_if(value.begin(), value.end(),
                                         [](unsigned char ch) { return !std::isspace(ch); }));
      value.erase(
        find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); })
          .base(),
        value.end());
      return value;
    }

    QString sourceIdForModel(int modelId) {
      return QString("model/%1").arg(modelId);
    }

    QString sourceIdForChain(const molecule::Chain& chain) {
      return QString("%1/chain/%2").arg(sourceIdForModel(chain.modelId), qstr(chain.id));
    }

    QString sourceIdForBackbone(const molecule::Chain& chain) {
      return QString("%1/backbone").arg(sourceIdForChain(chain));
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

    const std::vector<Colord>& chainPalette() {
      static const std::vector<Colord> palette = {
        Colord(0.10, 0.36, 0.95), Colord(0.90, 0.22, 0.16), Colord(0.10, 0.62, 0.32),
        Colord(0.95, 0.68, 0.10), Colord(0.55, 0.25, 0.85), Colord(0.05, 0.65, 0.70),
        Colord(0.90, 0.35, 0.70), Colord(0.50, 0.50, 0.18),
      };
      return palette;
    }

    Colord residueCategoryColor(const QString& category) {
      if (category == QStringLiteral("water"))
        return Colord(0.40, 0.75, 1.0);
      if (category == QStringLiteral("ligand"))
        return Colord(0.95, 0.55, 0.10);
      return Colord(0.30, 0.70, 0.35);
    }

    Colord colorForAtom(const molecule::Atom& atom, const QString& residueCategory,
                        const QString& colorScheme,
                        const std::map<std::string, std::size_t>& chainOrdinals) {
      if (colorScheme == QStringLiteral("chain")) {
        const auto found = chainOrdinals.find(atom.chainId);
        const auto index = found == chainOrdinals.end() ? 0u : found->second;
        return chainPalette()[index % chainPalette().size()];
      }
      if (colorScheme == QStringLiteral("residue-category"))
        return residueCategoryColor(residueCategory);
      return moleculeElementStyle(atom.element).color;
    }

    Colord colorForBond(const molecule::Atom& atom, const QString& colorScheme,
                        const std::map<std::string, std::size_t>& chainOrdinals) {
      if (colorScheme == QStringLiteral("chain")) {
        const auto found = chainOrdinals.find(atom.chainId);
        const auto index = found == chainOrdinals.end() ? 0u : found->second;
        return chainPalette()[index % chainPalette().size()];
      }
      return Colord(0.70, 0.70, 0.74);
    }

    std::unique_ptr<PhongMaterial> makeMaterial(const Colord& color) {
      auto material = std::make_unique<PhongMaterial>();
      material->setName(QStringLiteral("Material"));
      material->setSpecularCoefficient(0.25);
      material->setExponent(24);

      auto texture = new ConstantColorTexture;
      texture->setColor(color);
      material->setDiffuseTexture(texture);
      material->addChild(texture);
      return material;
    }

    const molecule::Atom* alphaCarbonFor(const molecule::Residue& residue,
                                         const molecule::Molecule& molecule) {
      if (residueCategory(residue, molecule) != QStringLiteral("polymer"))
        return nullptr;

      for (const auto atomIndex : residue.atomIndices) {
        const auto& atom = molecule.atoms()[atomIndex];
        if (!atom.hetero && trimmedAtomName(atom.name) == "CA")
          return &atom;
      }
      return nullptr;
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

    void applyCommonCurveMetadata(Curve& curve, const QString& sourceId, const QString& kind) {
      curve.setMetadataValue(GroupMetadata::sourceFormatKey(), QStringLiteral("molecule"));
      curve.setMetadataValue(GroupMetadata::sourceIdKey(), sourceId);
      curve.setMetadataValue(QStringLiteral("molecule.kind"), kind);
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

    QJsonObject sourceParameters(const ImportOptions& options) {
      return options.values().value(QStringLiteral("parameters")).toObject();
    }

    QVariant importOptionValue(const ImportOptions& options, const QString& name,
                               const QVariant& fallback = QVariant()) {
      const auto parameters = sourceParameters(options);
      if (parameters.contains(name))
        return parameters.value(name).toVariant();
      return options.value(name, fallback);
    }

    bool importOptionContains(const ImportOptions& options, const QString& name) {
      return sourceParameters(options).contains(name) || options.contains(name);
    }

    QString representationFromRenderMode(QString renderMode) {
      renderMode = renderMode.trimmed().toLower();
      if (renderMode == QStringLiteral("ball_and_stick"))
        return QStringLiteral("ball-and-stick");
      if (renderMode == QStringLiteral("space_filling"))
        return QStringLiteral("space-filling");
      if (renderMode == QStringLiteral("atoms"))
        return QStringLiteral("atoms");
      return renderMode;
    }

    bool emitsAtoms(const MoleculeSceneCompileOptions& options) {
      return options.representation != QStringLiteral("backbone");
    }

    bool emitsBonds(const MoleculeSceneCompileOptions& options) {
      return options.representation == QStringLiteral("ball-and-stick");
    }

    QString effectiveBackboneMode(const MoleculeSceneCompileOptions& options) {
      if (options.representation == QStringLiteral("backbone") &&
          options.backboneMode == QStringLiteral("overlay"))
        return QStringLiteral("tube");
      return options.backboneMode;
    }

    std::map<std::string, std::size_t> chainOrdinalsFor(const molecule::Molecule& molecule) {
      std::map<std::string, std::size_t> ordinals;
      for (const auto& chain : molecule.chains()) {
        if (ordinals.find(chain.id) == ordinals.end())
          ordinals[chain.id] = ordinals.size();
      }
      return ordinals;
    }

    bool includesModel(int modelId, const MoleculeSceneCompileOptions& options) {
      return !options.modelId || *options.modelId == modelId;
    }

    MoleculeRenderOptions renderOptionsFor(const MoleculeSceneCompileOptions& options) {
      MoleculeRenderOptions renderOptions;
      renderOptions.atomRadiusScale = options.atomRadius;
      renderOptions.bondRadius = options.bondRadius;
      renderOptions.inferBondsWhenMissing = options.inferBondsWhenMissing;
      return renderOptions;
    }

    bool isHydrogen(const molecule::Atom& atom) {
      return normalizedElement(atom.element) == "H";
    }

    bool isWaterResidueName(const std::string& name) {
      const QString normalized = qstr(name).trimmed().toUpper();
      return normalized == QStringLiteral("HOH") || normalized == QStringLiteral("WAT") ||
             normalized == QStringLiteral("H2O");
    }

    bool includesResidue(const molecule::Residue& residue,
                         const MoleculeSceneCompileOptions& options) {
      return options.includeWater || !isWaterResidueName(residue.name);
    }

    bool includesAtom(const molecule::Atom& atom, const MoleculeSceneCompileOptions& options) {
      if (!options.includeHydrogens && isHydrogen(atom))
        return false;
      if (!options.includeWater && isWaterResidueName(atom.residueName))
        return false;
      return true;
    }

    std::unique_ptr<Curve> compileBackboneCurve(const molecule::Molecule& molecule,
                                                const molecule::Chain& chain,
                                                const ImportSourceMetadata& source,
                                                const QString& backboneMode, double backboneWidth) {
      if (backboneMode == QStringLiteral("none"))
        return nullptr;

      struct BackboneResidue {
        const molecule::Residue* residue;
        const molecule::Atom* atom;
      };

      std::vector<BackboneResidue> residues;
      std::vector<Vector3d> points;
      for (const auto residueIndex : chain.residueIndices) {
        const auto& residue = molecule.residues()[residueIndex];
        const auto* atom = alphaCarbonFor(residue, molecule);
        if (!atom)
          continue;

        residues.push_back(BackboneResidue{&residue, atom});
        points.push_back(atom->position);
      }

      if (points.size() < 2)
        return nullptr;

      core::Polyline polyline(points);
      polyline.setAttribute("sourceFormat", std::string("molecule"));
      polyline.setAttribute("molecule.kind", std::string("backbone"));
      polyline.setAttribute("modelId", chain.modelId);
      polyline.setAttribute("chainId", chain.id);
      polyline.setAttribute("representation", backboneMode.toStdString());
      polyline.setAttribute("atomName", std::string("CA"));

      for (std::size_t i = 0; i != polyline.segmentCount(); ++i) {
        const auto& start = *residues[i].residue;
        const auto& end = *residues[i + 1].residue;
        polyline.setSegmentAttribute(i, "chainId", chain.id);
        polyline.setSegmentAttribute(i, "startResidueName", start.name);
        polyline.setSegmentAttribute(i, "startResidueIndex", start.sequenceNumber);
        polyline.setSegmentAttribute(i, "startResidueInsertionCode", start.insertionCode);
        polyline.setSegmentAttribute(i, "endResidueName", end.name);
        polyline.setSegmentAttribute(i, "endResidueIndex", end.sequenceNumber);
        polyline.setSegmentAttribute(i, "endResidueInsertionCode", end.insertionCode);
      }

      auto curve = std::make_unique<Curve>();
      curve->setName(qstr(chain.id).isEmpty() ? QStringLiteral("Backbone <blank>")
                                              : QString("Backbone %1").arg(qstr(chain.id)));
      curve->setPolyline(polyline);
      curve->setWidth(backboneMode == QStringLiteral("overlay") ? 0.0 : backboneWidth);
      curve->setTessellationMode(backboneMode == QStringLiteral("tube") ? QStringLiteral("tube")
                                                                        : QStringLiteral("ribbon"));
      applyCommonCurveMetadata(*curve, sourceIdForBackbone(chain), QStringLiteral("backbone"));
      curve->setMetadataValue(QStringLiteral("modelId"), chain.modelId);
      curve->setMetadataValue(QStringLiteral("chainId"), qstr(chain.id));
      curve->setMetadataValue(QStringLiteral("molecule.representation"), backboneMode);
      curve->setMetadataValue(QStringLiteral("atomName"), QStringLiteral("CA"));
      auto provenance = provenanceFor(source, sourceIdForBackbone(chain),
                                      QString("CHAIN %1 CA BACKBONE").arg(qstr(chain.id)),
                                      QStringLiteral("backbone"));
      provenance.category["representation"] = backboneMode;
      setImportProvenance(*curve, provenance);
      return curve;
    }
  }

  std::unique_ptr<Group>
  MoleculeSceneCompiler::compile(const molecule::Molecule& molecule,
                                 const ImportSourceMetadata& source,
                                 const MoleculeSceneCompileOptions& rawOptions) const {
    MoleculeSceneCompileOptions options = rawOptions;
    options.representation = options.representation.toLower();
    options.colorScheme = options.colorScheme.toLower();
    options.backboneMode = options.backboneMode.toLower();
    if (options.representation == QStringLiteral("ball_and_stick") ||
        options.representation == QStringLiteral("space_filling"))
      options.representation = representationFromRenderMode(options.representation);
    const auto chainOrdinals = chainOrdinalsFor(molecule);
    const auto renderOptions = renderOptionsFor(options);

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
    root->setMetadataValue(QStringLiteral("molecule.representation"), options.representation);
    root->setMetadataValue(QStringLiteral("molecule.colorScheme"), options.colorScheme);
    setImportProvenance(
      *root, provenanceFor(source, root->metadataValue(GroupMetadata::sourceIdKey()).toString(),
                           moleculeId, QStringLiteral("molecule")));

    for (const auto& model : molecule.models()) {
      if (!includesModel(model.id, options))
        continue;

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

        if (auto backbone = compileBackboneCurve(
              molecule, chain, source, effectiveBackboneMode(options), options.backboneWidth))
          chainGroup->addChild(std::move(backbone));

        for (const auto residueIndex : chain.residueIndices) {
          const auto& residue = molecule.residues()[residueIndex];
          if (!includesResidue(residue, options))
            continue;

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

          if (!emitsAtoms(options))
            continue;

          for (const auto atomIndex : residue.atomIndices) {
            const auto& atom = molecule.atoms()[atomIndex];
            if (!includesAtom(atom, options))
              continue;

            auto* atomSphere = new Sphere;
            atomSphere->setName(QString("%1 %2").arg(qstr(atom.name)).arg(atom.serialNumber));
            const auto elementStyle = moleculeElementStyle(atom.element);
            const double radius = options.representation == QStringLiteral("space-filling")
                                    ? elementStyle.displayRadius * options.spaceFillingScale
                                    : elementStyle.displayRadius * renderOptions.atomRadiusScale;
            atomSphere->setRadius(radius);
            atomSphere->setPosition(atom.position);
            auto material =
              makeMaterial(colorForAtom(atom, category, options.colorScheme, chainOrdinals));
            atomSphere->setMaterial(material.get());
            atomSphere->addChild(std::move(material));
            atomSphere->setMetadataValue(GroupMetadata::sourceFormatKey(),
                                         QStringLiteral("molecule"));
            atomSphere->setMetadataValue(GroupMetadata::sourceIdKey(), sourceIdForAtom(atom));
            atomSphere->setMetadataValue(QStringLiteral("molecule.kind"), QStringLiteral("atom"));
            atomSphere->setMetadataValue(QStringLiteral("molecule.representation"),
                                         options.representation);
            atomSphere->setMetadataValue(QStringLiteral("molecule.colorScheme"),
                                         options.colorScheme);
            atomSphere->setMetadataValue(QStringLiteral("chainId"), qstr(atom.chainId));
            atomSphere->setMetadataValue(QStringLiteral("residueName"), qstr(atom.residueName));
            atomSphere->setMetadataValue(QStringLiteral("residueIndex"), atom.residueSequence);
            atomSphere->setMetadataValue(QStringLiteral("atomName"), qstr(atom.name));
            atomSphere->setMetadataValue(QStringLiteral("atomSerialNumber"), atom.serialNumber);
            atomSphere->setMetadataValue(QStringLiteral("moleculeAtomIndex"),
                                         static_cast<int>(atomIndex));
            atomSphere->setMetadataValue(QStringLiteral("element"), qstr(atom.element));
            atomSphere->setMetadataValue(QStringLiteral("moleculeElement"),
                                         qstr(normalizedElement(atom.element)));
            atomSphere->setMetadataValue(QStringLiteral("sourceRecord"), qstr(atom.sourceRecord));
            auto atomProvenance = provenanceFor(source, sourceIdForAtom(atom),
                                                qstr(atom.sourceRecord), QStringLiteral("atom"));
            atomProvenance.lineStart =
              atom.sourceLine > 0 ? optional<int>(atom.sourceLine) : nullopt;
            atomProvenance.lineEnd = atomProvenance.lineStart;
            atomProvenance.category["recordType"] =
              atom.hetero ? QStringLiteral("HETATM") : QStringLiteral("ATOM");
            atomProvenance.category["representation"] = options.representation;
            atomProvenance.category["colorScheme"] = options.colorScheme;
            atomProvenance.category["element"] = qstr(normalizedElement(atom.element));
            atomProvenance.category["atomSerialNumber"] = atom.serialNumber;
            atomProvenance.category["atomIndex"] = static_cast<int>(atomIndex);
            setImportProvenance(*atomSphere, atomProvenance);
            residueGroup->addChild(atomSphere);
          }
        }

        if (emitsBonds(options)) {
          std::unique_ptr<Group> bondGroup;
          for (const auto& bond : moleculeBondsForRendering(molecule, renderOptions)) {
            const auto& first = molecule.atoms()[bond.firstAtomIndex];
            const auto& second = molecule.atoms()[bond.secondAtomIndex];
            if (first.modelId != model.id || second.modelId != model.id ||
                first.chainId != chain.id || second.chainId != chain.id)
              continue;
            if (!includesAtom(first, options) || !includesAtom(second, options))
              continue;

            const auto length = first.position.distanceTo(second.position);
            if (length <= std::numeric_limits<double>::epsilon())
              continue;

            if (!bondGroup) {
              bondGroup = std::make_unique<Group>();
              bondGroup->setName(QStringLiteral("Bonds"));
              bondGroup->setLabel(bondGroup->name());
              applyCommonGroupMetadata(*bondGroup,
                                       sourceIdForChain(chain) + QStringLiteral("/bonds"),
                                       QStringLiteral("bonds"));
              bondGroup->setMetadataValue(QStringLiteral("modelId"), model.id);
              bondGroup->setMetadataValue(QStringLiteral("chainId"), qstr(chain.id));
              bondGroup->setMetadataValue(QStringLiteral("molecule.representation"),
                                          options.representation);
            }

            auto* cylinder = new Cylinder;
            cylinder->setName(
              QStringLiteral("Bond %1-%2").arg(first.serialNumber).arg(second.serialNumber));
            cylinder->setRadius(renderOptions.bondRadius);
            cylinder->setHeight(length);
            cylinder->setMatrix(bondTransform(first.position, second.position));
            auto material = makeMaterial(colorForBond(first, options.colorScheme, chainOrdinals));
            cylinder->setMaterial(material.get());
            cylinder->addChild(std::move(material));
            cylinder->setMetadataValue(GroupMetadata::sourceFormatKey(),
                                       QStringLiteral("molecule"));
            cylinder->setMetadataValue(GroupMetadata::sourceIdKey(), QString("%1/bond/%2-%3")
                                                                       .arg(sourceIdForChain(chain))
                                                                       .arg(first.serialNumber)
                                                                       .arg(second.serialNumber));
            cylinder->setMetadataValue(QStringLiteral("molecule.kind"), QStringLiteral("bond"));
            cylinder->setMetadataValue(QStringLiteral("molecule.representation"),
                                       options.representation);
            cylinder->setMetadataValue(QStringLiteral("molecule.colorScheme"), options.colorScheme);
            cylinder->setMetadataValue(QStringLiteral("firstAtomSerialNumber"), first.serialNumber);
            cylinder->setMetadataValue(QStringLiteral("secondAtomSerialNumber"),
                                       second.serialNumber);
            cylinder->setMetadataValue(QStringLiteral("firstMoleculeAtomIndex"),
                                       static_cast<int>(bond.firstAtomIndex));
            cylinder->setMetadataValue(QStringLiteral("secondMoleculeAtomIndex"),
                                       static_cast<int>(bond.secondAtomIndex));
            cylinder->setMetadataValue(QStringLiteral("moleculeBondInferred"), bond.inferred);
            const QString bondRecord =
              bond.inferred
                ? QStringLiteral("INFERRED BOND %1-%2")
                    .arg(first.serialNumber)
                    .arg(second.serialNumber)
                : QStringLiteral("BOND %1-%2").arg(first.serialNumber).arg(second.serialNumber);
            cylinder->setMetadataValue(QStringLiteral("sourceRecord"), bondRecord);
            auto bondProvenance = provenanceFor(
              source, cylinder->metadataValue(GroupMetadata::sourceIdKey()).toString(), bondRecord,
              QStringLiteral("bond"));
            bondProvenance.category["inferred"] = bond.inferred;
            bondProvenance.category["representation"] = options.representation;
            bondProvenance.category["colorScheme"] = options.colorScheme;
            bondProvenance.category["firstAtomSerialNumber"] = first.serialNumber;
            bondProvenance.category["secondAtomSerialNumber"] = second.serialNumber;
            bondProvenance.category["firstAtomIndex"] = static_cast<int>(bond.firstAtomIndex);
            bondProvenance.category["secondAtomIndex"] = static_cast<int>(bond.secondAtomIndex);
            setImportProvenance(*cylinder, bondProvenance);
            bondGroup->addChild(cylinder);
          }

          if (bondGroup)
            chainGroup->addChild(std::move(bondGroup));
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
    return {
      {"representation",
       ImportOptionType::Choice,
       "Representation",
       "Molecular representation: ball-and-stick, space-filling, or backbone.",
       QStringLiteral("ball-and-stick"),
       false,
       {QStringLiteral("ball-and-stick"), QStringLiteral("space-filling"),
        QStringLiteral("backbone")}},
      {"colorScheme",
       ImportOptionType::Choice,
       "Color scheme",
       "Molecular color mapping: element, chain, or residue-category.",
       QStringLiteral("element"),
       false,
       {QStringLiteral("element"), QStringLiteral("chain"), QStringLiteral("residue-category")}},
      {"atomRadius",
       ImportOptionType::Double,
       "Atom radius scale",
       "Scale applied to element radii for generated ball-and-stick atom sphere surfaces.",
       0.25,
       false,
       {}},
      {"spaceFillingScale",
       ImportOptionType::Double,
       "Space-filling scale",
       "Scale applied to van der Waals radii for space-filling atom spheres.",
       1.0,
       false,
       {}},
      {"bondRadius",
       ImportOptionType::Double,
       "Bond radius",
       "Radius used for generated ball-and-stick bond cylinders.",
       0.08,
       false,
       {}},
      {"inferBondsWhenMissing",
       ImportOptionType::Boolean,
       "Infer missing bonds",
       "Infer simple covalent bonds when the molecule file has no explicit connectivity records.",
       true,
       false,
       {}},
      {"includeHydrogens",
       ImportOptionType::Boolean,
       "Include hydrogens",
       "Include hydrogen atoms and hydrogen bonds in generated molecule geometry.",
       true,
       false,
       {}},
      {"includeWater",
       ImportOptionType::Boolean,
       "Include water",
       "Include water residues in generated molecule geometry.",
       true,
       false,
       {}},
      {"modelId",
       ImportOptionType::Integer,
       "Model",
       "Specific source model to import, or 0 to import all models.",
       0,
       false,
       {},
       {},
       0,
       {},
       1},
      {"backboneMode",
       ImportOptionType::Choice,
       "Backbone mode",
       "Protein CA trace representation: none, overlay, ribbon, or tube.",
       QStringLiteral("overlay"),
       false,
       {QStringLiteral("none"), QStringLiteral("overlay"), QStringLiteral("ribbon"),
        QStringLiteral("tube")}},
      {"backboneWidth",
       ImportOptionType::Double,
       "Backbone width",
       "World-space width used for ribbon and tube backbone curves.",
       0.35,
       false,
       {}}};
  }

  ImportOptionSchemas MoleculeSceneImporter::editableSourceParameters(const QString&,
                                                                      const ImportOptions&) const {
    return {
      {"renderMode",
       ImportOptionType::Choice,
       "Render mode",
       "Molecular render mode for generated source geometry.",
       QStringLiteral("ball_and_stick"),
       false,
       {QStringLiteral("ball_and_stick"), QStringLiteral("space_filling"), QStringLiteral("atoms")},
       QStringLiteral("Molecule Parameters")},
      {"atomRadiusScale",
       ImportOptionType::Double,
       "Atom radius scale",
       "Scale applied to element radii for generated atom spheres.",
       0.25,
       false,
       {},
       QStringLiteral("Molecule Parameters"),
       0.01,
       5.0,
       0.05},
      {"bondRadius",
       ImportOptionType::Double,
       "Bond radius",
       "Radius used for generated ball-and-stick bond cylinders.",
       0.08,
       false,
       {},
       QStringLiteral("Molecule Parameters"),
       0.0,
       2.0,
       0.01},
      {"inferBondsWhenMissing",
       ImportOptionType::Boolean,
       "Infer missing bonds",
       "Infer simple covalent bonds when the molecule file has no explicit connectivity records.",
       true,
       false,
       {},
       QStringLiteral("Molecule Parameters")},
      {"includeHydrogens",
       ImportOptionType::Boolean,
       "Include hydrogens",
       "Include hydrogen atoms and hydrogen bonds in generated molecule geometry.",
       true,
       false,
       {},
       QStringLiteral("Molecule Parameters")},
      {"includeWater",
       ImportOptionType::Boolean,
       "Include water",
       "Include water residues in generated molecule geometry.",
       true,
       false,
       {},
       QStringLiteral("Molecule Parameters")},
      {"modelId",
       ImportOptionType::Integer,
       "Model",
       "Specific source model to generate, or 0 to generate all models.",
       0,
       false,
       {},
       QStringLiteral("Molecule Parameters")}};
  }

  bool MoleculeSceneImporter::wrapDirectImportInSourceAsset() const {
    return true;
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

    MoleculeSceneCompileOptions compileOptions;
    compileOptions.representation =
      importOptionValue(options, QStringLiteral("representation"), compileOptions.representation)
        .toString()
        .toLower();
    const auto renderMode =
      importOptionValue(options, QStringLiteral("renderMode"), QVariant()).toString();
    const bool hasRenderMode = !renderMode.trimmed().isEmpty();
    if (!renderMode.trimmed().isEmpty())
      compileOptions.representation = representationFromRenderMode(renderMode);
    compileOptions.colorScheme =
      importOptionValue(options, QStringLiteral("colorScheme"), compileOptions.colorScheme)
        .toString()
        .toLower();
    compileOptions.atomRadius =
      importOptionValue(options, QStringLiteral("atomRadius"), compileOptions.atomRadius)
        .toDouble();
    compileOptions.atomRadius =
      importOptionValue(options, QStringLiteral("atomRadiusScale"), compileOptions.atomRadius)
        .toDouble();
    compileOptions.spaceFillingScale =
      importOptionValue(options, QStringLiteral("spaceFillingScale"),
                        compileOptions.spaceFillingScale)
        .toDouble();
    compileOptions.bondRadius =
      importOptionValue(options, QStringLiteral("bondRadius"), compileOptions.bondRadius)
        .toDouble();
    compileOptions.inferBondsWhenMissing =
      importOptionValue(options, QStringLiteral("inferBondsWhenMissing"),
                        compileOptions.inferBondsWhenMissing)
        .toBool();
    compileOptions.includeHydrogens = importOptionValue(options, QStringLiteral("includeHydrogens"),
                                                        compileOptions.includeHydrogens)
                                        .toBool();
    compileOptions.includeWater =
      importOptionValue(options, QStringLiteral("includeWater"), compileOptions.includeWater)
        .toBool();
    const int selectedModelId = importOptionValue(options, QStringLiteral("modelId"), 0).toInt();
    if (selectedModelId > 0) {
      compileOptions.modelId = selectedModelId;
      const auto hasModel = any_of(
        parsed.molecule().models().begin(), parsed.molecule().models().end(),
        [selectedModelId](const molecule::Model& model) { return model.id == selectedModelId; });
      if (!hasModel)
        diagnostics.push_back(ImportDiagnostic::warning(
          QStringLiteral("Selected molecule model %1 was not present in the source")
            .arg(selectedModelId),
          filename));
    }
    compileOptions.backboneMode =
      importOptionValue(options, QStringLiteral("backboneMode"), compileOptions.backboneMode)
        .toString()
        .toLower();
    if (hasRenderMode && !importOptionContains(options, QStringLiteral("backboneMode")))
      compileOptions.backboneMode = QStringLiteral("none");
    compileOptions.backboneWidth =
      importOptionValue(options, QStringLiteral("backboneWidth"), compileOptions.backboneWidth)
        .toDouble();
    ImportResult result(MoleculeSceneCompiler().compile(parsed.molecule(), source, compileOptions),
                        source);
    for (const auto& diagnostic : diagnostics)
      result.addDiagnostic(diagnostic);
    return result;
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::MoleculeSceneImporter>("molecule");
