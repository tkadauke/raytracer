# LDraw fixtures

These hand-authored fixtures keep LDraw importer coverage independent from the
official parts library.

- `smoke/model.mpd` is the render smoke model. It includes inline triangle
  geometry, an MPD-local submodel, an external library part reference, inherited
  color 16, and BFC-certified winding.
- `smoke/library/parts/library-panel.dat` and
  `smoke/library/parts/s/nested-panel.dat` form a tiny library root for
  `--ldraw_library_root`, including nested subfile resolution and edge color 24.
- `nested/` pins transform composition through recursive type-1 references.
- `repeated/` pins compiled subfile cache reuse for repeated references.
- `rendercli/` is the legacy one-part fixture kept for focused CLI option
  coverage.
