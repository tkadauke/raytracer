#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/validate_sccache_coverage.sh

Builds the coverage preset from two different absolute checkouts with an
isolated local sccache instance, runs the second checkout's instrumented binary
with GCOV_PREFIX relocation, and verifies that gcovr can read coverage from the
second build tree.

GCC still records the compilation directory in .gcno notes. This validation
therefore asserts the cache-safety boundary that matters for gcovr correctness:
restored notes must not contain stale source-file paths, and cached objects must
write .gcda data into the current checkout via GCOV_PREFIX.

Environment overrides:
  RAYTRACER_SCCACHE_VALIDATION_TARGET   CMake target to build (default: rendercli)
  RAYTRACER_SCCACHE_VALIDATION_JOBS     Build parallelism (default: 2)
  RAYTRACER_SCCACHE_VALIDATION_KEEP     Keep temporary directories when set
USAGE
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

for tool in cmake git gcovr g++ ninja python3 sccache strings tar; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: required tool '$tool' was not found" >&2
    exit 2
  fi
done

repo_root=$(git rev-parse --show-toplevel)
target=${RAYTRACER_SCCACHE_VALIDATION_TARGET:-rendercli}
jobs=${RAYTRACER_SCCACHE_VALIDATION_JOBS:-2}
tmp_root=$(mktemp -d "${TMPDIR:-/tmp}/raytracer-sccache-coverage.XXXXXX")
checkout_a="$tmp_root/workflows/11111/raytracer"
checkout_b="$tmp_root/workflows/22222/raytracer"
sccache_conf="$tmp_root/sccache.toml"
sccache_port=${RAYTRACER_SCCACHE_VALIDATION_PORT:-$((40000 + ($$ % 20000)))}

cleanup() {
  if [[ -z "${RAYTRACER_SCCACHE_VALIDATION_KEEP:-}" ]]; then
    rm -rf "$tmp_root"
  else
    echo "kept validation scratch directory: $tmp_root"
  fi
}
trap cleanup EXIT

mkdir -p "$checkout_a" "$checkout_b" "$tmp_root/cache"
git -C "$repo_root" ls-files -z \
  | tar -C "$repo_root" --null -T - -cf - \
  | tar -x -C "$checkout_a"
git -C "$repo_root" ls-files -z \
  | tar -C "$repo_root" --null -T - -cf - \
  | tar -x -C "$checkout_b"
cat >"$sccache_conf" <<EOF
[cache.disk]
dir = "$tmp_root/cache"
EOF

export SCCACHE_CONF="$sccache_conf"
export SCCACHE_SERVER_PORT="$sccache_port"
export SCCACHE_BASEDIRS="$checkout_a:$checkout_b"

sccache --zero-stats >/dev/null

configure_and_build() {
  local checkout=$1
  cmake -S "$checkout" -B "$checkout/build/coverage" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DRAYTRACER_ENABLE_COVERAGE=ON \
    -DRAYTRACER_BUILD_TESTS=OFF \
    -DRAYTRACER_BUILD_MODELER=OFF \
    -DCMAKE_CXX_COMPILER_LAUNCHER=sccache
  cmake --build "$checkout/build/coverage" --target "$target" --parallel "$jobs"
}

cache_hits() {
  sccache --show-stats --stats-format=json \
    | python3 -c 'import json,sys; print(json.load(sys.stdin)["stats"]["cache_hits"]["counts"].get("C/C++", 0))'
}

configure_and_build "$checkout_a"
hits_after_first=$(cache_hits)

configure_and_build "$checkout_b"
hits_after_second=$(cache_hits)

if (( hits_after_second <= hits_after_first )); then
  echo "error: second checkout did not record any additional C/C++ sccache hits" >&2
  sccache --show-stats --stats-format=json
  exit 1
fi

rendercli="$checkout_b/build/coverage/tools/rendercli/rendercli"
if [[ ! -x "$rendercli" ]]; then
  echo "error: expected validation binary was not built: $rendercli" >&2
  exit 1
fi

gcov_prefix_strip=$(CHECKOUT="$checkout_b" python3 - <<'PY'
from pathlib import Path
import os
print(len(Path(os.environ["CHECKOUT"]).parts) - 1)
PY
)

(
  cd "$checkout_b"
  GCOV_PREFIX="$checkout_b" GCOV_PREFIX_STRIP="$gcov_prefix_strip" \
    "$rendercli" --help >/dev/null
  gcovr --root . \
    --object-directory build/coverage \
    --gcov-ignore-parse-errors=negative_hits.warn_once_per_file \
    --filter '^src/' \
    --filter '^tools/' \
    --exclude 'build/.*' \
    --txt -o "$tmp_root/coverage.txt" \
    --print-summary
)

stale_source_paths=$(find "$checkout_b/build/coverage" -name '*.gcno' -print0 \
  | xargs -0 strings \
  | grep -Ec "$checkout_a/(src|include|tools|test|examples)/" || true)
if (( stale_source_paths > 0 )); then
  echo "error: restored .gcno files contain stale source paths from the first checkout" >&2
  exit 1
fi

echo "sccache C/C++ hits after first build:  $hits_after_first"
echo "sccache C/C++ hits after second build: $hits_after_second"
echo "gcovr report: $tmp_root/coverage.txt"
sccache --show-stats --stats-format=json
