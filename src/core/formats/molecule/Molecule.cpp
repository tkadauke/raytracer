#include "core/formats/molecule/Molecule.h"

#include <algorithm>

namespace molecule {

  void Molecule::addAtom(const Atom& atom) {
    const auto atomIndex = m_atoms.size();
    const auto chainIndex = chainIndexFor(atom.modelId, atom.chainId);
    const auto residueIndex = residueIndexFor(atom);

    auto& chain = m_chains[chainIndex];
    if (std::find(chain.residueIndices.begin(), chain.residueIndices.end(), residueIndex) ==
        chain.residueIndices.end()) {
      chain.residueIndices.push_back(residueIndex);
    }

    m_residues[residueIndex].atomIndices.push_back(atomIndex);
    m_atoms.push_back(atom);
  }

  bool Molecule::addBond(std::size_t firstAtomIndex, std::size_t secondAtomIndex, int order,
                         bool inferred) {
    if (firstAtomIndex == secondAtomIndex || firstAtomIndex >= m_atoms.size() ||
        secondAtomIndex >= m_atoms.size()) {
      return false;
    }

    if (secondAtomIndex < firstAtomIndex)
      std::swap(firstAtomIndex, secondAtomIndex);

    const auto found = std::find_if(m_bonds.begin(), m_bonds.end(), [&](const Bond& bond) {
      return bond.firstAtomIndex == firstAtomIndex && bond.secondAtomIndex == secondAtomIndex;
    });
    if (found != m_bonds.end())
      return false;

    m_bonds.push_back(Bond{firstAtomIndex, secondAtomIndex, std::max(1, order), inferred});
    return true;
  }

  std::size_t Molecule::modelIndexFor(int modelId) {
    const auto found = std::find_if(m_models.begin(), m_models.end(),
                                    [modelId](const Model& model) { return model.id == modelId; });
    if (found != m_models.end())
      return static_cast<std::size_t>(std::distance(m_models.begin(), found));

    m_models.push_back(Model{modelId, {}});
    return m_models.size() - 1;
  }

  std::size_t Molecule::chainIndexFor(int modelId, const std::string& chainId) {
    const auto found =
      std::find_if(m_chains.begin(), m_chains.end(), [modelId, &chainId](const Chain& chain) {
        return chain.modelId == modelId && chain.id == chainId;
      });
    if (found != m_chains.end())
      return static_cast<std::size_t>(std::distance(m_chains.begin(), found));

    const auto modelIndex = modelIndexFor(modelId);
    m_chains.push_back(Chain{chainId, modelId, {}});
    m_models[modelIndex].chainIndices.push_back(m_chains.size() - 1);
    return m_chains.size() - 1;
  }

  std::size_t Molecule::residueIndexFor(const Atom& atom) {
    const auto found =
      std::find_if(m_residues.begin(), m_residues.end(), [&atom](const Residue& residue) {
        return residue.modelId == atom.modelId && residue.chainId == atom.chainId &&
               residue.sequenceNumber == atom.residueSequence &&
               residue.insertionCode == atom.insertionCode && residue.name == atom.residueName;
      });
    if (found != m_residues.end())
      return static_cast<std::size_t>(std::distance(m_residues.begin(), found));

    m_residues.push_back(Residue{
      atom.residueName, atom.residueSequence, atom.insertionCode, atom.chainId, atom.modelId, {}});
    return m_residues.size() - 1;
  }

}
