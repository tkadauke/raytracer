# Parked GitHub Actions workflows

This directory holds the GitHub Actions surface that used to live at
`.github/workflows/` and `.github/dependabot.yml`. It's preserved here
as a plan, not active configuration — these files are NOT picked up by
GitHub from this location.

## Why these are parked

GitHub Actions billing is blocked on this account. Every push produces
a `conclusion: failure` on every check run, which is noise — neither
the repo owner nor any reviewer can tell whether the failure means
"your change is broken" or "GitHub won't run anything until the bill
is paid." Worse, in Syrus-driven workflows, the same false failure
signal can trigger fix-attempt iterations that consume agent budget
rediscovering there's nothing to fix.

The pragmatic choice was to delete the active workflows and run the
quality gates inside Syrus instead. Per-iteration gates live in
`.syrus.yml`; everything else is parked here until either GitHub
billing is restored or Syrus grows the missing primitives that would
let those workflows migrate too.

## What moved where

| Old location | Status | Notes |
|---|---|---|
| `.github/workflows/ci.yml` | Replaced (partially) by `.syrus.yml` | `build-test`, `benchmark-build`, and `textbook` graders run in Syrus — see "What lives in Syrus" below. The sanitize/fuzz/coverage/lint/docker/sbom jobs are awaiting Syrus's post-grade primitive. |
| `.github/workflows/codeql.yml` | Parked | CodeQL uploads SARIF to GitHub's Security tab, which is a GitHub-only sink. No Syrus equivalent. |
| `.github/workflows/docs.yml` | Parked | Doxygen + textbook publish to GitHub Pages. Pages deploy is a GitHub-only primitive. |
| `.github/workflows/mutation.yml` | Parked | Monthly `mull-runner` against the math module. Cron-triggered; Syrus has no cron equivalent. |
| `.github/workflows/release.yml` | Parked | Tag-driven SBOM + Sigstore cosign + GitHub release. Needs GitHub OIDC + write access to the release API; not portable to Syrus. |
| `.github/dependabot.yml` | Parked | Dependabot is a GitHub-only service. Doesn't apply to a non-GitHub-CI repo. |

## What lives in Syrus today

Three graders run in the implement → grade iteration loop:

1. **`build-test`** — `cmake --preset release && cmake --build --preset release --parallel && QT_QPA_PLATFORM=offscreen ctest --preset release --parallel $(nproc) --output-on-failure`. Catches compile and correctness regressions in one pass. Single compiler (g++-12); incremental rebuilds across iterations.
2. **`benchmark-build`** — `cmake --preset benchmark && cmake --build --preset benchmark --target benchmarks --parallel && ./build/benchmark/benchmarks/benchmarks --benchmark_list_tests=true`. Catches compile/link errors and benchmark registration drift for sources excluded from the default release build, without running timing-sensitive measurements.
3. **`textbook`** — `rake docs:textbook:check && rake docs:textbook:source-map && git diff --exit-code -- docs/markdown/appendix/c-source-map.md`. Catches markdown drift and stale generated source-map appendix.

See `.syrus.yml` at the repo root for the full configuration.

## What's missing (and what unblocks bringing it back)

| Gate | Blocker | Trigger to revisit |
|---|---|---|
| Sanitize (ASan + UBSan), Fuzz (60s libfuzzer smoke), Lint (clang-format + clang-tidy advisory), Coverage (gcovr with 60% floor), SBOM (per-commit anchore/sbom-action) | Each adds significant per-iteration cost; running them on every implement→grade pass would balloon the agent's wall time. | When Syrus ships a post-grade primitive that runs gates once per workflow rather than per iteration (see `~/code/syrus/docs/plans/syrus-native-ci.md` "M3 scope"). |
| Multi-platform build matrix (macOS, gcc-13 on Linux) | Syrus today is one worker pod (Linux x86_64 on the user's K3s cluster). | When CI fans out to additional runners, or when a separate macOS/Linux smoke runner is wired up. |
| CodeQL SARIF upload | Security-tab integration is a GitHub-only sink. | Possibly a separate vulnerability scanner with a non-GitHub report sink (Sonar, semgrep-cloud, etc.). |
| Doxygen + textbook publish | Static-site deploy needs a hosting target. | Move to Cloudflare Pages, Netlify, or self-hosted. The `rake docs:textbook:html` step is hosting-agnostic; only the upload changes. |
| Tag-driven SBOM signing + GitHub release | Needs Sigstore OIDC + GitHub's release API. | Different signing flow (e.g. private cosign key + S3 upload + manual GitHub release) when releases resume. |
| Monthly mutation testing | Needs a cron scheduler. | Either a system cron that pokes Syrus's API, or restore `mutation.yml` when GH Actions billing returns. |
| Dependabot bumps | Dependabot is a GitHub service. | Replace with [Renovate](https://github.com/renovatebot/renovate) (self-hosted) or accept manual dependency updates. |

## Restoring any of these

The files in this directory are byte-identical to what was at
`.github/workflows/` (and `.github/dependabot.yml`) before the move.
To restore one:

```sh
mkdir -p .github/workflows
git mv docs/plans/github_actions/<file>.yml .github/workflows/
# or for dependabot:
git mv docs/plans/github_actions/dependabot.yml .github/
```

…and commit. They will pick up where they left off as soon as GitHub
Actions can run them.
