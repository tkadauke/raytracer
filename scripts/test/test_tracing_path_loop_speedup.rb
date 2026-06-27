$LOAD_PATH.unshift(File.expand_path("..", __dir__))

require "json"
require "minitest/autorun"
require "tempfile"
require "verify_tracing_path_loop_speedup"

class TracingPathLoopSpeedupVerifierTest < Minitest::Test
  def benchmark_row(name, seconds, extra = {})
    {
      "name" => name,
      "run_type" => "iteration",
      "iterations" => 1,
      "real_time" => seconds * 1_000_000_000.0,
      "time_unit" => "ns",
      "tracing_render_seconds" => seconds
    }.merge(extra)
  end

  def cpu_row(paths:, seconds:, camera: true)
    row_name = camera ? "bm_compiledDiffusePathLoopCameraCpuReference"
                      : "bm_compiledDiffusePathLoopCpuReference"
    benchmark_row("#{row_name}/3/#{paths}/4", seconds)
  end

  def gpu_row(paths:, seconds:, supported: 1.0, unavailable: 0.0, final_display: true,
              camera: true)
    row_name = if final_display
                 if camera
                   "bm_requestedGpuCompiledDiffusePathLoopCameraFinalDisplayWarmedSceneUpload"
                 else
                   "bm_requestedGpuCompiledDiffusePathLoopFinalDisplayWarmedSceneUpload"
                 end
               else
                 "bm_requestedGpuCompiledDiffusePathLoopWarmedSceneUpload"
               end
    benchmark_row("#{row_name}/3/#{paths}/4",
                  seconds,
                  "full_gpu_path_loop_supported" => supported,
                  "full_gpu_path_loop_unavailable" => unavailable,
                  "compiled_path_loop_final_display_mode" => final_display ? 1.0 : 0.0,
                  "compiled_path_loop_capture_resolved_display" => final_display ? 1.0 : 0.0,
                  "compiled_path_loop_capture_platform_accumulation" => final_display ? 0.0 : 1.0,
                  "compiled_path_loop_camera_primary_generation" => camera ? 1.0 : 0.0,
                  "compiled_path_loop_primary_paths_materialized" => camera ? 0.0 : paths)
  end

  def verify(rows, **options)
    TracingPathLoopSpeedupVerifier.verify_rows(
      rows,
      TracingPathLoopSpeedupVerifier::Options.new(**options)
    )
  end

  def test_accepts_warmed_full_gpu_row_that_beats_cpu
    result = verify([cpu_row(paths: 65_536, seconds: 2.0),
                     gpu_row(paths: 65_536, seconds: 0.5)],
                    paths: 65_536,
                    min_speedup: 2.0)

    assert result.ok?
    assert_in_delta 4.0, result.best_speedup, 0.0001
    assert_match "bm_requestedGpuCompiledDiffusePathLoopCameraFinalDisplayWarmedSceneUpload",
                 result.row_name
  end

  def test_rejects_gpu_row_that_was_skipped_by_benchmark
    result = verify([cpu_row(paths: 65_536, seconds: 2.0),
                     {
                       "name" => "bm_requestedGpuCompiledDiffusePathLoopCameraFinalDisplayWarmedSceneUpload/3/65536/4",
                       "error_occurred" => true,
                       "error_message" => "MTLCreateSystemDefaultDevice returned nil"
                     }],
                    paths: 65_536)

    refute result.ok?
    assert_match "MTLCreateSystemDefaultDevice returned nil", result.message
  end

  def test_rejects_fast_row_that_did_not_report_full_gpu_support
    result = verify([cpu_row(paths: 65_536, seconds: 2.0),
                     gpu_row(paths: 65_536, seconds: 0.5, supported: 0.0)],
                    paths: 65_536)

    refute result.ok?
    assert_match "full_gpu_path_loop_supported is not 1", result.message
  end

  def test_rejects_diagnostic_gpu_rows_by_default
    result = verify([cpu_row(paths: 65_536, seconds: 2.0, camera: false),
                     gpu_row(paths: 65_536, seconds: 0.5, final_display: false)],
                    paths: 65_536,
                    require_camera: false)

    refute result.ok?
    assert_match "no matching CPU/GPU compiled path-loop rows found", result.message
  end

  def test_can_allow_diagnostic_gpu_rows_explicitly
    result = verify([cpu_row(paths: 65_536, seconds: 2.0, camera: false),
                     gpu_row(paths: 65_536, seconds: 0.5, final_display: false)],
                    paths: 65_536,
                    require_final_display: false,
                    require_camera: false)

    assert result.ok?
    assert_match "bm_requestedGpuCompiledDiffusePathLoopWarmedSceneUpload", result.row_name
  end

  def test_rejects_final_display_row_without_final_display_counters
    broken = gpu_row(paths: 65_536, seconds: 0.5)
    broken["compiled_path_loop_capture_platform_accumulation"] = 1.0
    result = verify([cpu_row(paths: 65_536, seconds: 2.0), broken], paths: 65_536)

    refute result.ok?
    assert_match "compiled_path_loop_capture_platform_accumulation is not 0", result.message
  end

  def test_rejects_synthetic_path_rows_by_default
    result = verify([cpu_row(paths: 65_536, seconds: 2.0, camera: false),
                     gpu_row(paths: 65_536, seconds: 0.5, camera: false)],
                    paths: 65_536)

    refute result.ok?
    assert_match "no matching CPU/GPU compiled path-loop rows found", result.message
  end

  def test_can_allow_synthetic_path_rows_explicitly
    result = verify([cpu_row(paths: 65_536, seconds: 2.0, camera: false),
                     gpu_row(paths: 65_536, seconds: 0.5, camera: false)],
                    paths: 65_536,
                    require_camera: false)

    assert result.ok?
    assert_match "bm_requestedGpuCompiledDiffusePathLoopFinalDisplayWarmedSceneUpload",
                 result.row_name
  end

  def test_rejects_camera_row_with_materialized_primary_paths
    broken = gpu_row(paths: 65_536, seconds: 0.5)
    broken["compiled_path_loop_primary_paths_materialized"] = 65_536
    result = verify([cpu_row(paths: 65_536, seconds: 2.0), broken], paths: 65_536)

    refute result.ok?
    assert_match "compiled_path_loop_primary_paths_materialized is not 0", result.message
  end

  def test_can_verify_json_file
    file = Tempfile.new(["path-loop-speedup", ".json"])
    file.write(JSON.generate("benchmarks" => [cpu_row(paths: 65_536, seconds: 2.0),
                                             gpu_row(paths: 65_536, seconds: 1.0)]))
    file.close

    result = TracingPathLoopSpeedupVerifier.verify_file(
      file.path,
      TracingPathLoopSpeedupVerifier::Options.new(paths: 65_536)
    )

    assert result.ok?
  ensure
    file&.close!
  end
end
