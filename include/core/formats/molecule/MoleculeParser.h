#pragma once

#include "core/formats/molecule/Molecule.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace molecule {

  class MoleculeParseResult {
  public:
    [[nodiscard]] const Molecule& molecule() const {
      return m_molecule;
    }

    [[nodiscard]] Molecule& molecule() {
      return m_molecule;
    }

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const {
      return m_diagnostics;
    }

    [[nodiscard]] bool hasErrors() const;
    [[nodiscard]] bool hasWarnings() const;

    void addDiagnostic(Diagnostic diagnostic);

  private:
    Molecule m_molecule;
    std::vector<Diagnostic> m_diagnostics;
  };

  class MoleculeParser {
  public:
    MoleculeParseResult parsePdb(std::istream& input) const;
    /**
      * Parses PDBx/mmCIF coordinate data.
      *
      * Alternate-location rows are collapsed deterministically by atom site:
      * the first blank or alternate identifier encountered is kept, later
      * alternatives for the same model/chain/residue/atom site are skipped
      * with warnings so usable coordinates still import.
      */
    MoleculeParseResult parseMmcif(std::istream& input) const;
    MoleculeParseResult parse(std::istream& input, const std::string& format) const;
  };

}
