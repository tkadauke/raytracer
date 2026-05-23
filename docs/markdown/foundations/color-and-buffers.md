# Color and buffers

A renderer's job is to fill a 2D grid of pixels with colors. This
chapter introduces the two types that hold those: `Color<T>` for a
single color, `Buffer<T>` for the grid. Together they form the
output side of every rendering algorithm in the rest of the book.

By the end you should know:

- how `Colord` is laid out in memory, and how it converts to the
  packed `unsigned int` form that ends up in a display framebuffer,
- the practical difference between **linear RGB** (what the renderer
  works in) and **sRGB** (what the display expects),
- what [HDR](../appendix/a-glossary.md#h) and [LDR](../appendix/a-glossary.md#l) mean, and where the conversion between them
  happens in this codebase,
- how `Buffer<T>` provides a fixed-size 2D array with row-major
  indexing.

## <a id="colord-three-doubles-no-alpha"></a>`Colord`: three doubles, no alpha
The codebase's color type is
[`Color<T>`](../../../include/core/Color.h), templated on the
component scalar. The default instantiation is `Colord =
Color<double>`. There is no alpha channel — every color in the
math pipeline is opaque RGB.

```cpp
// include/core/Color.h
template<class T>
class Color : public InequalityOperator<Color<T>> {
  T m_components[3];
public:
  // ...
};

typedef Color<double> Colord;
```

The component layout is the obvious one: `m_components[0]` is red,
`[1]` is green, `[2]` is blue. Each component lives in `[0, 1]` for
a "displayable" color, but values can legitimately go higher when
the renderer is in HDR mode ([HDR vs LDR](#hdr-vs-ldr)). The class doesn't clamp on
write; it's expected to clamp at the boundary where pixels become
display values.

There's an SSE2 specialization of `Colord` under
[`include/core/color/sse3/`](../../../include/core/color/sse3/),
analogous to the [SSE3](../appendix/a-glossary.md#s) vector specialization from
[Numbers and vectors: The SSE3 specializations](numbers-and-vectors.md#the-sse3-specializations).
The story is the same one: storage occupies an XMM register's
worth, operators use intrinsics, and the API is unchanged.

The arithmetic is component-wise: `a + b`, `a - b`, scalar
`a * 0.5`, color-on-color `a * b` (which is what you want for a
material that absorbs along each channel separately — a red
surface lit by white light multiplies to red, a red surface lit by
green light multiplies to black). Division by zero on any
component throws `DivisionByZeroException`.

## <a id="the-color-model-conversions-widget"></a>The `color_model_conversions` widget
`Colord` also carries CMYK and HSV conversion helpers, mostly for
debugging and color-picking UI. The interactive
`color_model_conversions` widget shows the round-trip between RGB
storage and the helper views; you can drag handles in any color
space and watch the others update.

<!-- widget: color_model_conversions -->

The widget is also where you can convince yourself, hands-on, that
RGB is the only fundamental representation in the codebase: CMYK
and HSV exist as views on top of the same three doubles, computed
on demand by `cInt() / mInt() / yInt() / kInt()` and `h() / s() /
v()` accessors.

## <a id="linear-rgb-vs-srgb"></a>Linear RGB vs sRGB
This is the one piece of color theory the rest of the book
genuinely depends on, so it gets its own section.

When you write `Colord(0.5, 0.5, 0.5)` in scene code, you are
writing a linear-RGB color. "Linear" means: doubling the value
doubles the physical brightness. A pixel of value $0.5$ is exactly
half as bright as a pixel of value $1.0$. The math the renderer
performs — multiplying a surface color by a light intensity,
adding two contributions, integrating over a sampled area — only
works when the inputs and outputs are in this linear space.

The display you're reading this on does *not* show linear RGB. It
shows sRGB. The sRGB transfer function bends the relationship
between digital value and physical brightness: a value of $0.5$
sRGB is roughly $0.21$ of full brightness. The bend exists because
human eyes are more sensitive to dark-tone differences than light-
tone, and sRGB's nonlinear encoding gives more bits to the dark
end where they matter.

A renderer that ignores the distinction produces images that look
"right" only by accident. Mid-grays come out too dark; lit
surfaces have crushed shadows; reflections look posterized in the
mid-tones. This is the classic "gamma bug" that haunted real-time
rendering for a decade.

This codebase is honest: every internal color is linear. The
conversion to sRGB happens once, at the boundary between
floating-point colors and the `unsigned int` framebuffer that gets
displayed. That boundary lives in the tonemap stage
([Tone mapping](../ray-rendering/tone-mapping.md)) — even
the simplest "linear" tonemap performs the gamma encode before
emitting display pixels.

For now, the relevant invariant is:

> **Every `Colord` that appears in scene definitions, materials,
> light intensities, and shading computations is in linear RGB.**

If you read in an image texture that was authored in sRGB, you
have to inverse-gamma-encode it into linear before sampling. If
you write a color out for display, you have to gamma-encode it
back. Skip either step and you've reintroduced the gamma bug.

## <a id="hdr-vs-ldr"></a>HDR vs LDR
HDR — high dynamic range — means a color whose components are not
bounded by 1.0. A bright sun is `Colord(50, 50, 50)`. A direct
filament glow is in the hundreds. A diffuse interior is `Colord(0.4,
0.4, 0.4)`. There's no convention that says they all fit in a
single byte's worth of integer; HDR is what `Colord` carries
naturally, because it's just three doubles with no upper clamp.

LDR — low dynamic range — means colors fit in `[0, 1]` per
component, ready for display. Equivalently, in 8-bit
`unsigned int` form, ready for a framebuffer.

The renderer works in HDR throughout. Only at the very end does it
collapse the HDR float buffer to an LDR `unsigned int` buffer for
display. That collapse is the **tonemap** stage, and the choice of
tonemap operator (Linear / [Reinhard](../appendix/a-glossary.md#r) / [ACES](../appendix/a-glossary.md#a)) is what controls how
brightness gets compressed.

For this chapter, the takeaway is simply: **`Colord` carries HDR
data**. Code that wants to enforce LDR (clamp to `[0, 1]` and
convert to `unsigned int`) calls `Colord::rgb()`:

```cpp
// include/core/Color.h:416
inline unsigned int rgb() const {
  return rInt() << 16 |
         gInt() << 8 |
         bInt();
}
```

`rInt()`, `gInt()`, and `bInt()` clamp to $[0, 255]$ and round.
The bit-pattern packs as `0x00RRGGBB`, the layout the rest of the
codebase uses for `Buffer<unsigned int>` framebuffers.

## <a id="buffer-t-a-fixed-size-2d-array"></a>`Buffer<T>`: a fixed-size 2D array
Pixels need a 2D container. The codebase's answer is
[`Buffer<T>`](../../../include/core/Buffer.h), a thin wrapper over
a heap-allocated `T**`:

```cpp
// include/core/Buffer.h
template<class T>
class Buffer {
public:
  inline explicit Buffer(int width, int height)
    : m_width(width), m_height(height) {
    m_buffer = new T*[height];
    for (int i = 0; i != height; ++i)
      m_buffer[i] = new T[width];
  }

  inline T*& operator[](int index) { return m_buffer[index]; }
  inline int width() const  { return m_width; }
  inline int height() const { return m_height; }

  // ... copy disabled, clear() helper, ...
private:
  int m_width, m_height;
  T** m_buffer;
};
```

The constructor allocates one row pointer per scanline plus one
contiguous run of `T` per row. Indexing is row-major: `buffer[y][x]`
fetches the pixel at row $y$, column $x$. (Note the order — the
*row* index comes first.)

The two instantiations the renderer cares about are:

- `Buffer<Colord>` — the **HDR float framebuffer**. This is the
  buffer the `Raytracer` and `Rasterizer` write into during
  rendering. Each pixel is an HDR `Colord`, so over-1.0 light
  contributions accumulate without clipping.
- `Buffer<unsigned int>` — the **LDR display framebuffer**. This
  is what the GUI displays and what `rendercli` writes to a PNG.
  Each pixel is the packed `0x00RRGGBB` form returned by
  `Colord::rgb()`.

The handoff between them is the tonemap step:
HDR `Buffer<Colord>` → tonemap operator → LDR `Buffer<unsigned
int>` → display.

The choice to use `T**` instead of a contiguous `T*` of size
`width * height` is mildly wasteful — one extra indirection per
pixel access — but it makes `buffer[y][x]` work with no helper
methods and lets the rasterizer think about rows as cheaply
swappable units. On a real hot loop, the compiler often hoists the
row-pointer load out anyway.

The buffer is non-copyable (the copy constructor is `delete`d),
which sidesteps a class of bugs around accidentally cloning
megabyte-scale framebuffers when passing them by value.

## <a id="putting-it-together"></a>Putting it together
A render's data flow, expressed only in the types from this
chapter:

```
Scene + Camera
      │
      │  (The Whitted pipeline)
      ▼
Buffer<Colord>            ← HDR float framebuffer
      │
      │  (Tone mapping)
      ▼
Buffer<unsigned int>      ← LDR display framebuffer
      │
      ▼
display / PNG file
```

That's the whole pipeline at the buffer level. Chapters 5 through
12 expand each of those arrows. Chapters 17 through 21 do the
same for the rasterizer's pipeline, which writes into the same
`Buffer<Colord>` and gets to reuse the same tonemap stage.

## <a id="exercises"></a>Exercises
1. Create a `Buffer<Colord>` of size 200×150, fill every pixel
   with `Colord(1, 0, 0)`, then convert it to a
   `Buffer<unsigned int>`. What value does each pixel hold? Do the
   same with `Colord(0.5, 0, 0)` — is the result `0x008000` or
   `0x00007F00`? Why?
2. Write a function that flips a `Buffer<unsigned int>` vertically.
   How does the cost compare to flipping each row's pixel data
   instead of just swapping row pointers?
3. The codebase uses `unsigned int` for the LDR framebuffer with
   the layout `0x00RRGGBB`. The high byte is unused. What would
   it cost (memory, performance) to use that byte for an alpha
   channel? Would any current consumer notice?
4. Read `Color<T>::fromHSV` in
   [`include/core/Color.h`](../../../include/core/Color.h). Why
   are saturation and value clamped in their accessors but the
   stored RGB representation isn't?

## See also

- Volume index: [Foundations](README.md)
- Previous: [Rays and geometry](rays-and-geometry.md)
- Next: [The Whitted pipeline](../ray-rendering/the-whitted-pipeline.md)
- Picked up by:
  - [Tone mapping](../ray-rendering/tone-mapping.md) —
    the HDR → LDR conversion
  - [Image buffers and pixel formats](../image-and-vision/image-buffers-and-pixel-formats.md)
    — the buffer side, revisited as a CV input

## Source anchors

<!-- source-anchors -->
- `include/core/Color.h`
- `include/core/Buffer.h`
- `include/core/color/sse3/`
<!-- /source-anchors -->
