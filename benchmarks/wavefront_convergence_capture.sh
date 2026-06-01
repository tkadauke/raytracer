#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rendercli="${RENDERCLI:-${repo_root}/build/release/tools/rendercli/rendercli}"
image_probe="${RENDERCLI_IMAGE_PROBE:-${repo_root}/build/release/test/rendercli/rendercli_image_probe}"
out_root="${WAVEFRONT_CONVERGENCE_OUT:-${repo_root}/tmp/wavefront-convergence}"
repeat="${WAVEFRONT_CONVERGENCE_REPEAT:-5}"
width="${WAVEFRONT_CONVERGENCE_WIDTH:-160}"
height="${WAVEFRONT_CONVERGENCE_HEIGHT:-120}"
depth="${WAVEFRONT_CONVERGENCE_DEPTH:-5}"
samples="${WAVEFRONT_CONVERGENCE_SAMPLES:-8}"
active_fraction="${WAVEFRONT_CONVERGENCE_ACTIVE_FRACTION:-0.05}"
rms_delta="${WAVEFRONT_CONVERGENCE_RMS_DELTA:-0.002}"
bvh_grid="${WAVEFRONT_CONVERGENCE_BVH_GRID:-9}"

usage() {
  cat <<'USAGE'
Usage: benchmarks/wavefront_convergence_capture.sh <scene|all>

Scenes:
  bvh_whitted          generated BVH-heavy Whitted parity/performance fixture
  pathtracer_bounce    scenes/wavefront_indirect_bounce_demo.json
  all                  run both scenes above

Environment:
  RENDERCLI                             rendercli binary path
  RENDERCLI_IMAGE_PROBE                 rendercli image-probe binary path
  WAVEFRONT_CONVERGENCE_OUT            output directory
  WAVEFRONT_CONVERGENCE_REPEAT         repeated render count per variant
  WAVEFRONT_CONVERGENCE_WIDTH          output width
  WAVEFRONT_CONVERGENCE_HEIGHT         output height
  WAVEFRONT_CONVERGENCE_DEPTH          max ray depth
  WAVEFRONT_CONVERGENCE_SAMPLES        pathtracer samples per pixel
  WAVEFRONT_CONVERGENCE_ACTIVE_FRACTION convergence active-fraction threshold
  WAVEFRONT_CONVERGENCE_RMS_DELTA      convergence RMS-delta threshold
  WAVEFRONT_CONVERGENCE_BVH_GRID       generated sphere grid width/height

The capture writes images, stdout timing summaries, wavefront metrics JSON, and
image-probe comparisons under the output directory. Use it to tune Phase 4
wavefront convergence defaults before changing shipped presets.
USAGE
}

require_tools() {
  if [[ ! -x "${rendercli}" ]]; then
    echo "rendercli not found or not executable: ${rendercli}" >&2
    echo "Build it with: cmake --preset release && cmake --build --preset release --target rendercli" >&2
    exit 1
  fi
  if [[ ! -x "${image_probe}" ]]; then
    echo "rendercli image probe not found or not executable: ${image_probe}" >&2
    echo "Build it with: cmake --build --preset release --target rendercli_image_probe" >&2
    exit 1
  fi
}

generate_bvh_fixture() {
  local output="$1"
  ruby - "$output" "$bvh_grid" <<'RUBY'
path = ARGV.fetch(0)
grid = Integer(ARGV.fetch(1))
spacing = 0.46
radius = 0.18
start = -spacing * (grid - 1) / 2.0

File.open(path, "w") do |f|
  f.puts <<~JSON.chomp
    {
      "id": "{9a000000-0000-0000-0000-000000000000}",
      "name": "Wavefront BVH Convergence Fixture",
      "ambient": [0.42, 0.42, 0.42],
      "background": [0.02, 0.02, 0.03],
      "accelerationMode": 3,
      "type": "Scene",
      "children": [
        {
          "id": "camera",
          "name": "Camera",
          "position": [0.0, 1.0, -5.0],
          "target": [0.0, 0.0, 0.0],
          "distance": 5.0,
          "zoom": 1.25,
          "type": "PinholeCamera",
          "children": []
        },
        {
          "id": "light",
          "name": "Light",
          "color": [1.0, 1.0, 1.0],
          "intensity": 1.0,
          "direction": [-0.35, -1.0, -0.45],
          "type": "DirectionalLight",
          "children": []
        },
        {
          "id": "warm",
          "name": "Warm",
          "color": [0.9, 0.45, 0.18],
          "type": "ConstantColorTexture",
          "children": []
        },
        {
          "id": "matte",
          "name": "Matte",
          "diffuseTexture": "warm",
          "ambientCoefficient": 1.0,
          "diffuseCoefficient": 1.0,
          "type": "MatteMaterial",
          "children": []
        }
  JSON

  index = 0
  grid.times do |row|
    grid.times do |col|
      x = start + col * spacing
      z = start + row * spacing
      f.puts ","
      f.puts <<~JSON.chomp
        {
          "id": "bvh-sphere-#{index}",
          "name": "BVH Sphere #{index}",
          "position": [#{format('%.4f', x)}, 0.0, #{format('%.4f', z)}],
          "material": "matte",
          "radius": #{radius},
          "type": "Sphere",
          "children": []
        }
      JSON
      index += 1
    end
  end

  f.puts "\n  ]\n}"
end
RUBY
}

run_variant() {
  local scene_name="$1"
  local variant="$2"
  local input="$3"
  shift 3

  local out_dir="${out_root}/${scene_name}"
  mkdir -p "${out_dir}"
  local image="${out_dir}/${variant}.png"
  local stdout="${out_dir}/${variant}.stdout.txt"
  local metrics="${out_dir}/${variant}.metrics.json"

  local args=(
    "${rendercli}"
    --width "${width}"
    --height "${height}"
    --depth "${depth}"
    --repeat "${repeat}"
    --timing
  )

  args+=("$@")

  if [[ "${variant}" == wavefront_* ]]; then
    args+=(--wavefront_metrics_out "${metrics}" --wavefront_metrics_summary)
  fi

  args+=("${input}" "${image}")
  echo "capturing ${scene_name}/${variant}"
  "${args[@]}" | tee "${stdout}"
}

compare_variant() {
  local scene_name="$1"
  local reference="$2"
  local candidate="$3"
  local out_dir="${out_root}/${scene_name}"
  local output="${out_dir}/${candidate}.vs-${reference}.compare.txt"
  "${image_probe}" --compare "${out_dir}/${reference}.png" "${out_dir}/${candidate}.png" |
    tee "${output}"
}

capture_bvh_whitted() {
  local fixture_dir="${out_root}/fixtures"
  mkdir -p "${fixture_dir}"
  local fixture="${fixture_dir}/wavefront_bvh_convergence.json"
  generate_bvh_fixture "${fixture}"

  run_variant bvh_whitted raytracer_whitted "${fixture}" \
    --engine raytracer --integrator whitted --samples_per_pixel 1
  run_variant bvh_whitted wavefront_whitted_no_convergence "${fixture}" \
    --engine wavefront --integrator whitted --wavefront_no_convergence --samples_per_pixel 1
  run_variant bvh_whitted wavefront_whitted_convergence "${fixture}" \
    --engine wavefront --integrator whitted --wavefront_convergence \
    --wavefront_convergence_active_fraction "${active_fraction}" \
    --wavefront_convergence_rms_delta "${rms_delta}" --samples_per_pixel 1

  compare_variant bvh_whitted raytracer_whitted wavefront_whitted_no_convergence
  compare_variant bvh_whitted raytracer_whitted wavefront_whitted_convergence
}

capture_pathtracer_bounce() {
  local scene="${repo_root}/scenes/wavefront_indirect_bounce_demo.json"
  run_variant pathtracer_bounce wavefront_pathtracer_no_convergence "${scene}" \
    --engine wavefront --integrator pathtracer --wavefront_no_convergence \
    --samples_per_pixel "${samples}"
  run_variant pathtracer_bounce wavefront_pathtracer_convergence "${scene}" \
    --engine wavefront --integrator pathtracer --wavefront_convergence \
    --wavefront_convergence_active_fraction "${active_fraction}" \
    --wavefront_convergence_rms_delta "${rms_delta}" --samples_per_pixel "${samples}"

  compare_variant pathtracer_bounce wavefront_pathtracer_no_convergence \
    wavefront_pathtracer_convergence
}

scene="${1:-}"
if [[ -z "${scene}" || "${scene}" == "-h" || "${scene}" == "--help" ]]; then
  usage
  exit 0
fi

require_tools

case "${scene}" in
  bvh_whitted)
    capture_bvh_whitted
    ;;
  pathtracer_bounce)
    capture_pathtracer_bounce
    ;;
  all)
    capture_bvh_whitted
    capture_pathtracer_bounce
    ;;
  *)
    echo "unknown scene: ${scene}" >&2
    usage >&2
    exit 1
    ;;
esac
