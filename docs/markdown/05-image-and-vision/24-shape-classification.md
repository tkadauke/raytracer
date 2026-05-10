# 24. Shape classification

The previous chapter built the descriptor side of the
classical-CV pipeline:
[`Blob`](../../../test/helpers/Blob.h) and
[`Silhouette`](../../../test/helpers/Silhouette.h) extract
geometric properties (radial variance, aspect ratio, area,
perimeter) from a rendered image. This chapter is about the
*decision* side — turning those continuous descriptor values
into discrete shape predicates: *is this a circle? is this a
rectangle?*

The classifier the codebase ships,
[`ShapeClassifier`](../../../test/helpers/ShapeClassifier.h),
is deliberately small. It exposes two predicates, each
implemented as a threshold on one or two descriptors. The
thresholds were chosen empirically by running the classifier
on the test suite's rendered scenes and checking that the
right tests pass and the wrong ones fail. This chapter is
about how those thresholds were chosen, why the predicates
work, and what kinds of rendered shapes break them.

By the end of this chapter you should know:

- the two predicates `ShapeClassifier` exposes and the
  threshold values each one uses,
- why **radial variance** is the right descriptor to drive a
  shape predicate (and aspect ratio is the wrong one alone),
- the cluster-of-shapes plot that motivates the threshold
  choices,
- and the limits of the current classifier — what it gets
  right, what it gets wrong, what happens at the boundaries.

## 24.1 The two predicates

[`ShapeClassifier`](../../../test/helpers/ShapeClassifier.h)
has two methods, each returning a boolean:

```cpp
class ShapeClassifier {
public:
  explicit ShapeClassifier(const Colord& targetColor = Colord(1, 0, 0));

  bool isCircle(const Buffer<unsigned int>& buffer) const;
  bool isRectangle(const Buffer<unsigned int>& buffer) const;

private:
  Colord m_targetColor;
};
```

The `targetColor` defaults to red because the codebase's
standard test material is red. A scene that renders a red
sphere and asks `classifier.isCircle(buffer)` returns `true`;
the same scene rendered through the wireframe engine (which
turns the sphere into a wireframe outline) also returns `true`.

The two predicates are the *only* shape predicates the
codebase ships today. The catalog is intentionally small;
extensions are queued under roadmap §4.11.m for the eventual
larger CV pillar.

## 24.2 Why radial variance is the right descriptor

The defining geometric property of a circle is that **every
point on the boundary is the same distance from the center**.
The `radialVariance` descriptor —
[chapter 23 §23.3](23-blob-analysis-and-silhouettes.md#23-3-what-a-blob-carries) —
measures exactly that: the standard deviation of boundary-
point distance from the centroid, normalized by the mean.

For a perfect circle, every boundary point is at radius $r$,
and the standard deviation is $0$. For a square, boundary
points alternate between *edge midpoint* (distance $r$) and
*corner* (distance $r \sqrt{2}$, about $1.41 \times$ the
edge midpoint). The standard deviation is non-zero, and works
out — for a unit square — to about $0.12$ relative to the
mean.

For other shapes, `radialVariance` produces predictable values:

| Shape | Approximate radial variance |
|---|---|
| Perfect circle | $0$ |
| Slight ellipse (1.1:1) | $\approx 0.05$ |
| Square (axis-aligned) | $\approx 0.12$ |
| Equilateral triangle | $\approx 0.25$ |
| 5:1 elongated rectangle | $\approx 0.55$ |
| Random polygon | very high, varies |

The descriptor is **rotation-invariant** (rotating the shape
doesn't change point-to-centroid distances), **scale-invariant**
(the normalization by the mean cancels out the absolute scale),
and **fill-invariant** (the boundary is the same whether the
interior is filled in or not). That's three invariances at
once, from a single scalar descriptor. This is why
`radialVariance` is the natural choice for a "shape recognizer."

## 24.3 The `isCircle` decision boundary

`isCircle` returns `true` when:

```cpp
silhouette.radialVariance() < 0.10
  AND
silhouette.aspectRatio() in [0.83, 1.20]
```

The predicate combines two thresholds with a logical AND.

The **radial variance threshold** is `< 0.10`. From §24.2,
that band tolerates slight ellipses and pixel-discretization
jitter (a 200-pixel-wide rendered circle has at most a few
pixels of variance from the grid quantization), and rejects
shapes with corners at $\geq 0.12$. The threshold sits below
the square's $0.12$ but above the ellipse's $0.05$ — wide
enough to accept slightly-flattened circles but tight enough
to reject anything cornery.

The **aspect ratio gate** is `[0.83, 1.20]`. Why an additional
gate at all? Because radial variance has a failure mode: a
*vertical bar* (a 1-pixel-wide tall rectangle, say) centered on
its midpoint has every boundary point at almost the same
distance from the centroid (the boundary is two
near-parallel vertical lines), so its radial variance can be
quite low — sometimes below $0.10$. The aspect-ratio gate
catches this: a tall thin rectangle has aspect ratio
substantially different from $1$, so it falls outside
$[0.83, 1.20]$ and the predicate returns `false`.

The bracket $[0.83, 1.20]$ is asymmetric because aspect ratio
is height-over-width — values above $1$ are taller-than-wide,
values below $1$ are wider-than-tall, and the perfect square
(or circle) is exactly $1.0$. The bracket allows for moderate
sub-pixel jitter at the standard test buffer size $200 \times
150$ without rejecting genuine circles.

## 24.4 The `isRectangle` decision boundary

`isRectangle` returns `true` when:

```cpp
silhouette.radialVariance() in [0.10, 0.30]
```

A single descriptor, with both lower and upper thresholds.

The **lower bound** $0.10$ rejects circles, by symmetry with
the `isCircle` threshold above. A shape whose radial variance
is below $0.10$ is a circle (or an ellipse close enough to a
circle); a shape with variance above $0.10$ has visible
corners.

The **upper bound** $0.30$ rejects triangles ($\approx 0.25$
for equilateral) and shapes with even more pronounced corners
or elongation. The choice of $0.30$ is the gap between
"square or rectangle" ($0.12$ for square, up to $0.30$ for
elongated $\approx 5{:}1$ rectangles) and "more dramatic shape"
(triangles, irregular polygons).

The bracket $[0.10, 0.30]$ is therefore the **rectangle band**
in radial-variance space. A square (variance $0.12$) is in
band; a moderately elongated rectangle ($0.20$) is in band; a
$5{:}1$ rectangle ($\approx 0.55$) is *out* of band — at that
elongation the classifier reports "not a rectangle" even
though geometrically it still is. This is a deliberate choice:
beyond a certain elongation, the *visual* shape stops looking
"rectangle-like" to the human eye even if the geometry says it
is. The classifier matches that perceptual judgment rather
than the strict geometric definition.

## 24.5 Pushing the descriptors across the boundary

A static description of decision boundaries doesn't show what
happens when a shape *crosses* one. The figure below — backed
by `scripts/docs/shape_descriptors.js` — has a draggable
polygon on the left, the descriptor readouts on the right
(area, perimeter, bounding box, aspect ratio, radial
variance), the classifier verdict for `isCircle` and
`isRectangle`, and the threshold constants from
[`ShapeClassifier.cpp`](../../../test/helpers/ShapeClassifier.cpp)
so the comparison is concrete:

<!-- widget: shape_descriptors -->

Two illustrative things to try:

1. Start with the default pentagon and drag the vertices
   toward a regular shape — say, by pulling each vertex out
   to the same radius. Watch the radial variance drop through
   the $0.10$ threshold; the `isCircle` verdict flips from
   `false` to `true` at exactly that crossing.
2. Drag the vertices into a tall thin shape. Watch the aspect
   ratio leave the $[0.83, 1.20]$ band. Even with low radial
   variance, `isCircle` stays `false` because the aspect-ratio
   gate catches the elongation.

## 24.6 Why two predicates, not one

A reasonable design alternative is *one* predicate per shape
type — `isCircle`, `isRectangle`, `isTriangle`, `isPentagon`,
… — with the classifier producing the most-likely shape from
the descriptors. The shipped design is two-predicate: the
classifier returns boolean answers to specific questions
rather than picking a single best classification.

The trade-off favors honest answers over forced ones. A torus
side-view is neither a circle nor a rectangle; a test that
asserts "the rendered output is a circle" should fail on a
torus, not pick "rectangle" because that is the next-best
match. With independent boolean predicates, a shape that
fails both predicates produces a clear "neither" instead of a
silent miscategorization.

The same reasoning explains why the predicates are AND-tight
rather than OR-loose. `isCircle` requires *both* low variance
*and* near-1.0 aspect ratio; getting either wrong is enough to
reject. The conservative tightness produces predicate failures
that point at exactly the shape property that is wrong, rather
than rejecting on a vague averaged-out score.

## 24.7 What the classifier doesn't catch

Three classes of failure:

**Overlapping shapes.** Two red circles overlapping in screen
space produce one merged blob. The merged blob's silhouette
has radial variance considerably higher than either individual
circle's, and `isCircle` returns false even though the picture
is "two circles." A test that needs to handle multi-circle
scenes either uses `Blob` directly (counting separate connected
regions) or constrains the scene to one object at a time.

**Concave shapes.** A red star has high radial variance — the
points stick out, the inner notches recede. `isCircle` is
false; `isRectangle` is also false (the variance is well above
$0.30$). The star isn't classified as anything; the predicates
are silent on it. This is fine for the codebase's needs (no
test renders a star), but a classifier intended for general
use would need an `isStar` predicate or a more general
"polygon-with-N-points" classifier.

**Heavy distortion.** A fish-eye-rendered cube has
strongly-distorted edges that look neither circular nor
rectangular. The predicates correctly reject both, but the
test author has to know that the classifier won't *say*
"distorted cube" — it will just say "not a circle and not a
rectangle." The classifier's silence is the answer.

The fix for all three is the same: the classifier is small and
honest. When you need to assert something the classifier
doesn't cover, fall back to one of:

- A `Blob`-based test counting regions or measuring area.
- A `Silhouette`-based test asserting one specific descriptor
  is in some range.
- A pixel-level test (`buffer[y][x] == target_color` at a
  specific position) for tests where the geometric reasoning
  doesn't apply.

## 24.8 What this chapter does *not* cover

The full classical-CV classifier catalog from
`docs/topics-backlog.md` includes much more:

- **Hu invariant moments.** Seven moment-based descriptors
  invariant under rotation, translation, and scale. The
  textbook 1962 method.
- **Curvature-based features.** Tracking the rate of change
  of the boundary tangent — peaks at corners.
- **Fourier descriptors.** Treating the boundary as a closed
  curve and running an FFT; low-frequency coefficients
  summarize gross shape.
- **Convex-hull deviation.** Measuring how much the shape
  differs from its convex hull (zero for convex shapes,
  positive for concave).
- **Learned classifiers.** SVM, random forest, or neural
  network trained on a labeled shape dataset.

None ship today. The two-predicate threshold-based classifier
is sufficient for the synthetic-renderer-output use case the
test suite lives in.

## 24.9 Exercises

1. Predict `radialVariance` and `aspectRatio` for a
   $3 \times 1$ aspect rectangle. Run the values through the
   `isCircle` and `isRectangle` predicates. What happens?
2. The `isCircle` predicate uses both radial variance *and*
   aspect ratio. What goes wrong if you remove the aspect-
   ratio gate? Construct a specific shape that would falsely
   pass.
3. The `isRectangle` predicate has an upper bound at $0.30$
   that rejects elongated rectangles ($\approx 5{:}1$ and
   beyond). Suppose you wanted to support all rectangles
   regardless of elongation. What would you change, and what
   side effect would it have on the classifier's accuracy on
   non-rectangles?
4. Write a hypothetical `isTriangle` predicate. What
   thresholds and what additional descriptors would it need?
   How would it handle the "triangle that happens to be very
   tall and thin" case?

## See also

- Volume index:
  [Volume V — Image processing & computer vision](README.md)
- Previous:
  [23. Blob analysis and silhouettes](23-blob-analysis-and-silhouettes.md)
- Next volume: [Volume VI — Tools & I/O](../06-tools-and-io/README.md)
- Silhouette source:
  [23. Blob analysis and silhouettes §23.4](23-blob-analysis-and-silhouettes.md#23-4-the-silhouette-shortcut)
- Functional tests that consume the classifier:
  `test/functional/render/primitives/SphereTest.cpp` and
  similar (exposed via the `i should see the sphere` Cucumber
  step in the project's GIVEN/WHEN/THEN framework).

## Source anchors

<!-- source-anchors -->
- `test/helpers/ShapeClassifier.h`
- `test/helpers/ShapeClassifier.cpp`
- `test/helpers/Silhouette.h`
<!-- /source-anchors -->
