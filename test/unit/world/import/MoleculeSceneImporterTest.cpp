#include "world/import/MoleculeSceneImporter.h"

#include "core/formats/molecule/MoleculeParser.h"
#include "render/primitives/Scene.h"
#include "world/objects/ConstantColorTexture.h"
#include "world/objects/Cylinder.h"
#include "world/import/ImportResult.h"
#include "world/import/MoleculeSceneBuilder.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Element.h"
#include "world/objects/Group.h"
#include "world/objects/Curve.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

#include <gtest/gtest.h>

#include <QJsonObject>

#include <fstream>
#include <vector>

namespace MoleculeSceneImporterTest {
  namespace {
    Group* groupChild(Group* parent, int index) {
      return qobject_cast<Group*>(parent->childElements()[index]);
    }

    Sphere* sphereChild(Group* parent, int index) {
      return qobject_cast<Sphere*>(parent->childElements()[index]);
    }

    Curve* curveChild(Group* parent, int index) {
      return qobject_cast<Curve*>(parent->childElements()[index]);
    }

    Cylinder* cylinderChild(Group* parent, int index) {
      return qobject_cast<Cylinder*>(parent->childElements()[index]);
    }

    Colord materialColor(const Surface& surface) {
      auto* material = qobject_cast<MatteMaterial*>(surface.material());
      if (!material)
        return Colord();
      auto* texture = qobject_cast<ConstantColorTexture*>(material->diffuseTexture());
      if (!texture)
        return Colord();
      return texture->color();
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

    Curve* findCurveByMetadata(Element* root, const QString& key, const QString& value) {
      if (auto* curve = qobject_cast<Curve*>(root)) {
        if (curve->metadataValue(key).toString() == value)
          return curve;
      }
      for (auto* child : root->childElements()) {
        if (auto* curve = findCurveByMetadata(child, key, value))
          return curve;
      }
      return nullptr;
    }

    Sphere* findSphereByMetadata(Element* root, const QString& key, const QString& value) {
      if (auto* sphere = qobject_cast<Sphere*>(root)) {
        if (sphere->metadataValue(key).toString() == value)
          return sphere;
      }
      for (auto* child : root->childElements()) {
        if (auto* sphere = findSphereByMetadata(child, key, value))
          return sphere;
      }
      return nullptr;
    }

    void collectCylinders(Element* root, std::vector<Cylinder*>& cylinders) {
      if (auto* cylinder = qobject_cast<Cylinder*>(root))
        cylinders.push_back(cylinder);
      for (auto* child : root->childElements())
        collectCylinders(child, cylinders);
    }
  }

  TEST(MoleculeSceneCompiler, ShouldMapModelsChainsAndResiduesToNestedGroups) {
    std::ifstream input("test/fixtures/molecules/small.pdb");
    ASSERT_TRUE(input.is_open());
    const auto parsed = molecule::MoleculeParser().parsePdb(input);

    world::ImportSourceMetadata source;
    source.sourcePath = "test/fixtures/molecules/small.pdb";
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
    ASSERT_NE(nullptr, atom->material());
    EXPECT_EQ(QString("ball-and-stick"), atom->metadataValue("molecule.representation").toString());
    EXPECT_EQ(QString("element"), atom->metadataValue("molecule.colorScheme").toString());
    EXPECT_EQ(QString("N"), atom->metadataValue("moleculeElement").toString());
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

    auto* bonds = groupChild(chainA, 1);
    ASSERT_NE(nullptr, bonds);
    EXPECT_EQ(QString("bonds"), bonds->metadataValue("molecule.kind").toString());
    auto* bond = cylinderChild(bonds, 0);
    ASSERT_NE(nullptr, bond);
    EXPECT_EQ(QString("bond"), bond->metadataValue("molecule.kind").toString());
    EXPECT_FALSE(bond->metadataValue("moleculeBondInferred").toBool());
    EXPECT_EQ(QString("BOND 1-2"), bond->metadataValue("sourceRecord").toString());
    const auto bondProvenance = world::importProvenance(*bond);
    ASSERT_TRUE(bondProvenance.has_value());
    EXPECT_EQ(QString("bond"), bondProvenance->category["kind"].toString());
    EXPECT_FALSE(bondProvenance->category["inferred"].toBool());
  }

  TEST(MoleculeSceneCompiler, ShouldComposeGroupVisibilityForChainsAndResidues) {
    std::ifstream input("test/fixtures/molecules/small.pdb");
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
    EXPECT_EQ(4u, countVisibleSurfaces(scene));

    chainA->hide();
    EXPECT_EQ(1u, countVisibleSurfaces(scene));

    chainA->show();
    ligand->hide();
    EXPECT_EQ(3u, countVisibleSurfaces(scene));
  }

  TEST(MoleculeSceneCompiler, ShouldBuildBackboneCurvesFromChainAlphaCarbons) {
    std::ifstream input("test/fixtures/molecules/backbone_chain.pdb");
    ASSERT_TRUE(input.is_open());
    const auto parsed = molecule::MoleculeParser().parsePdb(input);

    world::ImportSourceMetadata source;
    source.sourcePath = "test/fixtures/molecules/backbone_chain.pdb";
    source.importerName = "molecule";
    source.formatName = "Molecule";

    world::MoleculeSceneCompileOptions options;
    options.backboneMode = "overlay";
    options.backboneWidth = 0.4;

    auto root = world::MoleculeSceneCompiler().compile(parsed.molecule(), source, options);

    auto* model = groupChild(root.get(), 0);
    auto* chainA = groupChild(model, 0);
    ASSERT_NE(nullptr, chainA);
    auto* backbone = curveChild(chainA, 0);
    ASSERT_NE(nullptr, backbone);

    EXPECT_EQ(QString("backbone"), backbone->metadataValue("molecule.kind").toString());
    EXPECT_EQ(QString("A"), backbone->metadataValue("chainId").toString());
    EXPECT_EQ(QString("overlay"), backbone->metadataValue("molecule.representation").toString());
    EXPECT_EQ(QString("model/1/chain/A/backbone"),
              backbone->metadataValue(GroupMetadata::sourceIdKey()).toString());
    EXPECT_DOUBLE_EQ(0.0, backbone->width());
    EXPECT_EQ(QString("ribbon"), backbone->tessellationMode());

    const auto& polyline = backbone->polyline();
    ASSERT_EQ(3u, polyline.pointCount());
    ASSERT_EQ(2u, polyline.segmentCount());
    EXPECT_EQ(Vector3d(0.0, 0.0, 0.0), polyline.point(0));
    EXPECT_EQ(Vector3d(1.5, 0.25, 0.0), polyline.point(1));
    EXPECT_EQ(Vector3d(2.75, 0.75, 0.5), polyline.point(2));
    ASSERT_NE(nullptr, polyline.attributeAs<std::string>("chainId"));
    EXPECT_EQ("A", *polyline.attributeAs<std::string>("chainId"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<std::string>(0, "startResidueName"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<int>(0, "startResidueIndex"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<std::string>(1, "endResidueName"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<int>(1, "endResidueIndex"));
    EXPECT_EQ("ALA", *polyline.segmentAttributeAs<std::string>(0, "startResidueName"));
    EXPECT_EQ(1, *polyline.segmentAttributeAs<int>(0, "startResidueIndex"));
    EXPECT_EQ("SER", *polyline.segmentAttributeAs<std::string>(1, "endResidueName"));
    EXPECT_EQ(3, *polyline.segmentAttributeAs<int>(1, "endResidueIndex"));
  }

  TEST(MoleculeSceneCompiler, ShouldSupportTubeBackboneModeAndDisableBackbones) {
    std::ifstream input("test/fixtures/molecules/backbone_chain.pdb");
    ASSERT_TRUE(input.is_open());
    const auto parsed = molecule::MoleculeParser().parsePdb(input);

    world::ImportSourceMetadata source;
    world::MoleculeSceneCompileOptions tubeOptions;
    tubeOptions.backboneMode = "tube";
    tubeOptions.backboneWidth = 0.6;
    auto withTube = world::MoleculeSceneCompiler().compile(parsed.molecule(), source, tubeOptions);
    auto* tubeBackbone = curveChild(groupChild(groupChild(withTube.get(), 0), 0), 0);
    ASSERT_NE(nullptr, tubeBackbone);
    EXPECT_DOUBLE_EQ(0.6, tubeBackbone->width());
    EXPECT_EQ(QString("tube"), tubeBackbone->tessellationMode());

    world::MoleculeSceneCompileOptions noBackboneOptions;
    noBackboneOptions.backboneMode = "none";
    noBackboneOptions.backboneWidth = 0.6;
    auto withoutBackbone =
      world::MoleculeSceneCompiler().compile(parsed.molecule(), source, noBackboneOptions);
    auto* chainA = groupChild(groupChild(withoutBackbone.get(), 0), 0);
    ASSERT_NE(nullptr, chainA);
    ASSERT_FALSE(chainA->childElements().isEmpty());
    EXPECT_EQ(nullptr, qobject_cast<Curve*>(chainA->childElements().front()));
  }

  TEST(MoleculeSceneImporter, ShouldImportAndRoundTripGroupedMoleculeJson) {
    world::MoleculeSceneImporter importer;
    world::ImportOptions options;
    options.setValue("atomRadius", 0.1);

    auto result = importer.importFile("test/fixtures/molecules/small.cif", options);

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
    EXPECT_DOUBLE_EQ(world::moleculeElementStyle("N").displayRadius * 0.1, atom->radius());
    EXPECT_EQ(QString("ATOM 1"), atom->metadataValue("sourceRecord").toString());
    const auto provenance = world::importProvenance(*atom);
    ASSERT_TRUE(provenance.has_value());
    EXPECT_EQ(QString("ATOM 1"), provenance->recordId);
    EXPECT_EQ(QString("atom"), provenance->category["kind"].toString());
  }

  TEST(MoleculeSceneImporter, ShouldImportStyledAtomsAndInferredBondsFromMmcif) {
    world::MoleculeSceneImporter importer;

    auto result = importer.importFile("test/fixtures/molecules/small.cif");

    ASSERT_TRUE(result.succeeded());
    auto root = result.takeRoot();
    auto* rootGroup = qobject_cast<Group*>(root.get());
    ASSERT_NE(nullptr, rootGroup);

    auto* nitrogen = findSphereByMetadata(rootGroup, "sourceRecord", "ATOM 1");
    auto* carbon = findSphereByMetadata(rootGroup, "sourceRecord", "ATOM 2");
    ASSERT_NE(nullptr, nitrogen);
    ASSERT_NE(nullptr, carbon);
    EXPECT_EQ(world::moleculeElementStyle("N").color, materialColor(*nitrogen));
    EXPECT_EQ(world::moleculeElementStyle("C").color, materialColor(*carbon));
    EXPECT_DOUBLE_EQ(world::moleculeElementStyle("N").displayRadius * 0.25, nitrogen->radius());
    EXPECT_DOUBLE_EQ(world::moleculeElementStyle("C").displayRadius * 0.25, carbon->radius());
    EXPECT_EQ(QString("ATOM 1"), nitrogen->metadataValue("sourceRecord").toString());
    EXPECT_EQ(QString("model/7/chain/A/residue/GLY/1/atom/1"),
              nitrogen->metadataValue(GroupMetadata::sourceIdKey()).toString());
    ASSERT_TRUE(world::importProvenance(*nitrogen).has_value());

    std::vector<Cylinder*> cylinders;
    collectCylinders(rootGroup, cylinders);
    ASSERT_GE(cylinders.size(), 1u);
    EXPECT_TRUE(cylinders[0]->metadataValue("moleculeBondInferred").toBool());
    EXPECT_EQ(QString("bond"), cylinders[0]->metadataValue("molecule.kind").toString());
    EXPECT_EQ(QString("INFERRED BOND 1-2"),
              cylinders[0]->metadataValue("sourceRecord").toString());
    EXPECT_EQ(QString("model/7/chain/A/bond/1-2"),
              cylinders[0]->metadataValue(GroupMetadata::sourceIdKey()).toString());
    const auto bondProvenance = world::importProvenance(*cylinders[0]);
    ASSERT_TRUE(bondProvenance.has_value());
    EXPECT_EQ(QString("bond"), bondProvenance->category["kind"].toString());
    EXPECT_TRUE(bondProvenance->category["inferred"].toBool());
  }

  TEST(MoleculeSceneImporter, ShouldRespectDisabledBondInference) {
    world::MoleculeSceneImporter importer;
    world::ImportOptions options;
    options.setValue("inferBondsWhenMissing", false);

    auto result = importer.importFile("test/fixtures/molecules/small.cif", options);

    ASSERT_TRUE(result.succeeded());
    auto root = result.takeRoot();
    ASSERT_NE(nullptr, root);
    std::vector<Cylinder*> cylinders;
    collectCylinders(root.get(), cylinders);
    EXPECT_TRUE(cylinders.empty());
  }

  TEST(MoleculeSceneImporter, ShouldExposeBackboneImportOptions) {
    world::MoleculeSceneImporter importer;

    const auto schema = importer.optionSchema();
    ASSERT_EQ(8u, schema.size());
    EXPECT_EQ(QString("representation"), schema[0].name);
    EXPECT_EQ(world::ImportOptionType::Choice, schema[0].type);
    EXPECT_TRUE(schema[0].choices.contains(QStringLiteral("ball-and-stick")));
    EXPECT_TRUE(schema[0].choices.contains(QStringLiteral("space-filling")));
    EXPECT_TRUE(schema[0].choices.contains(QStringLiteral("backbone")));
    EXPECT_EQ(QString("colorScheme"), schema[1].name);
    EXPECT_TRUE(schema[1].choices.contains(QStringLiteral("element")));
    EXPECT_TRUE(schema[1].choices.contains(QStringLiteral("chain")));
    EXPECT_TRUE(schema[1].choices.contains(QStringLiteral("residue-category")));
    EXPECT_EQ(QString("inferBondsWhenMissing"), schema[5].name);
    EXPECT_EQ(world::ImportOptionType::Boolean, schema[5].type);
    EXPECT_EQ(QString("backboneMode"), schema[6].name);
    EXPECT_EQ(world::ImportOptionType::Choice, schema[6].type);
    EXPECT_TRUE(schema[6].choices.contains(QStringLiteral("overlay")));
    EXPECT_TRUE(schema[6].choices.contains(QStringLiteral("ribbon")));
    EXPECT_TRUE(schema[6].choices.contains(QStringLiteral("tube")));
    EXPECT_EQ(QString("backboneWidth"), schema[7].name);
    EXPECT_EQ(world::ImportOptionType::Double, schema[7].type);
  }

  TEST(MoleculeSceneImporter, ShouldImportSpaceFillingAndChainColorOptions) {
    world::MoleculeSceneImporter importer;
    world::ImportOptions options;
    options.setValue("representation", "space-filling");
    options.setValue("colorScheme", "chain");
    options.setValue("spaceFillingScale", 0.5);

    auto result = importer.importFile("test/fixtures/molecules/small.pdb", options);

    ASSERT_TRUE(result.succeeded());
    auto root = result.takeRoot();
    auto* rootGroup = qobject_cast<Group*>(root.get());
    ASSERT_NE(nullptr, rootGroup);
    auto* model = groupChild(rootGroup, 0);
    auto* chainA = groupChild(model, 0);
    auto* chainB = groupChild(model, 1);
    auto* gly = groupChild(chainA, 0);
    auto* ligand = groupChild(chainB, 0);
    auto* nitrogen = sphereChild(gly, 0);
    auto* carbon = sphereChild(gly, 1);
    auto* ligandCarbon = sphereChild(ligand, 0);

    ASSERT_NE(nullptr, nitrogen);
    ASSERT_NE(nullptr, carbon);
    ASSERT_NE(nullptr, ligandCarbon);
    EXPECT_EQ(QString("space-filling"),
              nitrogen->metadataValue("molecule.representation").toString());
    EXPECT_DOUBLE_EQ(world::moleculeElementStyle("N").displayRadius * 0.5, nitrogen->radius());
    EXPECT_DOUBLE_EQ(world::moleculeElementStyle("C").displayRadius * 0.5, carbon->radius());
    EXPECT_EQ(2, model->childElements().size());
    EXPECT_EQ(materialColor(*nitrogen), materialColor(*carbon));
    EXPECT_NE(materialColor(*nitrogen), materialColor(*ligandCarbon));
  }

  TEST(MoleculeSceneImporter, ShouldImportBackboneRepresentationWithoutAtoms) {
    world::MoleculeSceneImporter importer;
    world::ImportOptions options;
    options.setValue("representation", "backbone");

    auto result = importer.importFile("test/fixtures/molecules/backbone_chain.pdb", options);

    ASSERT_TRUE(result.succeeded());
    auto root = result.takeRoot();
    auto* rootGroup = qobject_cast<Group*>(root.get());
    ASSERT_NE(nullptr, rootGroup);
    auto* model = groupChild(rootGroup, 0);
    auto* chainA = groupChild(model, 0);
    ASSERT_NE(nullptr, chainA);
    ASSERT_EQ(4, chainA->childElements().size());
    auto* backbone = curveChild(chainA, 0);
    ASSERT_NE(nullptr, backbone);
    EXPECT_EQ(QString("tube"), backbone->metadataValue("molecule.representation").toString());
    EXPECT_DOUBLE_EQ(0.35, backbone->width());
    EXPECT_EQ(QString("tube"), backbone->tessellationMode());
    for (int i = 1; i != chainA->childElements().size(); ++i)
      EXPECT_EQ(nullptr, qobject_cast<Sphere*>(chainA->childElements()[i]));
  }

  TEST(MoleculeSceneImporter, ShouldImportAndRoundTripBackboneCurves) {
    world::MoleculeSceneImporter importer;
    world::ImportOptions options;
    options.setValue("backboneMode", "tube");
    options.setValue("backboneWidth", 0.75);

    auto result = importer.importFile("test/fixtures/molecules/backbone_chain.pdb", options);

    ASSERT_TRUE(result.succeeded());

    Scene scene;
    scene.addChild(result.takeRoot());
    EXPECT_EQ(6u, countVisibleSurfaces(scene));

    QJsonObject json;
    scene.write(json);

    Scene decoded;
    decoded.read(json);
    decoded.resolveElementReferences();

    auto* backbone = findCurveByMetadata(&decoded, "molecule.kind", "backbone");
    ASSERT_NE(nullptr, backbone);
    EXPECT_DOUBLE_EQ(0.75, backbone->width());
    EXPECT_EQ(QString("tube"), backbone->tessellationMode());
    EXPECT_EQ(QString("A"), backbone->metadataValue("chainId").toString());

    const auto& polyline = backbone->polyline();
    ASSERT_EQ(3u, polyline.pointCount());
    ASSERT_EQ(2u, polyline.segmentCount());
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<std::string>(0, "startResidueName"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<int>(1, "endResidueIndex"));
    EXPECT_EQ("ALA", *polyline.segmentAttributeAs<std::string>(0, "startResidueName"));
    EXPECT_EQ(3, *polyline.segmentAttributeAs<int>(1, "endResidueIndex"));
  }

  TEST(MoleculeSceneImporter, ChoosesBVHForImportedAtomGroups) {
    world::MoleculeSceneImporter importer;
    auto result = importer.importFile("test/fixtures/molecules/small.cif");

    ASSERT_TRUE(result.succeeded());

    Scene scene;
    scene.addChild(result.takeRoot());
    const auto runtime = scene.toRaytracerScene();

    ASSERT_TRUE(runtime->accelerationDecision().has_value());
    EXPECT_EQ(render::SpatialIndexKind::BVH, runtime->accelerationDecision()->spatialIndexKind);
  }

  TEST(MoleculeSceneImporter, ShouldRegisterForMoleculeExtensions) {
    auto importer = world::SceneImporterRegistry::self().createForFile("example.pdb");
    ASSERT_NE(nullptr, importer);
    EXPECT_EQ(QString("molecule"), importer->name());
  }

}
