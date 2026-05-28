# Molecule import

The molecule importer turns small PDB and PDBx/mmCIF coordinate files into
editable scene groups and renderable primitives. It is aimed at molecular
visualization smoke tests and educational previews, not full structural
biology workflows.

PDB files are parsed from fixed-width `ATOM` and `HETATM` coordinate records.
`HEADER` supplies the molecule id, `TITLE` lines are concatenated into the
scene name, `MODEL` / `ENDMDL` create model groups, and `CONECT` creates
explicit bonds when present. Unsupported records are reported as warnings so a
coordinate-only file can still render.

PDBx/mmCIF support reads the `_atom_site` loop subset needed for atom
positions and hierarchy. The importer uses `group_PDB`, `id`, `type_symbol`,
atom / residue / chain labels, Cartesian coordinates, occupancy, B-factor,
author residue fields, and `pdbx_PDB_model_num` when those columns are present.
It also reads `_struct.title` and the `data_` block id for scene metadata.
mmCIF bond tables, symmetry operators, assemblies, secondary structure, and
chemical-component dictionaries are not imported.

## Representations

`rendercli`, Modeler, and scene `SourceAsset` imports use the shared importer
option surface:

- `representation=ball-and-stick` creates element-colored atom spheres and bond
  cylinders. PDB `CONECT` records are used directly; files without explicit
  connectivity infer bonds from covalent radii.
- `representation=space-filling` creates atom spheres scaled from simple
  element display radii and omits bond cylinders.
- `representation=backbone` emits per-chain protein CA traces and omits atom
  spheres. `backboneMode` selects `none`, `overlay`, `ribbon`, or `tube`, and
  `backboneWidth` controls ribbon / tube width.

`colorScheme` accepts `element`, `chain`, and `residue-category`. Residues with
`HETATM` atoms are categorized as ligands; residues with CA atoms are treated as
polymer residues for backbone generation.

Example:

```sh
rendercli --engine raster --width 256 --height 256 \
  --import_option representation=space-filling \
  --import_option colorScheme=chain \
  test/fixtures/molecules/small.cif molecule.png
```

## Limitations

The importer does not fetch structures from online databases, expand biological
assemblies, apply crystallographic symmetry, select alternate conformations,
draw hydrogen bonds, infer secondary-structure sheets / helices, or model
solvent surfaces. Element styling is a compact built-in table with generic
fallbacks, so it is suitable for visual regression tests and previews rather
than publication-grade chemistry.

The checked-in fixtures under `test/fixtures/molecules/` intentionally stay
small. Unit tests cover PDB and mmCIF parse paths and importer metadata;
rendercli smoke tests render PDB ball-and-stick and mmCIF space-filling scenes
to catch integration regressions that would otherwise produce blank images.

## Source anchors

<!-- source-anchors -->
- `include/core/formats/molecule/Molecule.h`
- `include/core/formats/molecule/MoleculeParser.h`
- `include/world/import/MoleculeSceneImporter.h`
- `include/world/import/MoleculeSceneBuilder.h`
- `src/core/formats/molecule/Molecule.cpp`
- `src/core/formats/molecule/MoleculeParser.cpp`
- `src/world/import/MoleculeSceneImporter.cpp`
- `src/world/import/MoleculeSceneBuilder.cpp`
- `test/fixtures/molecules/`
- `test/rendercli/ImportOptionTest.cmake`
- `test/unit/core/formats/molecule/MoleculeParserTest.cpp`
- `test/unit/world/import/MoleculeSceneImporterTest.cpp`
- `test/unit/world/import/MoleculeSceneBuilderTest.cpp`
<!-- /source-anchors -->

## See also

- Previous: [G-code parsing](gcode-parsing.md)
- Next: [Importer lifecycle](importer-lifecycle.md)
