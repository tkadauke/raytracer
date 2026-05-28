#include "core/formats/molecule/MoleculeParser.h"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

using namespace std;

namespace MoleculeParserTest {

  TEST(MoleculeParser, ShouldParsePdbAtomAndHetatmRecordsFromFixture) {
    ifstream input("test/fixtures/molecules/small.pdb");
    ASSERT_TRUE(input.is_open());

    const auto result = molecule::MoleculeParser().parsePdb(input);
    const auto& molecule = result.molecule();

    ASSERT_FALSE(result.hasErrors());
    ASSERT_EQ(3u, molecule.atoms().size());
    ASSERT_EQ(2u, molecule.residues().size());
    ASSERT_EQ(2u, molecule.chains().size());
    ASSERT_EQ(1u, molecule.models().size());
    ASSERT_EQ(1u, molecule.bonds().size());
    EXPECT_EQ("9XYZ", molecule.metadata().id);
    EXPECT_EQ("SMALL PROTEIN AND LIGAND STYLE MOLECULE", molecule.metadata().title);

    const auto& atom = molecule.atoms()[0];
    EXPECT_FALSE(atom.hetero);
    EXPECT_EQ(1, atom.serialNumber);
    EXPECT_EQ("N", atom.name);
    EXPECT_EQ("GLY", atom.residueName);
    EXPECT_EQ("A", atom.chainId);
    EXPECT_EQ(1, atom.residueSequence);
    EXPECT_EQ("N", atom.element);
    EXPECT_EQ(7, atom.modelId);
    EXPECT_DOUBLE_EQ(11.104, atom.position[0]);
    EXPECT_DOUBLE_EQ(13.207, atom.position[1]);
    EXPECT_DOUBLE_EQ(9.321, atom.position[2]);
    ASSERT_TRUE(atom.occupancy);
    EXPECT_DOUBLE_EQ(1.0, *atom.occupancy);
    ASSERT_TRUE(atom.temperatureFactor);
    EXPECT_DOUBLE_EQ(16.44, *atom.temperatureFactor);

    const auto& ligand = molecule.atoms()[2];
    EXPECT_TRUE(ligand.hetero);
    EXPECT_EQ("LIG", ligand.residueName);
    EXPECT_EQ("B", ligand.chainId);
    EXPECT_EQ(201, ligand.residueSequence);
    EXPECT_DOUBLE_EQ(-1.250, ligand.position[0]);
    EXPECT_DOUBLE_EQ(0.500, ligand.position[1]);
    EXPECT_DOUBLE_EQ(3.750, ligand.position[2]);

    EXPECT_EQ(2u, molecule.residues()[0].atomIndices.size());
    EXPECT_EQ(1u, molecule.residues()[1].atomIndices.size());
    EXPECT_EQ(2u, molecule.models()[0].chainIndices.size());
    EXPECT_EQ(0u, molecule.bonds()[0].firstAtomIndex);
    EXPECT_EQ(1u, molecule.bonds()[0].secondAtomIndex);
    EXPECT_FALSE(molecule.bonds()[0].inferred);
  }

  TEST(MoleculeParser, ShouldParseMmcifAtomSiteLoopFromFixture) {
    ifstream input("test/fixtures/molecules/small.cif");
    ASSERT_TRUE(input.is_open());

    const auto result = molecule::MoleculeParser().parseMmcif(input);
    const auto& parsed = result.molecule();

    ASSERT_FALSE(result.hasErrors());
    ASSERT_EQ(3u, parsed.atoms().size());
    ASSERT_EQ(2u, parsed.residues().size());
    ASSERT_EQ(2u, parsed.chains().size());
    ASSERT_EQ(1u, parsed.models().size());
    EXPECT_TRUE(parsed.bonds().empty());
    EXPECT_EQ("9XYZ", parsed.metadata().id);
    EXPECT_EQ("Small protein ligand mmCIF fixture", parsed.metadata().title);

    const auto& ca = parsed.atoms()[1];
    EXPECT_EQ("CA", ca.name);
    EXPECT_EQ("C", ca.element);
    EXPECT_EQ("GLY", ca.residueName);
    EXPECT_EQ("A", ca.chainId);
    EXPECT_EQ(1, ca.residueSequence);
    EXPECT_EQ(7, ca.modelId);
    EXPECT_DOUBLE_EQ(12.560, ca.position[0]);
    EXPECT_DOUBLE_EQ(13.420, ca.position[1]);
    EXPECT_DOUBLE_EQ(9.118, ca.position[2]);

    const auto& ligand = parsed.atoms()[2];
    EXPECT_TRUE(ligand.hetero);
    EXPECT_EQ("C1", ligand.name);
    EXPECT_EQ("LIG", ligand.residueName);
    EXPECT_EQ("B", ligand.chainId);
    EXPECT_EQ(201, ligand.residueSequence);
  }

  TEST(MoleculeParser, ShouldWarnAndSkipPdbRecordsWithMissingCoordinates) {
    istringstream input(
      "ATOM      1  N   GLY A   1      11.104          9.321  1.00 16.44           N\n");

    const auto result = molecule::MoleculeParser().parsePdb(input);

    EXPECT_TRUE(result.hasWarnings());
    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_EQ(0u, result.molecule().atoms().size());
    EXPECT_NE(string::npos, result.diagnostics()[0].message.find("missing"));
  }

  TEST(MoleculeParser, ShouldWarnAndSkipMmcifRowsWithMissingCoordinates) {
    istringstream input("data_missing\n"
                        "loop_\n"
                        "_atom_site.group_PDB\n"
                        "_atom_site.id\n"
                        "_atom_site.type_symbol\n"
                        "_atom_site.label_atom_id\n"
                        "_atom_site.label_comp_id\n"
                        "_atom_site.label_asym_id\n"
                        "_atom_site.label_seq_id\n"
                        "_atom_site.Cartn_x\n"
                        "_atom_site.Cartn_y\n"
                        "_atom_site.Cartn_z\n"
                        "ATOM 1 N N GLY A 1 1.0 ? 3.0\n");

    const auto result = molecule::MoleculeParser().parseMmcif(input);

    EXPECT_TRUE(result.hasWarnings());
    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_EQ(0u, result.molecule().atoms().size());
    EXPECT_NE(string::npos, result.diagnostics()[0].message.find("missing"));
  }

  TEST(MoleculeParser, ShouldReportUnsupportedPdbRecordsAsDiagnostics) {
    istringstream input("CRYST1   10.000   10.000   10.000  90.00  90.00  90.00 P 1\n");

    const auto result = molecule::MoleculeParser().parsePdb(input);

    ASSERT_EQ(1u, result.diagnostics().size());
    EXPECT_TRUE(result.diagnostics()[0].isWarning());
    EXPECT_NE(string::npos, result.diagnostics()[0].message.find("CRYST1"));
  }

}
