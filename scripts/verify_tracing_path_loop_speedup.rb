#!/usr/bin/env ruby

require "json"
require "optparse"

module TracingPathLoopSpeedupVerifier
  Options = Struct.new(:paths, :depth, :min_speedup, :require_warmed, :require_final_display,
                       keyword_init: true)
  Result = Struct.new(:ok?, :message, :best_speedup, :row_name, keyword_init: true)

  CPU_ROW =
    /\Abm_compiledDiffusePathLoopCpuReference\/(?<workload>\d+)\/(?<paths>\d+)\/(?<depth>\d+)\z/
  GPU_ROW =
    /\Abm_requestedGpuCompiledDiffusePathLoop(?<final>FinalDisplay)?(?<warmed>WarmedSceneUpload)?\/(?<workload>\d+)\/(?<paths>\d+)\/(?<depth>\d+)\z/

  module_function

  def verify_file(path, options = Options.new)
    document = JSON.parse(File.read(path))
    verify_rows(document.fetch("benchmarks"), options)
  end

  def verify_rows(rows, options = Options.new)
    options = normalize_options(options)
    cpu_rows = {}
    gpu_rows = {}
    rows.each do |row|
      name = row.fetch("name", "")
      if (match = CPU_ROW.match(name))
        key = row_key(match)
        cpu_rows[key] = row
      elsif (match = GPU_ROW.match(name))
        next if options.require_warmed && match[:warmed].nil?
        next if options.require_final_display && match[:final].nil?

        key = row_key(match)
        gpu_rows[key] = row
      end
    end

    candidates = (cpu_rows.keys & gpu_rows.keys).select do |(_workload, paths, depth)|
      (options.paths.nil? || paths == options.paths) &&
        (options.depth.nil? || depth == options.depth)
    end
    return failure("no matching CPU/GPU compiled path-loop rows found", nil) if candidates.empty?

    failures = []
    best = nil
    candidates.each do |key|
      cpu = cpu_rows.fetch(key)
      gpu = gpu_rows.fetch(key)
      if (reason = row_error(cpu))
        failures << "#{cpu.fetch("name")}: #{reason}"
        next
      end
      if (reason = row_error(gpu))
        failures << "#{gpu.fetch("name")}: #{reason}"
        next
      end
      unless gpu.fetch("full_gpu_path_loop_supported", 0.0).to_f >= 1.0
        failures << "#{gpu.fetch("name")}: full_gpu_path_loop_supported is not 1"
        next
      end
      unless gpu.fetch("full_gpu_path_loop_unavailable", 0.0).to_f <= 0.0
        failures << "#{gpu.fetch("name")}: full_gpu_path_loop_unavailable is not 0"
        next
      end
      if options.require_final_display && (reason = final_display_row_error(gpu))
        failures << "#{gpu.fetch("name")}: #{reason}"
        next
      end

      cpu_seconds = row_seconds(cpu)
      gpu_seconds = row_seconds(gpu)
      if cpu_seconds.nil? || cpu_seconds <= 0.0
        failures << "#{cpu.fetch("name")}: missing positive timing"
        next
      end
      if gpu_seconds.nil? || gpu_seconds <= 0.0
        failures << "#{gpu.fetch("name")}: missing positive timing"
        next
      end

      speedup = cpu_seconds / gpu_seconds
      current = Result.new(ok?: speedup >= options.min_speedup,
                           message: format("%s speedup %.3fx (CPU %.6fs, GPU %.6fs)",
                                           gpu.fetch("name"), speedup, cpu_seconds, gpu_seconds),
                           best_speedup: speedup,
                           row_name: gpu.fetch("name"))
      best = current if best.nil? || current.best_speedup > best.best_speedup
      return current if current.ok?
    end

    if best
      return failure("#{best.message}; required >= #{format("%.3f", options.min_speedup)}x",
                     best)
    end
    failure(failures.join("; "), nil)
  end

  def row_key(match)
    [Integer(match[:workload]), Integer(match[:paths]), Integer(match[:depth])]
  end

  def row_error(row)
    return nil unless row["error_occurred"]

    message = row.fetch("error_message", "benchmark row errored")
    "benchmark row errored: #{message}"
  end

  def final_display_row_error(row)
    unless row.fetch("compiled_path_loop_final_display_mode", 0.0).to_f >= 1.0
      return "compiled_path_loop_final_display_mode is not 1"
    end
    unless row.fetch("compiled_path_loop_capture_resolved_display", 0.0).to_f >= 1.0
      return "compiled_path_loop_capture_resolved_display is not 1"
    end
    unless row.fetch("compiled_path_loop_capture_platform_accumulation", 1.0).to_f <= 0.0
      return "compiled_path_loop_capture_platform_accumulation is not 0"
    end
    nil
  end

  def row_seconds(row)
    return row["tracing_render_seconds"].to_f if row.key?("tracing_render_seconds")

    return nil unless row.key?("real_time")

    case row.fetch("time_unit", "ns")
    when "s"
      row.fetch("real_time").to_f
    when "ms"
      row.fetch("real_time").to_f / 1_000.0
    when "us"
      row.fetch("real_time").to_f / 1_000_000.0
    when "ns"
      row.fetch("real_time").to_f / 1_000_000_000.0
    else
      nil
    end
  end

  def normalize_options(options)
    Options.new(paths: options&.paths,
                depth: options&.depth,
                min_speedup: options&.min_speedup || 1.0,
                require_warmed: options&.require_warmed.nil? ? true : options.require_warmed,
                require_final_display: options&.require_final_display.nil? ? true : options.require_final_display)
  end

  def failure(message, best)
    Result.new(ok?: false,
               message: message.empty? ? "no qualifying full-GPU speedup row found" : message,
               best_speedup: best&.best_speedup,
               row_name: best&.row_name)
  end
end

if $PROGRAM_NAME == __FILE__
  options = TracingPathLoopSpeedupVerifier::Options.new(min_speedup: 1.0,
                                                        require_warmed: true,
                                                        require_final_display: true)
  parser = OptionParser.new do |opts|
    opts.banner = "Usage: scripts/verify_tracing_path_loop_speedup.rb <benchmark-json> [options]"
    opts.on("--paths N", Integer, "Require a specific initial path count") do |value|
      options.paths = value
    end
    opts.on("--depth N", Integer, "Require a specific max depth") do |value|
      options.depth = value
    end
    opts.on("--min-speedup X", Float, "Minimum CPU/GPU speedup, default 1.0") do |value|
      options.min_speedup = value
    end
    opts.on("--allow-cold", "Allow cold requested-GPU rows instead of requiring warmed rows") do
      options.require_warmed = false
    end
    opts.on("--allow-diagnostic-gpu-row",
            "Allow diagnostic accumulation GPU rows instead of requiring final-display rows") do
      options.require_final_display = false
    end
  end
  parser.parse!

  abort parser.to_s if ARGV.size != 1

  result = TracingPathLoopSpeedupVerifier.verify_file(ARGV.fetch(0), options)
  puts result.message
  exit(result.ok? ? 0 : 1)
end
