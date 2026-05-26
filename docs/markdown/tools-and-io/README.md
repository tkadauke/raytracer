# Tools & I/O

The supporting cast around the rendering core. Two chapters; small
and pragmatic. Read them when you need to actually *run* the
codebase, not just understand it.

## Chapters

- [PLY parsing](ply-parsing.md) — ASCII vs binary PLY,
    element / property declarations, why this is the project's only
    untrusted-input surface, and how the LibFuzzer harness keeps it
    honest.
- [LDraw import](ldraw-import.md) — the supported `.dat` / `.ldr` /
    `.mpd` subset, tiny validation fixtures, and how to point
    `rendercli` at an LDraw parts library.
- [STL import](stl-import.md) — ASCII and binary triangle-mesh import,
    binary count validation, and explicit unit/material assumptions.
- [Importer lifecycle](importer-lifecycle.md) — shared scene-importer
    contract, options, sidecar asset resolution, diagnostics, provenance,
    fixtures, and render smoke patterns for new formats.
- [Tools and the Modeler](tools-and-modeler.md) — tour of `rendercli`
    (headless), `src/modeler` (interactive editor), checked-in scene
    JSON, and the engine-selector wiring.

## Light coverage

These chapters are deliberately short. PLY parsing is one file plus
the fuzzer; importer lifecycle is the reusable contract around future
formats; the Modeler is mostly Qt wiring on top of abstractions
covered in earlier volumes. The book doesn't try to walk you through
every menu of `Modeler` — Qt has
documentation for that, and the source is small enough to read.

## See also

- Previous: [Image processing & computer vision](../image-and-vision/README.md)
- Next: [Render graph](../render-graph/README.md)
- [Top-level TOC](../README.md)
- [Appendix](../appendix/a-glossary.md)
