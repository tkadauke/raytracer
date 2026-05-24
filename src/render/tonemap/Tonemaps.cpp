// Single TU for tonemap factory registration. The operators
// themselves are header-only (stateless math); this file exists
// purely so the static initializer for each factory entry has
// somewhere to live.
#include "render/tonemap/TonemapFactory.h"
#include "render/tonemap/LinearTonemap.h"
#include "render/tonemap/ReinhardTonemap.h"
#include "render/tonemap/AcesTonemap.h"

using namespace render;

static bool linearDummy = TonemapFactory::self().registerClass<LinearTonemap>("Linear");
static bool reinhardDummy = TonemapFactory::self().registerClass<ReinhardTonemap>("Reinhard");
static bool acesDummy = TonemapFactory::self().registerClass<AcesTonemap>("ACES");
