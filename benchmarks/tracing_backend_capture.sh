#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rendercli="${RENDERCLI:-${repo_root}/build/release/tools/rendercli/rendercli}"
out_root="${TRACING_BACKEND_OUT:-${repo_root}/tmp/tracing-backend-benchmarks}"
repeat="${TRACING_BACKEND_REPEAT:-3}"
width="${TRACING_BACKEND_WIDTH:-160}"
height="${TRACING_BACKEND_HEIGHT:-120}"
samples="${TRACING_BACKEND_SAMPLES:-4}"
depth="${TRACING_BACKEND_DEPTH:-3}"
large_mesh_side="${TRACING_BACKEND_LARGE_MESH_SIDE:-28}"

usage() {
  cat <<'USAGE'
Usage: benchmarks/tracing_backend_capture.sh <scene|all>

Scenes:
  small_primitive       supported analytic primitive workload
  large_mesh            generated supported triangle-grid workload
  visibility_heavy      any-hit/direct-light visibility workload
  indirect_diffuse      multi-depth diffuse path-tracing workload
  unsupported_fallback  transparent-material GPU fallback workload
  all                   run every scene above

Environment:
  RENDERCLI                        rendercli binary path
  TRACING_BACKEND_OUT              output directory
  TRACING_BACKEND_REPEAT           repeated render count per mode
  TRACING_BACKEND_WIDTH            output width
  TRACING_BACKEND_HEIGHT           output height
  TRACING_BACKEND_SAMPLES          samples per pixel
  TRACING_BACKEND_DEPTH            max path depth
  TRACING_BACKEND_LARGE_MESH_SIDE  generated mesh grid width/height

Each supported scene captures CPU, auto, and explicit GPU-request modes. The
unsupported fallback scene captures an explicit GPU request so metrics show the
fallback reason and runtime CPU execution path.
USAGE
}

require_rendercli() {
  if [[ ! -x "${rendercli}" ]]; then
    echo "rendercli not found or not executable: ${rendercli}" >&2
    echo "Build it with: cmake --preset release && cmake --build --preset release --target rendercli" >&2
    exit 1
  fi
}

generate_large_mesh_scene() {
  local output="$1"
  ruby - "$output" "$large_mesh_side" <<'RUBY'
path = ARGV.fetch(0)
side = Integer(ARGV.fetch(1))
spacing = 0.22
start = -spacing * side / 2.0

def height(x, z)
  Math.sin(x * 2.7) * 0.18 + Math.cos(z * 1.9) * 0.14
end

File.open(path, "w") do |f|
  f.puts <<~JSON.chomp
    {
      "id": "tracing-backend-large-mesh",
      "name": "Tracing Backend Large Mesh",
      "ambient": [0.05, 0.05, 0.05],
      "background": [0.015, 0.02, 0.03],
      "accelerationMode": 3,
      "type": "Scene",
      "renderIntent": {
        "defaultExecutor": "pathtracer",
        "defaultViewMode": "beauty",
        "defaultCamera": {
          "sceneCameraId": "camera"
        }
      },
      "children": [
        {
          "id": "camera",
          "name": "Camera",
          "position": [0.0, 2.2, -4.8],
          "target": [0.0, 0.0, 1.5],
          "distance": 5.0,
          "zoom": 1.05,
          "type": "PinholeCamera",
          "children": []
        },
        {
          "id": "key-light",
          "name": "Key Light",
          "position": [-2.8, 4.0, -2.5],
          "color": [1.0, 0.96, 0.9],
          "intensity": 1.4,
          "type": "PointLight",
          "children": []
        },
        {
          "id": "mesh-texture",
          "name": "Mesh Texture",
          "color": [0.38, 0.72, 0.95],
          "type": "ConstantColorTexture",
          "children": []
        },
        {
          "id": "mesh-material",
          "name": "Mesh Matte",
          "diffuseTexture": "mesh-texture",
          "ambientCoefficient": 0.0,
          "diffuseCoefficient": 1.0,
          "type": "MatteMaterial",
          "children": []
        },
  JSON

  first = true
  side.times do |x_index|
    side.times do |z_index|
      x0 = start + x_index * spacing
      x1 = x0 + spacing
      z0 = 0.2 + z_index * spacing
      z1 = z0 + spacing
      p00 = [x0, height(x0, z0), z0]
      p10 = [x1, height(x1, z0), z0]
      p01 = [x0, height(x0, z1), z1]
      p11 = [x1, height(x1, z1), z1]
      [[p00, p10, p11], [p00, p11, p01]].each_with_index do |vertices, triangle_index|
        f.puts "," unless first
        first = false
        id = "mesh-triangle-#{x_index}-#{z_index}-#{triangle_index}"
        f.puts <<~JSON.chomp
          {
            "id": "#{id}",
            "name": "#{id}",
            "position": [0.0, 0.0, 0.0],
            "rotation": [0.0, 0.0, 0.0],
            "scale": [1.0, 1.0, 1.0],
            "visible": true,
            "material": "mesh-material",
            "vertexA": [#{vertices[0].map { |v| format('%.6f', v) }.join(', ')}],
            "vertexB": [#{vertices[1].map { |v| format('%.6f', v) }.join(', ')}],
            "vertexC": [#{vertices[2].map { |v| format('%.6f', v) }.join(', ')}],
            "velocity": [0.0, 0.0, 0.0],
            "type": "Triangle",
            "children": []
          }
        JSON
      end
    end
  end

  f.puts
  f.puts "      ]"
  f.puts "    }"
end
RUBY
}

scene_path() {
  case "$1" in
    small_primitive) echo "${repo_root}/test/fixtures/tracing_parity/matte_direct_light.json" ;;
    visibility_heavy) echo "${repo_root}/test/fixtures/tracing_parity/visibility_heavy.json" ;;
    indirect_diffuse) echo "${repo_root}/test/fixtures/tracing_parity/indirect_bounce.json" ;;
    unsupported_fallback) echo "${repo_root}/test/fixtures/tracing_parity/transparent_fallback.json" ;;
    large_mesh)
      local fixture_dir="${out_root}/fixtures"
      mkdir -p "${fixture_dir}"
      local fixture="${fixture_dir}/tracing_backend_large_mesh.json"
      generate_large_mesh_scene "${fixture}"
      echo "${fixture}"
      ;;
    *) return 1 ;;
  esac
}

scene_modes() {
  case "$1" in
    unsupported_fallback) echo "gpu" ;;
    *) echo "cpu auto gpu" ;;
  esac
}

run_mode() {
  local scene="$1"
  local input="$2"
  local backend="$3"
  local out_dir="${out_root}/${scene}"
  mkdir -p "${out_dir}"

  local image="${out_dir}/${scene}_${backend}.png"
  local metrics="${out_dir}/${scene}_${backend}.metrics.json"
  local stdout="${out_dir}/${scene}_${backend}.stdout.txt"

  echo "Capturing ${scene} (${backend})"
  QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}" \
    "${rendercli}" \
      --engine wavefront \
      --integrator pathtracer \
      --width "${width}" \
      --height "${height}" \
      --sampler Halton \
      --samples_per_pixel "${samples}" \
      --sampling_seed 1337 \
      --pathtracer_direct_light_samples 1 \
      --wavefront_denoiser none \
      --wavefront_intersection_backend "${backend}" \
      --depth "${depth}" \
      --repeat "${repeat}" \
      --timing \
      --wavefront_metrics_out "${metrics}" \
      --wavefront_metrics_summary \
      "${input}" "${image}" | tee "${stdout}"
}

run_scene() {
  local scene="$1"
  local input
  input="$(scene_path "${scene}")"
  local modes
  modes="$(scene_modes "${scene}")"
  for backend in ${modes}; do
    run_mode "${scene}" "${input}" "${backend}"
  done
}

main() {
  if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -ne 1 ]]; then
    usage
    exit 0
  fi

  require_rendercli
  case "$1" in
    all)
      for scene in small_primitive large_mesh visibility_heavy indirect_diffuse unsupported_fallback; do
        run_scene "${scene}"
      done
      ;;
    small_primitive|large_mesh|visibility_heavy|indirect_diffuse|unsupported_fallback)
      run_scene "$1"
      ;;
    *)
      usage >&2
      exit 1
      ;;
  esac
}

main "$@"
