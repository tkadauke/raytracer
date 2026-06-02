#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

wavefront_constant() {
  local name="$1"
  ruby - "$repo_root/include/core/math/Constants.h" "$name" <<'RUBY'
path = ARGV.fetch(0)
name = ARGV.fetch(1)
pattern = /^\s*inline\s+constexpr\s+double\s+#{Regexp.escape(name)}\s*=\s*([^;]+);/
File.readlines(path).each do |line|
  match = line.match(pattern)
  next unless match
  puts Float(match[1]).to_s
  exit 0
end
warn "constant not found: #{name}"
exit 1
RUBY
}

rendercli="${RENDERCLI:-${repo_root}/build/release/tools/rendercli/rendercli}"
image_probe="${RENDERCLI_IMAGE_PROBE:-${repo_root}/build/release/test/rendercli/rendercli_image_probe}"
out_root="${WAVEFRONT_CONVERGENCE_OUT:-${repo_root}/tmp/wavefront-convergence}"
repeat="${WAVEFRONT_CONVERGENCE_REPEAT:-5}"
width="${WAVEFRONT_CONVERGENCE_WIDTH:-160}"
height="${WAVEFRONT_CONVERGENCE_HEIGHT:-120}"
depth="${WAVEFRONT_CONVERGENCE_DEPTH:-5}"
samples="${WAVEFRONT_CONVERGENCE_SAMPLES:-8}"
active_fraction="${WAVEFRONT_CONVERGENCE_ACTIVE_FRACTION:-}"
if [[ -z "${active_fraction}" ]]; then
  active_fraction="$(wavefront_constant RAYTRACER_WAVEFRONT_ACTIVE_SAMPLE_FRACTION_THRESHOLD)"
fi
rms_delta="${WAVEFRONT_CONVERGENCE_RMS_DELTA:-}"
if [[ -z "${rms_delta}" ]]; then
  rms_delta="$(wavefront_constant RAYTRACER_WAVEFRONT_RADIANCE_DELTA_RMS_THRESHOLD)"
fi
bvh_grid="${WAVEFRONT_CONVERGENCE_BVH_GRID:-9}"
queue_size="${WAVEFRONT_CONVERGENCE_QUEUE_SIZE:-}"
queue_sweep="${WAVEFRONT_CONVERGENCE_QUEUE_SWEEP:-}"
convergence_sweep="${WAVEFRONT_CONVERGENCE_SWEEP:-}"
current_queue_size="${queue_size}"
queue_output_suffix=""

usage() {
  cat <<'USAGE'
Usage: benchmarks/wavefront_convergence_capture.sh <scene|all>

Scenes:
  bvh_whitted          generated BVH-heavy Whitted parity/performance fixture
  reflection_whitted   scenes/reflections.json secondary-ray Whitted fixture
  pathtracer_bounce    scenes/wavefront_indirect_bounce_demo.json
  all                  run all scenes above

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
                                      (default: shipped balanced constant)
  WAVEFRONT_CONVERGENCE_RMS_DELTA      convergence RMS-delta threshold
                                      (default: shipped balanced constant)
  WAVEFRONT_CONVERGENCE_BVH_GRID       generated sphere grid width/height
  WAVEFRONT_CONVERGENCE_QUEUE_SIZE     optional rendercli --queue_size for every variant
  WAVEFRONT_CONVERGENCE_QUEUE_SWEEP    optional comma-separated queue sizes
  WAVEFRONT_CONVERGENCE_SWEEP          optional comma-separated active:rms pairs

The capture writes images, stdout timing summaries, wavefront metrics JSON,
image-probe comparisons, active sample-depth work comparisons, tile
load-balance summaries, frontier hit/miss summaries, packet width summaries,
packet-fill and scalar-tail ratios, packet scalar-fallback reason breakdowns,
and packet-hit refinement material breakdowns under the output directory. Queue
sweeps also write a compact queue_sweep.summary.txt per scene. Use it to tune
Phase 4 wavefront convergence defaults and to baseline Phase 7
scheduler/intersection work before changing shipped presets.

When WAVEFRONT_CONVERGENCE_SWEEP is set, the script reuses the non-converged
baseline and captures one convergence variant per pair, for example:
  WAVEFRONT_CONVERGENCE_SWEEP="0.05:0.002,0.25:0.01,1.0:0.002"

When WAVEFRONT_CONVERGENCE_QUEUE_SWEEP is set, the script runs the selected
scene once per queue size and writes each run under scene/queue_<size>. This
mode ignores WAVEFRONT_CONVERGENCE_QUEUE_SIZE.
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

scene_output_dir() {
  local scene_name="$1"
  echo "${out_root}/${scene_name}${queue_output_suffix}"
}

run_variant() {
  local scene_name="$1"
  local variant="$2"
  local input="$3"
  shift 3

  local out_dir
  out_dir="$(scene_output_dir "${scene_name}")"
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
  if [[ -n "${current_queue_size}" ]]; then
    args+=(--queue_size "${current_queue_size}")
  fi

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
  local out_dir
  out_dir="$(scene_output_dir "${scene_name}")"
  local output="${out_dir}/${candidate}.vs-${reference}.compare.txt"
  "${image_probe}" --compare "${out_dir}/${reference}.png" "${out_dir}/${candidate}.png" |
    tee "${output}"
}

compare_wavefront_work() {
  local scene_name="$1"
  local reference="$2"
  local candidate="$3"
  local out_dir
  out_dir="$(scene_output_dir "${scene_name}")"
  local reference_metrics="${out_dir}/${reference}.metrics.json"
  local candidate_metrics="${out_dir}/${candidate}.metrics.json"
  local output="${out_dir}/${candidate}.vs-${reference}.work.txt"

  if [[ ! -f "${reference_metrics}" || ! -f "${candidate_metrics}" ]]; then
    return
  fi

  ruby -rjson - "$reference_metrics" "$candidate_metrics" <<'RUBY' | tee "${output}"
def wavefront_metric_values(path)
  document = JSON.parse(File.read(path))
  values = {
    tile_count: [],
    nonempty_tile_count: [],
    min_nonempty_tile_samples: [],
    average_nonempty_tile_samples: [],
    max_tile_samples: [],
    active_sample_depths: [],
    frontier_hit_rays: [],
    frontier_miss_rays: [],
    frontier_packet_chunks: [],
    frontier_packet_rays: [],
    frontier_ray4_packet_chunks: [],
    frontier_ray8_packet_chunks: [],
    frontier_scalar_rays: [],
    frontier_packet_scalar_fallback_rays: [],
    frontier_packet_scalar_fallback_rays_by_reason: [],
    frontier_packet_refined_rays: [],
    frontier_packet_refined_rays_by_material: [],
    convergence_feedback_depths: [],
    sample_generation_worker_seconds: [],
    sample_stream_worker_seconds: [],
    sample_primary_ray_worker_seconds: [],
    sample_enqueue_worker_seconds: [],
    sample_generation_overhead_worker_seconds: [],
    integrator_batch_worker_seconds: [],
    integrator_intersection_worker_seconds: [],
    integrator_shading_worker_seconds: [],
    integrator_overhead_worker_seconds: [],
    integrator_path_setup_worker_seconds: [],
    integrator_frontier_bookkeeping_worker_seconds: [],
    integrator_progress_snapshot_worker_seconds: [],
    integrator_convergence_test_worker_seconds: [],
    integrator_residual_worker_seconds: []
  }
  document.fetch("runs").each do |run|
    run_values = {
      tile_count: 0.0,
      nonempty_tile_count: 0.0,
      min_nonempty_tile_samples: nil,
      average_nonempty_tile_samples: 0.0,
      max_tile_samples: 0.0,
      active_sample_depths: 0.0,
      frontier_hit_rays: 0.0,
      frontier_miss_rays: 0.0,
      frontier_packet_chunks: 0.0,
      frontier_packet_rays: 0.0,
      frontier_ray4_packet_chunks: 0.0,
      frontier_ray8_packet_chunks: 0.0,
      frontier_scalar_rays: 0.0,
      frontier_packet_scalar_fallback_rays: 0.0,
      frontier_packet_scalar_fallback_rays_by_reason: Hash.new(0.0),
      frontier_packet_refined_rays: 0.0,
      frontier_packet_refined_rays_by_material: Hash.new(0.0),
      convergence_feedback_depths: 0.0,
      sample_generation_worker_seconds: 0.0,
      sample_stream_worker_seconds: 0.0,
      sample_primary_ray_worker_seconds: 0.0,
      sample_enqueue_worker_seconds: 0.0,
      sample_generation_overhead_worker_seconds: 0.0,
      integrator_batch_worker_seconds: 0.0,
      integrator_intersection_worker_seconds: 0.0,
      integrator_shading_worker_seconds: 0.0,
      integrator_overhead_worker_seconds: 0.0,
      integrator_path_setup_worker_seconds: 0.0,
      integrator_frontier_bookkeeping_worker_seconds: 0.0,
      integrator_progress_snapshot_worker_seconds: 0.0,
      integrator_convergence_test_worker_seconds: 0.0,
      integrator_residual_worker_seconds: 0.0
    }
    tilings = []
    batchings = []
    convergences = []
    timings = []
    if run["metrics"]
      tilings << run.dig("metrics", "tiling")
      batchings << run.dig("metrics", "batching")
      convergences << run.dig("metrics", "convergence")
      timings << run.dig("metrics", "timings")
    end
    run.fetch("passes", []).each do |pass|
      tilings << pass.dig("metrics", "tiling")
      batchings << pass.dig("metrics", "batching")
      convergences << pass.dig("metrics", "convergence")
      timings << pass.dig("metrics", "timings")
    end

    weighted_tile_sample_sum = 0.0
    tilings.compact.each do |tiling|
      tile_count = tiling.fetch("tileCount", 0).to_f
      nonempty_tile_count = tiling.fetch("nonEmptyTileCount", 0).to_f
      min_tile_samples = tiling.fetch("minNonEmptyTileSamples", 0).to_f
      max_tile_samples = tiling.fetch("maxTileSamples", 0).to_f
      average_tile_samples = tiling.fetch("averageNonEmptyTileSamples", 0).to_f
      run_values[:tile_count] += tile_count
      run_values[:nonempty_tile_count] += nonempty_tile_count
      if min_tile_samples.positive?
        current_min = run_values[:min_nonempty_tile_samples]
        run_values[:min_nonempty_tile_samples] =
          current_min.nil? ? min_tile_samples : [current_min, min_tile_samples].min
      end
      run_values[:max_tile_samples] = [run_values[:max_tile_samples], max_tile_samples].max
      weighted_tile_sample_sum += average_tile_samples * nonempty_tile_count
    end
    if run_values[:nonempty_tile_count].positive?
      run_values[:average_nonempty_tile_samples] =
        weighted_tile_sample_sum / run_values[:nonempty_tile_count]
    end

    batchings.compact.each do |batching|
      run_values[:active_sample_depths] += batching.fetch("activeSampleDepthsProcessed", 0).to_f
      run_values[:frontier_hit_rays] +=
        batching.fetch("frontierRayHitsPerDepth", []).sum { |value| value.to_f }
      run_values[:frontier_miss_rays] +=
        batching.fetch("frontierRayMissesPerDepth", []).sum { |value| value.to_f }
      run_values[:frontier_packet_chunks] +=
        batching.fetch("frontierPacketChunksPerDepth", []).sum { |value| value.to_f }
      run_values[:frontier_packet_rays] +=
        batching.fetch("frontierPacketRaysPerDepth", []).sum { |value| value.to_f }
      run_values[:frontier_ray4_packet_chunks] +=
        batching.fetch("frontierRay4PacketChunksPerDepth", []).sum { |value| value.to_f }
      run_values[:frontier_ray8_packet_chunks] +=
        batching.fetch("frontierRay8PacketChunksPerDepth", []).sum { |value| value.to_f }
      run_values[:frontier_scalar_rays] +=
        batching.fetch("frontierScalarRaysPerDepth", []).sum { |value| value.to_f }
      run_values[:frontier_packet_scalar_fallback_rays] +=
        batching.fetch("frontierPacketScalarFallbackRaysPerDepth", []).sum { |value| value.to_f }
      batching.fetch("frontierPacketScalarFallbackRaysByReason", {}).each do |reason, value|
        run_values[:frontier_packet_scalar_fallback_rays_by_reason][reason] += value.to_f
      end
      run_values[:frontier_packet_refined_rays] +=
        batching.fetch("frontierPacketRefinedRaysPerDepth", []).sum { |value| value.to_f }
      batching.fetch("frontierPacketRefinedRaysByMaterial", {}).each do |material, value|
        run_values[:frontier_packet_refined_rays_by_material][material] += value.to_f
      end
    end
    convergences.compact.each do |convergence|
      run_values[:convergence_feedback_depths] += convergence.fetch("feedbackDepthCount", 0).to_f
    end
    timings.compact.each do |timing|
      run_values[:sample_generation_worker_seconds] +=
        timing.fetch("sampleGenerationWorkerSeconds", 0).to_f
      run_values[:sample_stream_worker_seconds] +=
        timing.fetch("sampleStreamWorkerSeconds", 0).to_f
      run_values[:sample_primary_ray_worker_seconds] +=
        timing.fetch("primaryRayWorkerSeconds", 0).to_f
      run_values[:sample_enqueue_worker_seconds] +=
        timing.fetch("sampleEnqueueWorkerSeconds", 0).to_f
      run_values[:sample_generation_overhead_worker_seconds] +=
        timing.fetch("sampleGenerationOverheadWorkerSeconds", 0).to_f
      run_values[:integrator_batch_worker_seconds] +=
        timing.fetch("integratorBatchWorkerSeconds", 0).to_f
      run_values[:integrator_intersection_worker_seconds] +=
        timing.fetch("integratorIntersectionWorkerSeconds", 0).to_f
      run_values[:integrator_shading_worker_seconds] +=
        timing.fetch("integratorShadingWorkerSeconds", 0).to_f
      run_values[:integrator_overhead_worker_seconds] +=
        timing.fetch("integratorOverheadWorkerSeconds", 0).to_f
      run_values[:integrator_path_setup_worker_seconds] +=
        timing.fetch("integratorPathSetupWorkerSeconds", 0).to_f
      run_values[:integrator_frontier_bookkeeping_worker_seconds] +=
        timing.fetch("integratorFrontierBookkeepingWorkerSeconds", 0).to_f
      run_values[:integrator_progress_snapshot_worker_seconds] +=
        timing.fetch("integratorProgressSnapshotWorkerSeconds", 0).to_f
      run_values[:integrator_convergence_test_worker_seconds] +=
        timing.fetch("integratorConvergenceTestWorkerSeconds", 0).to_f
      run_values[:integrator_residual_worker_seconds] +=
        timing.fetch("integratorResidualWorkerSeconds", 0).to_f
    end
    next if tilings.compact.empty? && batchings.compact.empty? && convergences.compact.empty? &&
            timings.compact.empty?

    run_values.each do |key, value|
      values[key] << (value || 0.0)
    end
  end
  raise "no batching metrics in #{path}" if values[:active_sample_depths].empty?

  values
end

def median(values)
  sorted = values.sort
  sorted[sorted.length / 2]
end

def median_map_values(values)
  keys = values.flat_map(&:keys).uniq.sort
  keys.to_h do |key|
    [key, median(values.map { |map| map.fetch(key, 0.0) })]
  end
end

reference_values = wavefront_metric_values(ARGV.fetch(0))
candidate_values = wavefront_metric_values(ARGV.fetch(1))

reference = median(reference_values[:active_sample_depths])
candidate = median(candidate_values[:active_sample_depths])
saved = reference - candidate
fraction = reference.zero? ? 0.0 : saved / reference
puts format("active_sample_depths reference=%.0f candidate=%.0f saved=%.0f saved_fraction=%.6f",
            reference, candidate, saved, fraction)

%i[tile_count
   nonempty_tile_count
   min_nonempty_tile_samples
   average_nonempty_tile_samples
   max_tile_samples].each do |key|
  reference = median(reference_values[key])
  candidate = median(candidate_values[key])
  delta = candidate - reference
  puts format("%s reference=%.3f candidate=%.3f delta=%.3f",
              key, reference, candidate, delta)
end

%i[frontier_hit_rays
   frontier_miss_rays
   frontier_packet_chunks
   frontier_packet_rays
   frontier_ray4_packet_chunks
   frontier_ray8_packet_chunks
   frontier_scalar_rays
   frontier_packet_scalar_fallback_rays
   frontier_packet_refined_rays].each do |key|
  reference = median(reference_values[key])
  candidate = median(candidate_values[key])
  delta = candidate - reference
  puts format("%s reference=%.0f candidate=%.0f delta=%.0f",
              key, reference, candidate, delta)
end

reference_by_reason = median_map_values(reference_values[:frontier_packet_scalar_fallback_rays_by_reason])
candidate_by_reason = median_map_values(candidate_values[:frontier_packet_scalar_fallback_rays_by_reason])
(reference_by_reason.keys + candidate_by_reason.keys).uniq.sort.each do |reason|
  reference = reference_by_reason.fetch(reason, 0.0)
  candidate = candidate_by_reason.fetch(reason, 0.0)
  delta = candidate - reference
  puts format("frontier_packet_scalar_fallback_rays_by_reason reason=%s reference=%.0f candidate=%.0f delta=%.0f",
              reason, reference, candidate, delta)
end

reference_by_material = median_map_values(reference_values[:frontier_packet_refined_rays_by_material])
candidate_by_material = median_map_values(candidate_values[:frontier_packet_refined_rays_by_material])
(reference_by_material.keys + candidate_by_material.keys).uniq.sort.each do |material|
  reference = reference_by_material.fetch(material, 0.0)
  candidate = candidate_by_material.fetch(material, 0.0)
  delta = candidate - reference
  puts format("frontier_packet_refined_rays_by_material material=%s reference=%.0f candidate=%.0f delta=%.0f",
              material, reference, candidate, delta)
end

reference = median(reference_values[:convergence_feedback_depths])
candidate = median(candidate_values[:convergence_feedback_depths])
puts format("convergence_feedback_depths reference=%.0f candidate=%.0f delta=%.0f",
            reference, candidate, candidate - reference)

%i[sample_generation_worker_seconds
   sample_stream_worker_seconds
   sample_primary_ray_worker_seconds
   sample_enqueue_worker_seconds
   sample_generation_overhead_worker_seconds
   integrator_batch_worker_seconds
   integrator_intersection_worker_seconds
   integrator_shading_worker_seconds
   integrator_overhead_worker_seconds
   integrator_path_setup_worker_seconds
   integrator_frontier_bookkeeping_worker_seconds
   integrator_progress_snapshot_worker_seconds
   integrator_convergence_test_worker_seconds
   integrator_residual_worker_seconds].each do |key|
  reference = median(reference_values[key]) * 1000.0
  candidate = median(candidate_values[key]) * 1000.0
  delta = candidate - reference
  label = key.to_s.sub(/_seconds\z/, "_ms")
  puts format("%s reference=%.3f candidate=%.3f delta=%.3f",
              label, reference, candidate, delta)
end
RUBY
}

write_queue_sweep_summary() {
  local scene_name="$1"
  local scene_dir="${out_root}/${scene_name}"
  local output="${scene_dir}/queue_sweep.summary.txt"
  if [[ -z "${queue_sweep}" || ! -d "${scene_dir}" ]]; then
    return
  fi

  ruby -rjson - "$scene_dir" <<'RUBY' | tee "${output}"
def median(values)
  sorted = values.sort
  sorted[sorted.length / 2]
end

def sum_array(object, key)
  object.fetch(key, []).sum { |value| value.to_f }
end

def metric_objects_for(run)
  objects = []
  objects << run["metrics"] if run["metrics"]
  run.fetch("passes", []).each do |pass|
    objects << pass["metrics"] if pass["metrics"]
  end
  objects
end

def aggregate_run(run)
  values = {
    primary_samples: 0.0,
    tile_count: 0.0,
    nonempty_tile_count: 0.0,
    average_tile_samples: 0.0,
    max_tile_samples: 0.0,
    ray8_chunks: 0.0,
    ray4_chunks: 0.0,
    packet_rays: 0.0,
    scalar_rays: 0.0,
    fallback_rays: 0.0,
    sample_generation_ms: 0.0,
    integrator_ms: 0.0,
    residual_ms: 0.0
  }
  weighted_tile_sample_sum = 0.0
  metric_objects_for(run).each do |metrics|
    input = metrics.fetch("input", {})
    tiling = metrics.fetch("tiling", {})
    batching = metrics.fetch("batching", {})
    timings = metrics.fetch("timings", {})
    primary_samples = input.fetch("primarySamples", 0).to_f
    nonempty_tile_count = tiling.fetch("nonEmptyTileCount", 0).to_f
    average_tile_samples = tiling.fetch("averageNonEmptyTileSamples", 0).to_f

    values[:primary_samples] += primary_samples
    values[:tile_count] += tiling.fetch("tileCount", 0).to_f
    values[:nonempty_tile_count] += nonempty_tile_count
    weighted_tile_sample_sum += average_tile_samples * nonempty_tile_count
    values[:max_tile_samples] = [values[:max_tile_samples],
                                 tiling.fetch("maxTileSamples", 0).to_f].max
    values[:ray8_chunks] += sum_array(batching, "frontierRay8PacketChunksPerDepth")
    values[:ray4_chunks] += sum_array(batching, "frontierRay4PacketChunksPerDepth")
    values[:packet_rays] += sum_array(batching, "frontierPacketRaysPerDepth")
    values[:scalar_rays] += sum_array(batching, "frontierScalarRaysPerDepth")
    values[:fallback_rays] += sum_array(batching, "frontierPacketScalarFallbackRaysPerDepth")
    values[:sample_generation_ms] +=
      timings.fetch("sampleGenerationWorkerSeconds", 0).to_f * 1000.0
    values[:integrator_ms] += timings.fetch("integratorBatchWorkerSeconds", 0).to_f * 1000.0
    values[:residual_ms] += timings.fetch("integratorResidualWorkerSeconds", 0).to_f * 1000.0
  end
  if values[:nonempty_tile_count].positive?
    values[:average_tile_samples] = weighted_tile_sample_sum / values[:nonempty_tile_count]
  end
  values
end

def render_median_ms(stdout_path)
  return 0.0 unless File.exist?(stdout_path)

  File.readlines(stdout_path).reverse_each do |line|
    match = line.match(/render_ms\s+.*\bmedian=([0-9.]+)/)
    return match[1].to_f if match
  end
  0.0
end

scene_dir = ARGV.fetch(0)
queue_dirs = Dir.glob(File.join(scene_dir, "queue_*")).select { |path| File.directory?(path) }
queue_dirs.sort_by! { |path| File.basename(path).delete_prefix("queue_").to_i }

puts "queue_size variant render_ms primary_samples tile_count avg_tile_samples max_tile_samples ray8_chunks ray4_chunks packet_fill scalar_tail_fraction fallback_fraction scalar_rays fallback_rays sample_generation_worker_ms integrator_worker_ms integrator_residual_worker_ms"
queue_dirs.each do |queue_dir|
  queue_size = File.basename(queue_dir).delete_prefix("queue_")
  Dir.glob(File.join(queue_dir, "wavefront_*.metrics.json")).sort.each do |metrics_path|
    variant = File.basename(metrics_path, ".metrics.json")
    document = JSON.parse(File.read(metrics_path))
    runs = document.fetch("runs").map { |run| aggregate_run(run) }
    next if runs.empty?

    median_for = lambda { |key| median(runs.map { |run| run.fetch(key) }) }
    ray8_chunks = median_for.call(:ray8_chunks)
    ray4_chunks = median_for.call(:ray4_chunks)
    packet_rays = median_for.call(:packet_rays)
    scalar_rays = median_for.call(:scalar_rays)
    fallback_rays = median_for.call(:fallback_rays)
    packet_capacity = ray8_chunks * 8.0 + ray4_chunks * 4.0
    packet_fill = packet_capacity.zero? ? 0.0 : packet_rays / packet_capacity
    frontier_rays = packet_rays + scalar_rays
    scalar_tail_fraction = frontier_rays.zero? ? 0.0 : scalar_rays / frontier_rays
    fallback_fraction = packet_rays.zero? ? 0.0 : fallback_rays / packet_rays
    stdout_path = File.join(queue_dir, "#{variant}.stdout.txt")
    puts format(
      "%s %s %.3f %.0f %.0f %.3f %.0f %.0f %.0f %.6f %.6f %.6f %.0f %.0f %.3f %.3f %.3f",
      queue_size,
      variant,
      render_median_ms(stdout_path),
      median_for.call(:primary_samples),
      median_for.call(:tile_count),
      median_for.call(:average_tile_samples),
      median_for.call(:max_tile_samples),
      ray8_chunks,
      ray4_chunks,
      packet_fill,
      scalar_tail_fraction,
      fallback_fraction,
      scalar_rays,
      fallback_rays,
      median_for.call(:sample_generation_ms),
      median_for.call(:integrator_ms),
      median_for.call(:residual_ms)
    )
  end
end
RUBY
}

safe_suffix_number() {
  local value="$1"
  value="${value//./p}"
  value="${value//-/m}"
  value="${value//+/p}"
  echo "${value}"
}

require_positive_integer() {
  local value="$1"
  local label="$2"
  if [[ ! "${value}" =~ ^[1-9][0-9]*$ ]]; then
    echo "${label} must be a positive integer: ${value}" >&2
    exit 1
  fi
}

queue_specs() {
  if [[ -z "${queue_sweep}" ]]; then
    if [[ -n "${queue_size}" ]]; then
      require_positive_integer "${queue_size}" "WAVEFRONT_CONVERGENCE_QUEUE_SIZE"
    fi
    printf '%s=%s\n' "" "${queue_size}"
    return
  fi

  local specs
  IFS=',' read -r -a specs <<<"${queue_sweep}"
  for spec in "${specs[@]}"; do
    if [[ -z "${spec}" ]]; then
      echo "invalid WAVEFRONT_CONVERGENCE_QUEUE_SWEEP entry: ${queue_sweep}" >&2
      exit 1
    fi
    require_positive_integer "${spec}" "WAVEFRONT_CONVERGENCE_QUEUE_SWEEP"
    printf '%s=%s\n' "/queue_$(safe_suffix_number "${spec}")" "${spec}"
  done
}

convergence_specs() {
  if [[ -z "${convergence_sweep}" ]]; then
    printf '%s\t%s\t%s\n' "convergence" "${active_fraction}" "${rms_delta}"
    return
  fi

  local specs
  IFS=',' read -r -a specs <<<"${convergence_sweep}"
  for spec in "${specs[@]}"; do
    local fraction threshold suffix
    IFS=':' read -r fraction threshold <<<"${spec}"
    if [[ -z "${fraction}" || -z "${threshold}" ]]; then
      echo "invalid WAVEFRONT_CONVERGENCE_SWEEP entry: ${spec}" >&2
      exit 1
    fi
    suffix="convergence_af$(safe_suffix_number "${fraction}")_rms$(safe_suffix_number "${threshold}")"
    printf '%s\t%s\t%s\n' "${suffix}" "${fraction}" "${threshold}"
  done
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

  compare_variant bvh_whitted raytracer_whitted wavefront_whitted_no_convergence
  while IFS=$'\t' read -r suffix fraction threshold; do
    local variant="wavefront_whitted_${suffix}"
    run_variant bvh_whitted "${variant}" "${fixture}" \
      --engine wavefront --integrator whitted --wavefront_convergence \
      --wavefront_convergence_active_fraction "${fraction}" \
      --wavefront_convergence_rms_delta "${threshold}" --samples_per_pixel 1
    compare_variant bvh_whitted raytracer_whitted "${variant}"
    compare_wavefront_work bvh_whitted wavefront_whitted_no_convergence "${variant}"
  done < <(convergence_specs)
}

capture_reflection_whitted() {
  local scene="${repo_root}/scenes/reflections.json"
  run_variant reflection_whitted raytracer_whitted "${scene}" \
    --engine raytracer --integrator whitted --samples_per_pixel 1
  run_variant reflection_whitted wavefront_whitted_no_convergence "${scene}" \
    --engine wavefront --integrator whitted --wavefront_no_convergence --samples_per_pixel 1

  compare_variant reflection_whitted raytracer_whitted wavefront_whitted_no_convergence
  while IFS=$'\t' read -r suffix fraction threshold; do
    local variant="wavefront_whitted_${suffix}"
    run_variant reflection_whitted "${variant}" "${scene}" \
      --engine wavefront --integrator whitted --wavefront_convergence \
      --wavefront_convergence_active_fraction "${fraction}" \
      --wavefront_convergence_rms_delta "${threshold}" --samples_per_pixel 1
    compare_variant reflection_whitted raytracer_whitted "${variant}"
    compare_wavefront_work reflection_whitted wavefront_whitted_no_convergence "${variant}"
  done < <(convergence_specs)
}

capture_pathtracer_bounce() {
  local scene="${repo_root}/scenes/wavefront_indirect_bounce_demo.json"
  run_variant pathtracer_bounce wavefront_pathtracer_no_convergence "${scene}" \
    --engine wavefront --integrator pathtracer --wavefront_no_convergence \
    --samples_per_pixel "${samples}"

  while IFS=$'\t' read -r suffix fraction threshold; do
    local variant="wavefront_pathtracer_${suffix}"
    run_variant pathtracer_bounce "${variant}" "${scene}" \
      --engine wavefront --integrator pathtracer --wavefront_convergence \
      --wavefront_convergence_active_fraction "${fraction}" \
      --wavefront_convergence_rms_delta "${threshold}" --samples_per_pixel "${samples}"
    compare_variant pathtracer_bounce wavefront_pathtracer_no_convergence "${variant}"
    compare_wavefront_work pathtracer_bounce wavefront_pathtracer_no_convergence "${variant}"
  done < <(convergence_specs)
}

selected_scene_names() {
  case "${scene}" in
    bvh_whitted)
      echo "bvh_whitted"
      ;;
    reflection_whitted)
      echo "reflection_whitted"
      ;;
    pathtracer_bounce)
      echo "pathtracer_bounce"
      ;;
    all)
      printf '%s\n' "bvh_whitted" "reflection_whitted" "pathtracer_bounce"
      ;;
  esac
}

scene="${1:-}"
if [[ -z "${scene}" || "${scene}" == "-h" || "${scene}" == "--help" ]]; then
  usage
  exit 0
fi

require_tools

queue_spec_lines="$(queue_specs)"
while IFS='=' read -r queue_suffix queue_value; do
  queue_output_suffix="${queue_suffix}"
  current_queue_size="${queue_value}"
  if [[ -n "${queue_suffix}" ]]; then
    echo "queue sweep: queue_size=${current_queue_size}"
  fi

  case "${scene}" in
    bvh_whitted)
      capture_bvh_whitted
      ;;
    reflection_whitted)
      capture_reflection_whitted
      ;;
    pathtracer_bounce)
      capture_pathtracer_bounce
      ;;
    all)
      capture_bvh_whitted
      capture_reflection_whitted
      capture_pathtracer_bounce
      ;;
    *)
      echo "unknown scene: ${scene}" >&2
      usage >&2
      exit 1
      ;;
  esac
done <<<"${queue_spec_lines}"

if [[ -n "${queue_sweep}" ]]; then
  while read -r summary_scene; do
    write_queue_sweep_summary "${summary_scene}"
  done < <(selected_scene_names)
fi
