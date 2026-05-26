Importer fixtures keep each source file beside the sidecar assets it would
resolve in production.

Recommended layout:

- `<format>/<case>.<ext>` for the importer source file.
- `<format>/assets/` for textures, material libraries, subfiles, or other
  files referenced from that source.
- `<format>/expected/` for native scene JSON snapshots when the expected output
  is easier to maintain as data than as C++ assertions.

Tests should resolve the source with `test::importers::importerFixturePath()`
and let the importer use `core::AssetResolver` with the source filename as the
current file. That keeps sidecar lookup identical to the shipped importer path.
