#include "core/formats/molecule/MoleculeParser.h"
#include "world/import/MoleculeSceneBuilder.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Group.h"
#include "world/objects/Sphere.h"

#include <gtest/gtest.h>

#include <fstream>
#include <vector>

namespace MoleculeSceneBuilderTest {

  TEST(MoleculeSceneBuilder, ShouldBuildAtomsBondsAndHierarchyFromTinyPdbFixture) {
    std::ifstream input("test/fixtures/molecules/small.pdb");
    ASSERT_TRUE(input.is_open());
    const auto parsed = molecule::MoleculeParser().parsePdb(input);

    const auto root = world::buildBallAndStickMolecule(parsed.molecule());

    ASSERT_EQ(QStringLiteral("SMALL PROTEIN AND LIGAND STYLE MOLECULE"), root->name());
    const auto models = root->childElementsOfType<Group>();
    ASSERT_EQ(1u, models.size());
    EXPECT_EQ(QStringLiteral("Model 7"), models[0]->name());

    const auto modelChildren = models[0]->childElementsOfType<Group>();
    ASSERT_EQ(3u, modelChildren.size());
    EXPECT_EQ(QStringLiteral("Chain A"), modelChildren[0]->name());
    EXPECT_EQ(QStringLiteral("Chain B"), modelChildren[1]->name());
    EXPECT_EQ(QStringLiteral("Bonds"), modelChildren[2]->name());

    const auto chainAResidues = modelChildren[0]->childElementsOfType<Group>();
    ASSERT_EQ(1u, chainAResidues.size());
    EXPECT_EQ(QStringLiteral("GLY 1"), chainAResidues[0]->name());
    const auto glyAtoms = chainAResidues[0]->childElementsOfType<Sphere>();
    ASSERT_EQ(2u, glyAtoms.size());
    EXPECT_EQ(QStringLiteral("N"), glyAtoms[0]->name());
    EXPECT_DOUBLE_EQ(11.104, glyAtoms[0]->position().x());
    EXPECT_GT(glyAtoms[0]->radius(), 0.0);
    ASSERT_NE(nullptr, glyAtoms[0]->material());

    const auto chainBResidues = modelChildren[1]->childElementsOfType<Group>();
    ASSERT_EQ(1u, chainBResidues.size());
    const auto ligandAtoms = chainBResidues[0]->childElementsOfType<Sphere>();
    ASSERT_EQ(1u, ligandAtoms.size());

    const auto bonds = modelChildren[2]->childElementsOfType<Cylinder>();
    ASSERT_EQ(1u, bonds.size());
    EXPECT_DOUBLE_EQ(
      parsed.molecule().atoms()[0].position.distanceTo(parsed.molecule().atoms()[1].position),
      bonds[0]->height());
    EXPECT_FALSE(bonds[0]->metadataValue(QStringLiteral("moleculeBondInferred")).toBool());
  }

  TEST(MoleculeSceneBuilder, ShouldInferBondsWhenConnectivityIsAbsent) {
    std::ifstream input("test/fixtures/molecules/small.cif");
    ASSERT_TRUE(input.is_open());
    const auto parsed = molecule::MoleculeParser().parseMmcif(input);

    const auto bonds = world::moleculeBondsForRendering(parsed.molecule());

    ASSERT_EQ(1u, bonds.size());
    EXPECT_EQ(0u, bonds[0].firstAtomIndex);
    EXPECT_EQ(1u, bonds[0].secondAtomIndex);
    EXPECT_TRUE(bonds[0].inferred);
  }

  TEST(MoleculeSceneBuilder, ShouldUseElementAwareStyles) {
    const auto carbon = world::moleculeElementStyle("c");
    const auto oxygen = world::moleculeElementStyle("O");

    EXPECT_NE(carbon.color, oxygen.color);
    EXPECT_GT(carbon.displayRadius, 0.0);
    EXPECT_GT(oxygen.covalentRadius, 0.0);
  }

}
