#include "world/import/MoleculeSceneBuilder.h"

#include "core/math/Quaternion.h"
#include "world/objects/ConstantColorTexture.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Group.h"
#include "world/objects/PhongMaterial.h"
#include "world/objects/Sphere.h"

#include <QString>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>

namespace world {
  namespace {
    std::string normalizedElement(std::string element) {
      element.erase(std::remove_if(element.begin(), element.end(),
                                   [](unsigned char ch) { return std::isspace(ch) != 0; }),
                    element.end());
      if (element.empty())
        return "X";

      element[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(element[0])));
      for (size_t i = 1; i < element.size(); ++i)
        element[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(element[i])));
      return element;
    }

    const std::map<std::string, MoleculeElementStyle>& elementStyles() {
      static const std::map<std::string, MoleculeElementStyle> styles = {
        {"H", {Colord(1.0, 1.0, 1.0), 1.20, 0.31}},
        {"C", {Colord(0.20, 0.20, 0.20), 1.70, 0.76}},
        {"N", {Colord(0.10, 0.25, 0.90), 1.55, 0.71}},
        {"O", {Colord(0.90, 0.05, 0.05), 1.52, 0.66}},
        {"F", {Colord(0.30, 0.80, 0.20), 1.47, 0.57}},
        {"P", {Colord(1.0, 0.50, 0.0), 1.80, 1.07}},
        {"S", {Colord(1.0, 0.85, 0.05), 1.80, 1.05}},
        {"Cl", {Colord(0.10, 0.70, 0.10), 1.75, 1.02}},
        {"Br", {Colord(0.55, 0.15, 0.05), 1.85, 1.20}},
        {"I", {Colord(0.45, 0.10, 0.65), 1.98, 1.39}},
        {"Fe", {Colord(0.80, 0.35, 0.10), 1.56, 1.24}},
        {"Mg", {Colord(0.10, 0.55, 0.10), 1.73, 1.30}},
        {"Zn", {Colord(0.45, 0.45, 0.75), 1.39, 1.22}},
      };
      return styles;
    }

    std::unique_ptr<PhongMaterial> makeMaterial(Element* parent, const Colord& color) {
      auto material = std::make_unique<PhongMaterial>(parent);
      material->setName(QStringLiteral("Material"));
      material->setSpecularCoefficient(0.25);
      material->setExponent(24);

      auto texture = new ConstantColorTexture(material.get());
      texture->setColor(color);
      material->setDiffuseTexture(texture);
      return material;
    }

    Matrix4d bondTransform(const Vector3d& first, const Vector3d& second) {
      const auto center = (first + second) * 0.5;
      const auto delta = second - first;
      const auto length = delta.length();
      if (length <= std::numeric_limits<double>::epsilon())
        return Matrix4d::translate(center);

      const auto direction = delta / length;
      const auto up = Vector3d::up();
      const auto dot = std::max(-1.0, std::min(1.0, up * direction));

      Matrix4d rotation;
      if (dot > 1.0 - 1e-9) {
        rotation = Matrix4d();
      } else if (dot < -1.0 + 1e-9) {
        rotation = Quaterniond::fromAxisAngle(Vector3d(1, 0, 0), std::acos(-1.0)).toMatrix4();
      } else {
        const auto axis = (up ^ direction).normalized();
        rotation = Quaterniond::fromAxisAngle(axis, std::acos(dot)).toMatrix4();
      }

      return Matrix4d::translate(center) * rotation;
    }

    QString modelName(int modelId) {
      return QStringLiteral("Model %1").arg(modelId);
    }

    QString chainName(const molecule::Chain& chain) {
      if (chain.id.empty())
        return QStringLiteral("Chain");
      return QStringLiteral("Chain %1").arg(QString::fromStdString(chain.id));
    }

    QString residueName(const molecule::Residue& residue) {
      return QStringLiteral("%1 %2%3")
        .arg(QString::fromStdString(residue.name.empty() ? "Residue" : residue.name))
        .arg(residue.sequenceNumber)
        .arg(QString::fromStdString(residue.insertionCode));
    }

    Group* addGeneratedGroup(Group& parent, const QString& name) {
      auto group = std::make_unique<Group>();
      group->setName(name);
      group->setGenerated(true);
      auto* raw = group.get();
      parent.addChild(std::move(group));
      return raw;
    }

    void addAtom(Group& residueGroup, const molecule::Atom& atom,
                 const MoleculeRenderOptions& options) {
      const auto style = moleculeElementStyle(atom.element);
      auto sphere = std::make_unique<Sphere>();
      sphere->setName(QString::fromStdString(atom.name.empty() ? atom.element : atom.name));
      sphere->setPosition(atom.position);
      sphere->setRadius(style.displayRadius * options.atomRadiusScale);
      sphere->setGenerated(true);
      sphere->setMetadataValue(QStringLiteral("moleculeElement"),
                               QString::fromStdString(normalizedElement(atom.element)));

      auto material = makeMaterial(sphere.get(), style.color);
      sphere->setMaterial(material.release());
      residueGroup.addChild(std::move(sphere));
    }

    void addBond(Group& bondGroup, const molecule::Molecule& molecule, const molecule::Bond& bond,
                 const MoleculeRenderOptions& options) {
      const auto& first = molecule.atoms()[bond.firstAtomIndex];
      const auto& second = molecule.atoms()[bond.secondAtomIndex];
      const auto length = first.position.distanceTo(second.position);
      if (length <= std::numeric_limits<double>::epsilon())
        return;

      auto cylinder = std::make_unique<Cylinder>();
      cylinder->setName(
        QStringLiteral("Bond %1-%2").arg(first.serialNumber).arg(second.serialNumber));
      cylinder->setRadius(options.bondRadius);
      cylinder->setHeight(length);
      cylinder->setMatrix(bondTransform(first.position, second.position));
      cylinder->setGenerated(true);
      cylinder->setMetadataValue(QStringLiteral("moleculeBondInferred"), bond.inferred);

      auto material = makeMaterial(cylinder.get(), Colord(0.75, 0.75, 0.75));
      cylinder->setMaterial(material.release());
      bondGroup.addChild(std::move(cylinder));
    }
  }

  MoleculeElementStyle moleculeElementStyle(const std::string& element) {
    const auto normalized = normalizedElement(element);
    const auto found = elementStyles().find(normalized);
    if (found != elementStyles().end())
      return found->second;
    return {Colord(0.65, 0.65, 0.65), 1.60, 0.77};
  }

  std::vector<molecule::Bond> moleculeBondsForRendering(const molecule::Molecule& molecule,
                                                        const MoleculeRenderOptions& options) {
    if (!molecule.bonds().empty() || !options.inferBondsWhenMissing)
      return molecule.bonds();

    std::vector<molecule::Bond> bonds;
    const auto& atoms = molecule.atoms();
    for (size_t i = 0; i < atoms.size(); ++i) {
      for (size_t j = i + 1; j < atoms.size(); ++j) {
        if (atoms[i].modelId != atoms[j].modelId)
          continue;

        const auto firstStyle = moleculeElementStyle(atoms[i].element);
        const auto secondStyle = moleculeElementStyle(atoms[j].element);
        const auto maxDistance =
          (firstStyle.covalentRadius + secondStyle.covalentRadius) * options.bondInferenceScale;
        if (atoms[i].position.distanceTo(atoms[j].position) <= maxDistance)
          bonds.push_back(molecule::Bond{i, j, 1, true});
      }
    }
    return bonds;
  }

  std::unique_ptr<Group> buildBallAndStickMolecule(const molecule::Molecule& molecule,
                                                   const MoleculeRenderOptions& options) {
    auto root = std::make_unique<Group>();
    root->setName(QString::fromStdString(
      molecule.metadata().title.empty() ? std::string("Molecule") : molecule.metadata().title));
    root->setMetadataValue(QStringLiteral("sourceFormat"), QStringLiteral("molecule"));
    if (!molecule.metadata().id.empty())
      root->setMetadataValue(QStringLiteral("sourceId"),
                             QString::fromStdString(molecule.metadata().id));

    std::map<int, Group*> modelGroups;
    for (const auto& model : molecule.models()) {
      auto* modelGroup = addGeneratedGroup(*root, modelName(model.id));
      modelGroups[model.id] = modelGroup;

      for (const auto chainIndex : model.chainIndices) {
        const auto& chain = molecule.chains()[chainIndex];
        auto* chainGroup = addGeneratedGroup(*modelGroup, chainName(chain));
        for (const auto residueIndex : chain.residueIndices) {
          const auto& residue = molecule.residues()[residueIndex];
          auto* residueGroup = addGeneratedGroup(*chainGroup, residueName(residue));
          for (const auto atomIndex : residue.atomIndices)
            addAtom(*residueGroup, molecule.atoms()[atomIndex], options);
        }
      }
    }

    std::map<int, Group*> bondGroups;
    for (const auto& bond : moleculeBondsForRendering(molecule, options)) {
      const auto modelId = molecule.atoms()[bond.firstAtomIndex].modelId;
      auto& bondGroup = bondGroups[modelId];
      if (!bondGroup)
        bondGroup = addGeneratedGroup(*modelGroups[modelId], QStringLiteral("Bonds"));
      addBond(*bondGroup, molecule, bond, options);
    }

    return root;
  }

}
