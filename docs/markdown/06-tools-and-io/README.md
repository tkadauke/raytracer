# Volume VI — Tools & I/O

The supporting cast around the rendering core. Two chapters; small
and pragmatic. Read them when you need to actually *run* the
codebase, not just understand it.

## Chapters

25. [PLY parsing](25-ply-parsing.md) — ASCII vs binary PLY,
    element / property declarations, why this is the project's only
    untrusted-input surface, and how the LibFuzzer harness keeps it
    honest.
26. [The example apps](26-the-example-apps.md) — tour of `rendercli`
    (headless), `examples/GeneratedRayTracer` (interactive editor),
    `examples/SceneBrowser` (interactive scene picker). How to add
    a built-in scene; how the engine selector wires in.

## Light coverage

Both chapters are deliberately short. PLY parsing is one file plus
the fuzzer; the example apps are mostly Qt wiring on top of
abstractions covered in earlier volumes. The book doesn't try to
walk you through every menu of `GeneratedRayTracer` — Qt has
documentation for that, and the source is small enough to read.

## See also

- Previous: [Volume V — Image processing & computer vision](../05-image-and-vision/README.md)
- [Top-level TOC](../README.md)
- [Appendix](../appendix/a-glossary.md)
