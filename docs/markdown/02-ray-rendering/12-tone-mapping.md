# 12. Tone mapping

The renderer accumulates floating-point colors that can grow
arbitrarily bright. The display can show only the values
$[0, 1]$ per channel. **Tone mapping** is the conversion that
turns the first into the second.

This chapter is short because the topic is bounded — three
operators, one transfer function each, and a contract that says
they bracket each other in a specific order. By the end of this
chapter you should know:

- the role of the float framebuffer in keeping [HDR](../appendix/a-glossary.md#h) data alive
  through the entire render,
- the three tonemap operators the codebase ships, with their
  transfer functions written out,
- the ordering invariant that the functional test pins
  (`Linear ≥ ACES ≥ Reinhard`),
- and the gamma-encoding step that has to happen *somewhere*
  between linear render output and sRGB display.

## 12.1 Why HDR exists

A real photograph captures a scene whose dynamic range can span
many orders of magnitude — direct sunlight, lit cloud, sky,
shaded sand, deep shadow. The 8-bit `unsigned int` framebuffer
the display reads has 256 levels per channel; that's roughly
two and a half orders of magnitude before quantization starts
crushing the steps together visibly.

A renderer that wrote directly into 8-bit storage would be
forced to clip the upper end of the range as soon as any
contribution exceeded $1.0$. Three white point lights overlapping
on a single surface would clip; a bright sun would clip; a
mirror reflecting a brightly-lit ball would clip. Once a value
clips, it stays clipped — no later operation can recover the
information that was thrown away.

The codebase avoids this by computing in **HDR**:
floating-point colors with no upper bound. The float framebuffer
[`Buffer<Colord>`](../../../include/core/Buffer.h) accumulates
contributions all the way up to whatever value the math
produces, with no clipping at any intermediate step. The
conversion to 8-bit happens once, at the end of the pipeline,
through a configurable tonemap operator.

This is the data flow from
[chapter 4 §4.6](../01-foundations/04-color-and-buffers.md#4-6-putting-it-together):

```
Buffer<Colord>          ← HDR float framebuffer, all pixels filled
      │
      │  tonemap operator
      ▼
Buffer<unsigned int>    ← LDR display framebuffer
```

The tonemap is the arrow.

## 12.2 The `Tonemap` interface

The base class is
[`render::Tonemap`](../../../include/render/tonemap/Tonemap.h).
Every concrete tonemap implements one method:

```cpp
virtual Colord apply(const Colord& hdr) const = 0;
```

Pure function: input HDR color in, [LDR](../appendix/a-glossary.md#l)-clamped color out.
Stateless. Per-pixel. Trivially parallelizable (the renderer
applies it tile-by-tile during the final pass).

The three concrete subclasses below differ only in the formula
inside `apply`. The factory in
[`TonemapFactory`](../../../include/render/tonemap/TonemapFactory.h)
registers each by name so `rendercli` and `Modeler`
can switch operators from a config file or a dropdown.

## 12.3 Linear (pass-through, hard clamp)

[`LinearTonemap`](../../../include/render/tonemap/LinearTonemap.h)
is the trivial operator. It returns the input unchanged:

```cpp
// include/render/tonemap/LinearTonemap.h:20
inline Colord apply(const Colord& hdr) const override {
  return hdr;
}
```

The clamping happens later, in `Colord::rgb()`'s 8-bit pack
([chapter 4 §4.4](../01-foundations/04-color-and-buffers.md#4-4-hdr-vs-ldr)),
where any channel above $1.0$ becomes $255$ and any channel
below $0.0$ becomes $0$. The "linear" name reflects that the
tonemap stage doesn't reshape the input distribution — every
input value below $1.0$ passes through to its corresponding
8-bit value, and every input above $1.0$ saturates to white.

The visible effect is heavy clipping in any HDR scene. Bright
highlights blow out to pure white; mid-tones look correct; dark
regions look correct. The operator is the right pick for
scenes whose dynamic range *fits* in $[0, 1]$ to begin with —
diagnostic renders, test scenes with carefully-tuned light
intensities, anything where you specifically want the no-op
mapping.

## 12.4 Reinhard (compressed everywhere)

[`ReinhardTonemap`](../../../include/render/tonemap/ReinhardTonemap.h)
applies the canonical [Reinhard](../appendix/a-glossary.md#r) 2002 operator:

$$
y = \frac{x}{1 + x}
$$

per channel. The transfer function is monotonically increasing,
asymptotic to $1$ as $x \to \infty$, and unit-derivative at
$x = 0$ — so values near zero pass through nearly unchanged
while values much greater than $1$ get heavily compressed
toward $1$. There is no saturation or clipping; arbitrarily
bright inputs map to $[0, 1)$.

Plotted: at $x = 0.5$, $y = 0.33$; at $x = 1.0$, $y = 0.50$;
at $x = 5.0$, $y = 0.83$; at $x = 50$, $y = 0.98$.

The visible effect is uniform compression. No region of the
image is over-bright; no region is crushed. Mid-tones look
slightly *darker* than they would in the Linear operator,
because Reinhard maps the input value $1.0$ to $0.5$ instead of
$1.0$. The whole image takes on a slightly washed-out feel
because the contrast curve is the geometric average instead of
the identity. This is the right pick for scenes with extreme
dynamic range that you specifically want to *show* without
clipping anywhere.

## 12.5 ACES (filmic-ish, punchy midtones)

[`AcesTonemap`](../../../include/render/tonemap/AcesTonemap.h)
implements Krzysztof Narkowicz's polynomial fit to the [ACES](../appendix/a-glossary.md#a)
filmic tone curve:

$$
y = \frac{x \, (a x + b)}{x \, (c x + d) + e}
$$

with $a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14$. The
formula is a rational function — a ratio of two quadratics —
and produces an S-shaped curve: dark inputs map to slightly
darker outputs, bright inputs compress toward $1$, and the
midtones get an extra "punch" from the steeper slope around
$x = 0.18$ (the standard photographic mid-gray).

The implementation is one line plus a clamp:

```cpp
// include/render/tonemap/AcesTonemap.h:52
static inline double applyChannel(double x) {
  constexpr double a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
  double y = (x * (a * x + b)) / (x * (c * x + d) + e);
  return std::clamp(y, 0.0, 1.0);
}
```

The clamp is necessary because the rational form can overshoot
$[0, 1]$ for extreme negative inputs (a "negative light"
contribution from numerical edge cases) and slightly overshoot
above $1$ at very high inputs.

The visible effect is punchier midtones and slightly cooler
highlights compared to Reinhard. The "ACES look" is what
modern game engines and rendering pipelines target by default;
it produces images that read as photographic without manual
tweaking.

## 12.6 The cross-operator ordering invariant

The three operators don't agree about how to compress the
midtones, but they *do* agree about one ordering: for the same
HDR input, the maximum displayed channel goes
$\text{Linear} \geq \text{ACES} \geq \text{Reinhard}$. This is
the **monotonicity contract**, and it is pinned by
[`TonemapMonotonicityTest.BuiltInOperatorsHaveMonotoneMaxChannel`](../../../test/functional/render/tonemap/TonemapMonotonicityTest.cpp).

The test renders a white sphere lit by three stacked point
lights — guaranteed to produce over-1.0 HDR contributions — and
asserts:

- Linear's max channel saturates at 255 (it clips, so it must
  reach the 8-bit ceiling).
- ACES's max channel is at most Linear's max (so 255 or below).
- Reinhard's max channel is at most ACES's max (so the same or
  lower) and strictly below 255 (Reinhard never saturates).

The ordering is what makes the operator choice predictable: if
you turn the tonemap from Linear to ACES, the image gets at
most the same brightness in any pixel; if you go from ACES to
Reinhard, brighter still gets darker. This holds per-channel,
so a red highlight that was clipped at $(1.0, 0.5, 0.0)$ in
Linear becomes something like $(0.95, 0.50, 0.00)$ in ACES and
$(0.83, 0.45, 0.00)$ in Reinhard.

The actual images shipped in the doc-render gallery
(`tonemap_linear.png`, `tonemap_reinhard.png`,
`tonemap_aces.png`) make the ordering visible at a glance — the
sky region clips in Linear, compresses in Reinhard, and lands
somewhere in between for ACES.

## 12.7 The gamma encode

There is one concept this chapter is intentionally fuzzy about:
the **gamma encode** that has to happen between the
linear-RGB float values the renderer produces and the sRGB
8-bit values the display expects.

[Chapter 4 §4.3](../01-foundations/04-color-and-buffers.md#4-3-linear-rgb-vs-srgb)
introduces the linear-vs-sRGB distinction and sets the
invariant: "every internal `Colord` is in linear RGB." Tone
mapping happens in linear space — the input to the operator is
linear, the output is linear-but-clamped. Some pipeline step
between the operator's output and the display's input has to
encode that linear value into sRGB.

In this codebase, that step is folded into the simple
LinearTonemap implementation: the `Colord::rgb()` pack at the
buffer boundary is a quantize-and-clamp, *not* a sRGB encode.
The result is that renders viewed on an sRGB display look
slightly darker than the math says they should — the gamma bug
described in chapter 4. A correct renderer would do the gamma
encode after the tonemap and before the 8-bit pack:

$$
v_{\text{srgb}} =
\begin{cases}
12.92 \, v_{\text{linear}} & \text{if } v_{\text{linear}} \leq 0.0031308 \\
1.055 \, v_{\text{linear}}^{1/2.4} - 0.055 & \text{otherwise}
\end{cases}
$$

This is one of the items queued under
`docs/topics-backlog.md` §B (color science) — the codebase
deliberately leans toward "render linear, display linear" until
the broader gamma + color-space work is done correctly. When
that work lands the operator chain will gain a `SrgbTonemap` (or
the equivalent) and the visible output will sit one notch
closer to color-correct.

## 12.8 Picking a tonemap

For test renders that require byte-stable output:
**Linear**. The pass-through arithmetic is the most predictable
and avoids any rational-function rounding sensitivity.

For scenes with high dynamic range that should not clip
anywhere: **Reinhard**. The asymptotic compression toward $1$
keeps every pixel in range without burning anything out.

For final-output renders that should look photographic:
**ACES**. The S-curve produces punchier midtones and
photogenic highlights without manual color grading.

Switching is a single line:

```cpp
raytracer->setTonemap(std::make_shared<AcesTonemap>());
```

or, via the factory:

```cpp
raytracer->setTonemap(TonemapFactory::self().createShared("ACES"));
```

The choice doesn't affect render time meaningfully — the
tonemap pass is one pixel-loop multiplication compared to the
many millions of ray casts that produced the HDR buffer.

## 12.9 Exercises

1. Compute the Reinhard-tonemapped value of the input
   $\text{Colord}(2.0, 1.0, 0.5)$. Compare to the Linear
   tonemapped value of the same input. Which channel is
   compressed the most, and why?
2. The ACES `applyChannel` formula has a clamp at the end. Find
   the input value for which the unclamped formula would return
   exactly $1.0$. What happens above that?
3. The `TonemapMonotonicityTest` pins
   $\text{Linear} \geq \text{ACES} \geq \text{Reinhard}$. Find
   an HDR input — a real `Colord` — for which
   $\text{ACES} > \text{Reinhard}$ is *strict* (not just
   greater-than-or-equal). What property of the input makes the
   ordering strict instead of equal?
4. The chapter notes the missing sRGB-encode step. Predict the
   visible difference between a render with the current
   Linear tonemap and a hypothetical "Linear + sRGB encode"
   tonemap on a 50%-gray surface lit by a single white
   directional light.

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [11. Textures](11-textures.md)
- Next volume:
  [Volume III — Scene structure](../03-scene-structure/README.md)
- Buffer foundation:
  [4. Color and buffers](../01-foundations/04-color-and-buffers.md)
- Linear-vs-sRGB invariant:
  [4. Color and buffers §4.3](../01-foundations/04-color-and-buffers.md#4-3-linear-rgb-vs-srgb)
- Cross-operator monotonicity test:
  [`test/functional/render/tonemap/TonemapMonotonicityTest.cpp`](../../../test/functional/render/tonemap/TonemapMonotonicityTest.cpp)

## Source anchors

<!-- source-anchors -->
- `include/render/tonemap/Tonemap.h`
- `include/render/tonemap/TonemapFactory.h`
- `include/render/tonemap/LinearTonemap.h`
- `include/render/tonemap/ReinhardTonemap.h`
- `include/render/tonemap/AcesTonemap.h`
- `test/functional/render/tonemap/TonemapMonotonicityTest.cpp`
<!-- /source-anchors -->
