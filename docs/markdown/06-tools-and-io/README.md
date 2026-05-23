# Volume VI — Tools & I/O

The supporting cast around the rendering core. Two chapters; small
and pragmatic. Read them when you need to actually *run* the
codebase, not just understand it.

## Chapters

25. [PLY parsing](25-ply-parsing.md) — ASCII vs binary PLY,
    element / property declarations, why this is the project's only
    untrusted-input surface, and how the LibFuzzer harness keeps it
    honest.
26. [Tools and the Modeler](26-tools-and-modeler.md) — tour of `rendercli`
    (headless), `src/modeler` (interactive editor), checked-in scene
    JSON, and the engine-selector wiring.

## Light coverage

Both chapters are deliberately short. PLY parsing is one file plus
the fuzzer; the Modeler is mostly Qt wiring on top of abstractions
covered in earlier volumes. The book doesn't try to walk you through
every menu of `Modeler` — Qt has
documentation for that, and the source is small enough to read.

## See also

- Previous: [Volume V — Image processing & computer vision](../05-image-and-vision/README.md)
- Next: [Volume VII — Render graph](../07-render-graph/README.md)
- [Top-level TOC](../README.md)
- [Appendix](../appendix/a-glossary.md)
