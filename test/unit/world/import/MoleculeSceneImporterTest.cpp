#include "world/import/MoleculeSceneImporter.h"

#include "core/formats/molecule/MoleculeParser.h"
#include "world/import/ImportResult.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Element.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

#include <gtest/gtest.h>

#include <QJsonObject>

#include <fstream>

namespace MoleculeSceneImporterTest {
  namespace {
    Group* groupChild(Group* parent, int index) {
      return qobject_cast<Group*>(parent->childElements()[index]);
    }

    Sphere* sphereChild(Group* parent, int index) {
      return qobject_cast<Sphere*>(parent->childElements()[index]);
    }

    std::size_t countVisibleSurfaces(const Scene& scene) {
      return scene.renderGraphAnalysis().visibleSurfaceCount();
    }

    Group* findGroupByMetadata(Element* root, const QString& key, const QString& value) {
      if (auto* group = qobject_cast<Group*>(root)) {
        if (group->metadataValue(key).toString() == value)
          return group;
      }
      for (auto* child : root->childElements()) {
        if (auto* group = findGroupByMetadata(child, key, value))
          return group;
      }
      return nullptr;
    }
  }

  TEST(MoleculeSceneCompiler, ShouldMapModelsChainsAndResiduesToNestedGroups) {
    std::ifstream input("test/fixtures/molecule/small.pdb");
    ASSERT_TRUE(input.is_open());
    const auto parsed = molecule::MoleculeParser().parsePdb(input);

    world::ImportSourceMetadata source;
    source.sourcePath = "test/fixtures/molecule/small.pdb";
    source.importerName = "molecule";
    source.formatName = "Molecule";

    auto root = world::MoleculeSceneCompiler().compile(parsed.molecule(), source);

    ASSERT_EQ(1, root->childElements().size());
    auto* model = groupChild(root.get(), 0);
    ASSERT_NE(nullptr, model);
    EXPECT_EQ(QString("model"), model->metadataValue("molecule.kind").toString());
    EXPECT_EQ(7, model->metadataValue("modelId").toInt());

    ASSERT_EQ(2, model->childElements().size());
    auto* chainA = groupChild(model, 0);
    auto* chainB = groupChild(model, 1);
    ASSERT_NE(nullptr, chainA);
    ASSERT_NE(nullptr, chainB);
    EXPECT_EQ(QString("A"), chainA->metadataValue("chainId").toString());
    EXPECT_EQ(QString("model/7/chain/A"),
              chainA->metadataValue(GroupMetadata::sourceIdKey()).toString());

    auto* gly = groupChild(chainA, 0);
    ASSERT_NE(nullptr, gly);
    EXPECT_EQ(QString("residue"), gly->metadataValue("molecule.kind").toString());
    EXPECT_EQ(QString("A"), gly->metadataValue("chainId").toString());
    EXPECT_EQ(QString("GLY"), gly->metadataValue("residueName").toString());
    EXPECT_EQ(1, gly->metadataValue("residueIndex").toInt());
    EXPECT_EQ(QString("polymer"), gly->metadataValue("molecule.category").toString());

    ASSERT_EQ(2, gly->childElements().size());
    auto* atom = sphereChild(gly, 0);
    ASSERT_NE(nullptr, atom);
    EXPECT_EQ(QString("ATOM 1"), atom->metadataValue("sourceRecord").toString());
    EXPECT_EQ(QString("model/7/chain/A/residue/GLY/1/atom/1"),
              atom->metadataValue(GroupMetadata::sourceIdKey()).toString());
    const auto atomProvenance = world::importProvenance(*atom);
    ASSERT_TRUE(atomProvenance.has_value());
    EXPECT_EQ(QString("ATOM 1"), atomProvenance->recordId);
    ASSERT_TRUE(atomProvenance->lineStart.has_value());
    EXPECT_EQ(4, *atomProvenance->lineStart);

    auto* ligand = groupChild(chainB, 0);
    ASSERT_NE(nullptr, ligand);
    EXPECT_EQ(QString("LIG"), ligand->metadataValue("residueName").toString());
    EXPECT_EQ(201, ligand->metadataValue("residueIndex").toInt());
    EXPECT_EQ(QString("ligand"), ligand->metadataValue("molecule.category").toString());
  }

  TEST(MoleculeSceneCompiler, ShouldComposeGroupVisibilityForChainsAndResidues) {
    std::ifstream input("test/fixtures/molecule/small.pdb");
    ASSERT_TRUE(input.is_open());
    const auto parsed = molecule::MoleculeParser().parsePdb(input);

    world::ImportSourceMetadata source;
    auto root = world::MoleculeSceneCompiler().compile(parsed.molecule(), source);
    auto* model = groupChild(root.get(), 0);
    auto* chainA = groupChild(model, 0);
    auto* chainB = groupChild(model, 1);
    auto* ligand = groupChild(chainB, 0);

    Scene scene;
    scene.addChild(std::move(root));
    EXPECT_EQ(3u, countVisibleSurfaces(scene));

    chainA->hide();
    EXPECT_EQ(1u, countVisibleSurfaces(scene));

    chainA->show();
    ligand->hide();
    EXPECT_EQ(2u, countVisibleSurfaces(scene));
  }

  TEST(MoleculeSceneImporter, ShouldImportAndRoundTripGroupedMoleculeJson) {
    world::MoleculeSceneImporter importer;
    world::ImportOptions options;
    options.setValue("atomRadius", 0.1);

    auto result = importer.importFile("test/fixtures/molecule/small.cif", options);

    ASSERT_TRUE(result.succeeded());
    ASSERT_NE(nullptr, result.groupRoot());
    EXPECT_EQ(QString("molecule"), result.source().importerName);
    EXPECT_FALSE(result.hasErrors());

    Scene scene;
    scene.addChild(result.takeRoot());

    QJsonObject json;
    scene.write(json);

    Scene decoded;
    decoded.read(json);
    decoded.resolveElementReferences();

    auto* root = findGroupByMetadata(&decoded, "molecule.kind", "molecule");
    ASSERT_NE(nullptr, root);
    auto* model = groupChild(root, 0);
    auto* chainA = groupChild(model, 0);
    auto* gly = groupChild(chainA, 0);
    auto* atom = sphereChild(gly, 0);
    ASSERT_NE(nullptr, atom);

    EXPECT_EQ(QString("A"), gly->metadataValue("chainId").toString());
    EXPECT_EQ(QString("GLY"), gly->metadataValue("residueName").toString());
    EXPECT_EQ(1, gly->metadataValue("residueIndex").toInt());
    EXPECT_DOUBLE_EQ(0.1, atom->radius());
    EXPECT_EQ(QString("ATOM 1"), atom->metadataValue("sourceRecord").toString());
    const auto provenance = world::importProvenance(*atom);
    ASSERT_TRUE(provenance.has_value());
    EXPECT_EQ(QString("ATOM 1"), provenance->recordId);
    EXPECT_EQ(QString("atom"), provenance->category["kind"].toString());
  }

  TEST(MoleculeSceneImporter, ShouldRegisterForMoleculeExtensions) {
    auto importer = world::SceneImporterRegistry::self().createForFile("example.pdb");
    ASSERT_NE(nullptr, importer);
    EXPECT_EQ(QString("molecule"), importer->name());
  }

}
