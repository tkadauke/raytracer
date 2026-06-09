// Performance-regression tests for the packed wavefront intersection ABI.
//
// These tests assert ratios, not absolute timings. The packed CPU intersector
// is the host-side contract the Metal/Vulkan kernels must match, and it should
// stay much faster than re-entering the runtime Scene for large supported mesh
// batches. The precise numbers belong in benchmarks/; this test catches
// regressions that accidentally route the packed path back through the runtime
// primitive tree or break flat-BVH traversal.

#include <gtest/gtest.h>

#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "render/GpuIntersectionScene.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/State.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Triangle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace WavefrontIntersectionBackendPerformanceTest {
  using namespace render;

  using Clock = std::chrono::steady_clock;

  class MeshHeavyIntersectionWorkload {
  public:
    MeshHeavyIntersectionWorkload(int side, int rayCount)
        : m_scene(buildScene(side)),
          m_rays(generateRays(rayCount)),
          m_compiled(IntersectionSceneCompiler().compile(*m_scene)),
          m_buffers(GpuIntersectionScenePacker().packScene(m_compiled)),
          m_packedRays(packRays(m_rays)) {
    }

    const GpuIntersectionSceneBuffers& buffers() const {
      return m_buffers;
    }

    std::chrono::nanoseconds timeRuntimeClosestHit() const {
      runRuntimeClosestHit();

      const auto start = Clock::now();
      runRuntimeClosestHit();
      return Clock::now() - start;
    }

    std::chrono::nanoseconds timePackedClosestHit() const {
      runPackedClosestHit();

      const auto start = Clock::now();
      runPackedClosestHit();
      return Clock::now() - start;
    }

    std::chrono::nanoseconds timeRuntimeAnyHit() const {
      runRuntimeAnyHit();

      const auto start = Clock::now();
      runRuntimeAnyHit();
      return Clock::now() - start;
    }

    std::chrono::nanoseconds timePackedAnyHit() const {
      runPackedAnyHit();

      const auto start = Clock::now();
      runPackedAnyHit();
      return Clock::now() - start;
    }

  private:
    static std::shared_ptr<Scene> buildScene(int side) {
      auto scene = std::make_shared<Scene>();

      auto height = [](double x, double z) {
        return std::sin(x * 0.31) * 0.45 + std::cos(z * 0.27) * 0.35;
      };
      for (int x = 0; x != side; ++x) {
        for (int z = 0; z != side; ++z) {
          const double x0 = (x - side / 2) * 0.45;
          const double x1 = (x + 1 - side / 2) * 0.45;
          const double z0 = z * 0.45 + 4.0;
          const double z1 = (z + 1) * 0.45 + 4.0;
          const Vector3d p00{x0, height(x0, z0), z0};
          const Vector3d p10{x1, height(x1, z0), z0};
          const Vector3d p01{x0, height(x0, z1), z1};
          const Vector3d p11{x1, height(x1, z1), z1};
          scene->add(std::make_shared<Triangle>(p00, p10, p11));
          scene->add(std::make_shared<Triangle>(p00, p11, p01));
        }
      }

      return scene;
    }

    static std::vector<Rayd> generateRays(int count) {
      std::mt19937 rng(1234);
      std::uniform_real_distribution<double> xy(-8.0, 8.0);
      std::uniform_real_distribution<double> z(3.5, 22.5);
      std::uniform_real_distribution<double> jitter(-0.25, 0.25);

      std::vector<Rayd> rays;
      rays.reserve(static_cast<std::size_t>(count));
      for (int index = 0; index != count; ++index) {
        const Vector3d origin(xy(rng), 2.5 + jitter(rng), -5.0);
        const Vector3d target(xy(rng), jitter(rng), z(rng));
        rays.emplace_back(origin, (target - origin).normalized());
      }
      return rays;
    }

    static std::vector<GpuIntersectionRay> packRays(const std::vector<Rayd>& rays) {
      GpuIntersectionScenePacker packer;
      std::vector<GpuIntersectionRay> packed;
      packed.reserve(rays.size());
      for (std::size_t index = 0; index != rays.size(); ++index) {
        packed.push_back(packer.packRay(rays[index], static_cast<std::uint32_t>(index), 0.0, 40.0));
      }
      return packed;
    }

    std::size_t runRuntimeClosestHit() const {
      std::size_t hits = 0;
      for (const Rayd& ray : m_rays) {
        State state;
        HitPointInterval hitPoints;
        if (CpuWavefrontIntersectionBackend::instance().intersectClosest(*m_scene, ray, hitPoints,
                                                                         state)) {
          ++hits;
        }
      }
      return hits;
    }

    std::size_t runPackedClosestHit() const {
      const std::vector<GpuIntersectionHitRecord> hits =
        GpuIntersectionIntersector().intersectClosest(m_buffers, m_packedRays);
      return static_cast<std::size_t>(
        std::count_if(hits.begin(), hits.end(), [](const auto& hit) { return hit.hit != 0; }));
    }

    std::size_t runRuntimeAnyHit() const {
      std::size_t hits = 0;
      for (const Rayd& ray : m_rays) {
        State state;
        if (CpuWavefrontIntersectionBackend::instance().intersectAny(*m_scene, ray, 40.0, state)) {
          ++hits;
        }
      }
      return hits;
    }

    std::size_t runPackedAnyHit() const {
      const std::vector<GpuIntersectionOcclusionRecord> records =
        GpuIntersectionIntersector().intersectAny(m_buffers, m_packedRays);
      return static_cast<std::size_t>(std::count_if(
        records.begin(), records.end(), [](const auto& record) { return record.occluded != 0; }));
    }

    std::shared_ptr<Scene> m_scene;
    std::vector<Rayd> m_rays;
    CompiledIntersectionScene m_compiled;
    GpuIntersectionSceneBuffers m_buffers;
    std::vector<GpuIntersectionRay> m_packedRays;
  };

  constexpr double kMinPackedClosestHitRatioVsRuntime = 10.0;
  constexpr double kMinPackedAnyHitRatioVsRuntime = 10.0;

  TEST(WavefrontIntersectionBackendPerformance,
       PackedClosestHitIsAtLeast10xFasterThanRuntimeSceneOnMeshHeavyScene) {
    const MeshHeavyIntersectionWorkload workload(32, 256);
    ASSERT_TRUE(workload.buffers().packedClosestHitKernelEligible());

    const auto runtime = workload.timeRuntimeClosestHit();
    const auto packed = workload.timePackedClosestHit();

    const double ratio =
      static_cast<double>(runtime.count()) / std::max<std::int64_t>(1, packed.count());
    EXPECT_GT(ratio, kMinPackedClosestHitRatioVsRuntime)
      << "Packed closest-hit traversal was only " << ratio << "x faster than runtime Scene "
      << "(runtime=" << runtime.count() << "ns, packed=" << packed.count() << "ns); "
      << "expected at least " << kMinPackedClosestHitRatioVsRuntime << "x.";
  }

  TEST(WavefrontIntersectionBackendPerformance,
       PackedAnyHitIsAtLeast10xFasterThanRuntimeSceneOnMeshHeavyScene) {
    const MeshHeavyIntersectionWorkload workload(32, 256);
    ASSERT_TRUE(workload.buffers().packedAnyHitKernelEligible());

    const auto runtime = workload.timeRuntimeAnyHit();
    const auto packed = workload.timePackedAnyHit();

    const double ratio =
      static_cast<double>(runtime.count()) / std::max<std::int64_t>(1, packed.count());
    EXPECT_GT(ratio, kMinPackedAnyHitRatioVsRuntime)
      << "Packed any-hit traversal was only " << ratio << "x faster than runtime Scene "
      << "(runtime=" << runtime.count() << "ns, packed=" << packed.count() << "ns); "
      << "expected at least " << kMinPackedAnyHitRatioVsRuntime << "x.";
  }
}
