# Molecule fixtures

Tiny PDB and PDBx/mmCIF files for parser, importer, and render smoke tests.
The files are intentionally synthetic: they keep atom counts low enough for
fast CTest runs while still covering polymer atoms, ligand atoms, explicit PDB
connectivity, missing mmCIF connectivity, and CA-trace backbone rendering.

- `small.pdb` covers PDB `HEADER`, `TITLE`, `MODEL`, `ATOM`, `HETATM`,
  `CONECT`, `ENDMDL`, and `END`.
- `small.cif` covers a PDBx/mmCIF `_atom_site` loop with atom coordinates,
  author fields, model number, occupancy, and B-factor values.
- `backbone_chain.pdb` covers a minimal three-residue CA trace for backbone
  curve imports.
