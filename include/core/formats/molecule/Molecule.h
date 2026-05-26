#pragma once

#include "core/math/Vector.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace molecule {

  enum class DiagnosticSeverity { Warning, Error };

  struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::Warning};
    std::string message;
    int line{-1};

    [[nodiscard]] bool isWarning() const {
      return severity == DiagnosticSeverity::Warning;
    }

    [[nodiscard]] bool isError() const {
      return severity == DiagnosticSeverity::Error;
    }
  };

  struct Atom {
    bool hetero{false};
    int serialNumber{0};
    std::string name;
    std::string alternateLocation;
    std::string element;
    std::string residueName;
    int residueSequence{0};
    std::string insertionCode;
    std::string chainId;
    int modelId{1};
    Vector3d position;
    std::optional<double> occupancy;
    std::optional<double> temperatureFactor;
    std::string sourceRecord;
    int sourceLine{-1};
  };

  struct Bond {
    std::size_t firstAtomIndex{0};
    std::size_t secondAtomIndex{0};
    int order{1};
    bool inferred{false};
  };

  struct Residue {
    std::string name;
    int sequenceNumber{0};
    std::string insertionCode;
    std::string chainId;
    int modelId{1};
    std::vector<std::size_t> atomIndices;
  };

  struct Chain {
    std::string id;
    int modelId{1};
    std::vector<std::size_t> residueIndices;
  };

  struct Model {
    int id{1};
    std::vector<std::size_t> chainIndices;
  };

  struct Metadata {
    std::string id;
    std::string title;
  };

  class Molecule {
  public:
    void addAtom(const Atom& atom);
    bool addBond(std::size_t firstAtomIndex, std::size_t secondAtomIndex, int order = 1,
                 bool inferred = false);

    [[nodiscard]] const std::vector<Atom>& atoms() const {
      return m_atoms;
    }

    [[nodiscard]] const std::vector<Residue>& residues() const {
      return m_residues;
    }

    [[nodiscard]] const std::vector<Chain>& chains() const {
      return m_chains;
    }

    [[nodiscard]] const std::vector<Model>& models() const {
      return m_models;
    }

    [[nodiscard]] const std::vector<Bond>& bonds() const {
      return m_bonds;
    }

    [[nodiscard]] const Metadata& metadata() const {
      return m_metadata;
    }

    [[nodiscard]] Metadata& metadata() {
      return m_metadata;
    }

  private:
    [[nodiscard]] std::size_t modelIndexFor(int modelId);
    [[nodiscard]] std::size_t chainIndexFor(int modelId, const std::string& chainId);
    [[nodiscard]] std::size_t residueIndexFor(const Atom& atom);

    std::vector<Atom> m_atoms;
    std::vector<Residue> m_residues;
    std::vector<Chain> m_chains;
    std::vector<Model> m_models;
    std::vector<Bond> m_bonds;
    Metadata m_metadata;
  };

}
