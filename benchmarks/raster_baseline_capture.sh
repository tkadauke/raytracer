#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rendercli="${RENDERCLI:-${repo_root}/build/release/tools/rendercli/rendercli}"
out_root="${RASTER_BASELINE_OUT:-${repo_root}/tmp/raster-baselines}"
repeat="${RASTER_BASELINE_REPEAT:-5}"
width="${RASTER_BASELINE_WIDTH:-640}"
height="${RASTER_BASELINE_HEIGHT:-480}"
depth_prepass="${RASTER_BASELINE_DEPTH_PREPASS:-off}"

counter_aovs=(
  raster_coverage_count
  raster_depth_test_count
  raster_depth_pass_count
  raster_shade_count
  raster_color_write_count
)

usage() {
  cat <<'USAGE'
Usage: benchmarks/raster_baseline_capture.sh [--aovs] <scene|all>

Scenes:
  materials              benchmarks/scenes/rasterizer_baseline_materials.json
  dense_sphere           benchmarks/scenes/rasterizer_baseline_dense_sphere.json
  offscreen_floor        benchmarks/scenes/rasterizer_baseline_offscreen_floor.json
  alpha_blend_stencil    benchmarks/scenes/rasterizer_baseline_alpha_blend_stencil.json
  dense_ldraw            generated dense curved LDraw MPD fixture
  all                    run every scene above

Environment:
  RENDERCLI              rendercli binary path (default: build/release/tools/rendercli/rendercli)
  RASTER_BASELINE_OUT    output directory (default: tmp/raster-baselines)
  RASTER_BASELINE_REPEAT repeated render count per variant (default: 5)
  RASTER_BASELINE_WIDTH  output width (default: 640)
  RASTER_BASELINE_HEIGHT output height (default: 480)
  RASTER_BASELINE_DEPTH_PREPASS depth prepass mode: off, on, or auto (default: off)

Each scene captures metrics JSON for MSAA 1 and 4 at LOD 0 and LOD 2. Passing
--aovs also exports the five raster counter AOV preview PNGs for each variant.
USAGE
}

generate_dense_ldraw_fixture() {
  local output="$1"
  ruby - "$output" <<'RUBY'
path = ARGV.fetch(0)
segments = 24
rows = 6
cols = 8
radius = 7.0
height = 6.0

File.open(path, "w") do |f|
  f.puts "0 FILE rasterizer_dense_curved_baseline.mpd"
  f.puts "0 Synthesized dense curved LDraw raster benchmark fixture"
  f.puts "0 BFC CERTIFY CCW"
  rows.times do |row|
    cols.times do |col|
      x = (col - (cols - 1) / 2.0) * 22.0
      z = (row - (rows - 1) / 2.0) * 18.0
      color = %w[0x02C91A09 0x020055BF 0x02237841 0x02F2CD37][(row + col) % 4]
      f.puts "1 #{color} #{format('%.3f', x)} 0 #{format('%.3f', z)} 1 0 0 0 1 0 0 0 1 stud_curved.dat"
    end
  end
  f.puts "0 NOFILE"
  f.puts "0 FILE stud_curved.dat"
  f.puts "0 BFC CERTIFY CCW"
  segments.times do |i|
    a0 = (2.0 * Math::PI * i) / segments
    a1 = (2.0 * Math::PI * (i + 1)) / segments
    x0 = Math.cos(a0) * radius
    z0 = Math.sin(a0) * radius
    x1 = Math.cos(a1) * radius
    z1 = Math.sin(a1) * radius
    f.puts format("4 16 %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f",
                  x0, -height / 2.0, z0,
                  x1, -height / 2.0, z1,
                  x1, height / 2.0, z1,
                  x0, height / 2.0, z0)
    f.puts format("3 16 0 %.3f 0 %.3f %.3f %.3f %.3f %.3f %.3f",
                  height / 2.0, x0, height / 2.0, z0, x1, height / 2.0, z1)
  end
  f.puts "0 NOFILE"
end
RUBY
}

scene_path() {
  case "$1" in
    materials) echo "${repo_root}/benchmarks/scenes/rasterizer_baseline_materials.json" ;;
    dense_sphere) echo "${repo_root}/benchmarks/scenes/rasterizer_baseline_dense_sphere.json" ;;
    offscreen_floor) echo "${repo_root}/benchmarks/scenes/rasterizer_baseline_offscreen_floor.json" ;;
    alpha_blend_stencil) echo "${repo_root}/benchmarks/scenes/rasterizer_baseline_alpha_blend_stencil.json" ;;
    dense_ldraw)
      local fixture_dir="${out_root}/fixtures"
      mkdir -p "${fixture_dir}"
      local fixture="${fixture_dir}/rasterizer_dense_curved_baseline.mpd"
      generate_dense_ldraw_fixture "${fixture}"
      echo "${fixture}"
      ;;
    *) return 1 ;;
  esac
}

scene_extra_args() {
  case "$1" in
    alpha_blend_stencil)
      echo "--blend --blend_src source_alpha --blend_dst one_minus_source_alpha --alpha_test --alpha_func greater --alpha_ref 0.20 --render_graph_view stencil_composite"
      ;;
    dense_ldraw)
      echo "--ldraw_input --ldraw_coordinate_conversion ldraw_to_raytracer --ldraw_scale 0.035 --ldraw_normals smooth --ldraw_no_edge_overlays"
      ;;
    *)
      echo ""
      ;;
  esac
}

run_variant() {
  local scene="$1"
  local input="$2"
  local msaa="$3"
  local lod="$4"
  local export_aovs="$5"
  local variant="${scene}_msaa${msaa}_lod${lod}"
  local out_dir="${out_root}/${scene}"
  mkdir -p "${out_dir}"

  local image="${out_dir}/${variant}.png"
  local metrics="${out_dir}/${variant}.metrics.json"
  local stdout="${out_dir}/${variant}.stdout.txt"
  local extra
  extra="$(scene_extra_args "${scene}")"

  local args=(
    "${rendercli}"
    --engine raster
    --width "${width}"
    --height "${height}"
    --lod "${lod}"
    --msaa "${msaa}"
    --repeat "${repeat}"
    --raster_metrics_out "${metrics}"
    --raster_metrics_summary
  )

  if [[ "${depth_prepass}" != "off" ]]; then
    args+=(--depth_prepass "${depth_prepass}")
  fi

  if [[ -n "${extra}" ]]; then
    # shellcheck disable=SC2206
    args+=( ${extra} )
  fi

  if [[ "${export_aovs}" == "1" ]]; then
    for aov in "${counter_aovs[@]}"; do
      args+=(--render_graph_aov_out "${aov}=${out_dir}/${variant}.${aov}.png")
    done
  fi

  args+=("${input}" "${image}")
  echo "capturing ${variant}"
  "${args[@]}" | tee "${stdout}"
}

export_aovs=0
if [[ "${1:-}" == "--aovs" ]]; then
  export_aovs=1
  shift
fi

scene="${1:-}"
if [[ -z "${scene}" || "${scene}" == "-h" || "${scene}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ ! -x "${rendercli}" ]]; then
  echo "rendercli not found or not executable: ${rendercli}" >&2
  echo "Build it with: cmake --preset release && cmake --build --preset release --target rendercli" >&2
  exit 1
fi

if [[ "${scene}" == "all" ]]; then
  scenes=(materials dense_sphere offscreen_floor alpha_blend_stencil dense_ldraw)
else
  scenes=("${scene}")
fi

for item in "${scenes[@]}"; do
  input="$(scene_path "${item}")" || {
    echo "unknown scene: ${item}" >&2
    usage >&2
    exit 1
  }
  for msaa in 1 4; do
    for lod in 0 2; do
      run_variant "${item}" "${input}" "${msaa}" "${lod}" "${export_aovs}"
    done
  done
done
