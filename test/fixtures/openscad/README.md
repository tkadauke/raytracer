# OpenSCAD fixtures

Tiny OpenSCAD sources used by importer tests, rendercli smoke tests, and docs.

- `external-compiler/compiler_smoke.scad` is valid OpenSCAD intended for the
  external `openscad` executable path. It combines primitives, transforms, and a
  boolean difference so the compiler adapter has a representative source.
- `native-subset/` contains the hand-authored subset fixtures expected to remain
  readable by the future native importer path: simple primitives, transforms,
  and booleans. Keep these files intentionally small and avoid modules,
  variables, loops, list comprehensions, projection, text, surface imports, or
  `$fn`-driven curved geometry requirements.
