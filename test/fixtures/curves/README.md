# Curve Fixture Schema

These fixtures are intentionally small, importer-oriented polyline examples.
They are not a production scene format. Downstream importers can reuse them as
golden inputs when mapping G-code, molecules, trajectories, GPS routes, or
similar path data into `core::Polyline` plus optional segment attributes.

Each fixture has:

- `kind`: currently `polyline`.
- `name`: stable fixture name.
- `points`: ordered `[x, y, z]` coordinates.
- `attributes`: whole-curve metadata for `core::Curve`.
- `segments`: one entry per derived segment, where segment `i` connects
  `points[i]` to `points[i + 1]`.
- `segments[].attributes`: per-segment scalar or categorical values suitable
  for `core::AttributeColorMap`.

`plain_polyline.json` covers point-only path import. `attributed_toolpath.json`
covers scalar feed-rate coloring and categorical move-type coloring while also
looking like route or trajectory data.
