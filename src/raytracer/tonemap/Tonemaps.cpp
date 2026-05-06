// Single TU for tonemap factory registration. The operators
// themselves are header-only (stateless math); this file exists
// purely so the static initializer for each factory entry has
// somewhere to live.
#include "raytracer/tonemap/TonemapFactory.h"
#include "raytracer/tonemap/LinearTonemap.h"
#include "raytracer/tonemap/ReinhardTonemap.h"
#include "raytracer/tonemap/AcesTonemap.h"

using namespace raytracer;

static bool linearDummy   = TonemapFactory::self().registerClass<LinearTonemap>("Linear");
static bool reinhardDummy = TonemapFactory::self().registerClass<ReinhardTonemap>("Reinhard");
static bool acesDummy     = TonemapFactory::self().registerClass<AcesTonemap>("ACES");
