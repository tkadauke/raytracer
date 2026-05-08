# Interactive Widget Modernization Plan

The documentation widgets should feel like one coherent teaching tool, even
when they explain different parts of the renderer. Today they are split across
older `figure.js` canvas widgets, newer slider-based widgets, and custom raw SVG
widgets. This plan standardizes them on an evolved `figure.js` library while
preserving the existing Doxygen embedding model.

## Principles

1. **Direct manipulation for spatial state.** If a point, vertex, edge endpoint,
   ray origin, or other spatial handle moves, the user should drag that visible
   thing directly. Do not hide a spatial update behind a generic slider or
   whole-widget drag gesture.
2. **Sliders for scalar state.** If the input is one-dimensional, use a labeled
   slider or segmented control with a visible value. Do not use "drag anywhere
   horizontally" for scalar parameters.
3. **No hidden gestures.** Every interactive affordance must be visible: point
   handle, slider, segmented control, checkbox, or button.
4. **Scoped styling.** Widget CSS must be scoped to a widget root or library
   canvas class. No global `svg`, `circle`, `line`, `text`, or `rect` rules.
5. **Shared visual language.** Controls, grids, handles, captions, labels, and
   active states should come from the shared widget library unless there is a
   clear one-off reason.
6. **One lifecycle.** Widgets should be state-driven: render from current state,
   update state from controls/handles, then rerender through a shared helper.
7. **US English spelling.** User-facing widget text, documentation, comments,
   tests, and changelog entries should use US English spellings such as
   "color", "behavior", "labeled", and "gray".

## Migration Phases

1. **Foundation.**
   - Scope the legacy `figure.js` CSS so it no longer affects raw SVG widgets.
   - Add reusable widget primitives: root container, SVG canvas helper, segmented
     controls, and draggable point handles.
   - Keep existing `Canvas`, `Vector`, `Slider`, and `DragHandler` APIs working
     during migration.

2. **Reference migration.**
   - Migrate `rasterizer_msaa_coverage.js` first.
   - Replace the edge-position slider with visible draggable triangle vertices.
   - Keep the MSAA sample count as a segmented control.
   - Use this as the visual and code-style reference before touching the rest.

3. **Rasterizer widgets.**
   - Migrate `rasterizer_clip_attributes.js` next by making the outside vertex
     directly draggable.
   - Port `rasterizer_pipeline.js` onto the shared primitives without changing
     its successful interaction model.
   - Review `rasterizer_perspective_uv.js`; keep the depth slider if the state is
     truly scalar, but move shared styling and controls into the library.

4. **Legacy drag cleanup.**
   - Replace whole-widget `DragHandler` usage.
   - Spatial widgets get point/ray handles.
   - Scalar widgets get sliders or segmented controls.

5. **Slider-style unification.**
   - Port tessellation, thin-lens, tilt-shift, and tonemap widgets to the shared
     control styling and lifecycle.

6. **Tests and review surface.**
   - Keep `rake docs:widgets` as the manual bulk review page.
   - Extend JS tests to pin the shared primitives, absence of global CSS leakage,
     and eventually the retirement of `new DragHandler` from production widgets.
