#include <QCoreApplication>
#include <QCommandLineParser>
#include <QImage>

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"
#include "world/objects/Material.h"
#include "world/objects/Texture.h"

#include "render/lights/PointLight.h"
#include "render/RenderEngine.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/raster/Rasterizer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/primitives/Scene.h"
#include "render/cameras/Camera.h"
#include "render/samplers/SamplerFactory.h"
#include "render/tonemap/TonemapFactory.h"
#include "render/viewplanes/TiledViewPlane.h"

#include "core/Buffer.h"

#include <QThread>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

namespace {
  using Clock = std::chrono::steady_clock;

  struct TimingStats {
    double minMs;
    double medianMs;
    double avgMs;
    double maxMs;
  };

  double elapsedMilliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
  }

  TimingStats summarizeTimings(std::vector<double> timings) {
    std::sort(timings.begin(), timings.end());
    const std::size_t n = timings.size();
    const double median =
      (n % 2 == 0) ? (timings[n / 2 - 1] + timings[n / 2]) / 2.0 : timings[n / 2];
    const double total = std::accumulate(timings.begin(), timings.end(), 0.0);
    return {timings.front(), median, total / static_cast<double>(n), timings.back()};
  }

  void printTimings(const std::vector<double>& timings) {
    const auto stats = summarizeTimings(timings);
    std::cout << std::fixed << std::setprecision(3) << "render_ms"
              << " runs=" << timings.size() << " min=" << stats.minMs
              << " median=" << stats.medianMs << " avg=" << stats.avgMs << " max=" << stats.maxMs
              << '\n';
  }

  QString normalizedRasterOption(QString value) {
    value = value.trimmed().toLower();
    value.remove('_');
    value.remove('-');
    value.remove(',');
    value.remove(' ');
    return value;
  }

  bool parseColorWriteMask(const QString& value, std::uint8_t* mask) {
    const QString normalized = normalizedRasterOption(value);
    if (normalized == "none" || normalized == "0") {
      *mask = 0;
      return true;
    }
    if (normalized == "all") {
      *mask = engine::raster::Rasterizer::ColorWriteAll;
      return true;
    }
    if (normalized.isEmpty()) {
      return false;
    }

    std::uint8_t parsed = 0;
    for (const QChar ch : normalized) {
      if (ch == 'r') {
        parsed |= engine::raster::Rasterizer::ColorWriteRed;
      } else if (ch == 'g') {
        parsed |= engine::raster::Rasterizer::ColorWriteGreen;
      } else if (ch == 'b') {
        parsed |= engine::raster::Rasterizer::ColorWriteBlue;
      } else {
        return false;
      }
    }
    *mask = parsed;
    return true;
  }

  bool parseBlendFactor(const QString& value, engine::raster::Rasterizer::BlendFactor* factor) {
    const QString normalized = normalizedRasterOption(value);
    using BlendFactor = engine::raster::Rasterizer::BlendFactor;
    if (normalized == "zero") {
      *factor = BlendFactor::Zero;
    } else if (normalized == "one") {
      *factor = BlendFactor::One;
    } else if (normalized == "sourcecolor" || normalized == "srccolor") {
      *factor = BlendFactor::SourceColor;
    } else if (normalized == "oneminussourcecolor" || normalized == "1minussourcecolor" ||
               normalized == "1srccolor") {
      *factor = BlendFactor::OneMinusSourceColor;
    } else if (normalized == "destinationcolor" || normalized == "dstcolor") {
      *factor = BlendFactor::DestinationColor;
    } else if (normalized == "oneminusdestinationcolor" ||
               normalized == "1minusdestinationcolor" || normalized == "1dstcolor") {
      *factor = BlendFactor::OneMinusDestinationColor;
    } else if (normalized == "constantcolor" || normalized == "constcolor") {
      *factor = BlendFactor::ConstantColor;
    } else if (normalized == "oneminusconstantcolor" || normalized == "1minusconstantcolor" ||
               normalized == "1constcolor") {
      *factor = BlendFactor::OneMinusConstantColor;
    } else if (normalized == "constantalpha" || normalized == "constalpha") {
      *factor = BlendFactor::ConstantAlpha;
    } else if (normalized == "oneminusconstantalpha" || normalized == "1minusconstantalpha" ||
               normalized == "1constalpha") {
      *factor = BlendFactor::OneMinusConstantAlpha;
    } else {
      return false;
    }
    return true;
  }

  bool parseBlendOp(const QString& value, engine::raster::Rasterizer::BlendOp* op) {
    const QString normalized = normalizedRasterOption(value);
    using BlendOp = engine::raster::Rasterizer::BlendOp;
    if (normalized == "add") {
      *op = BlendOp::Add;
    } else if (normalized == "subtract" || normalized == "sub") {
      *op = BlendOp::Subtract;
    } else if (normalized == "reversesubtract" || normalized == "revsub") {
      *op = BlendOp::ReverseSubtract;
    } else if (normalized == "min") {
      *op = BlendOp::Min;
    } else if (normalized == "max") {
      *op = BlendOp::Max;
    } else {
      return false;
    }
    return true;
  }

  bool parseColorTriplet(const QString& value, Colord* color) {
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    if (parts.size() != 3) {
      return false;
    }

    double components[3];
    for (int i = 0; i < 3; ++i) {
      bool ok = false;
      components[i] = parts[i].trimmed().toDouble(&ok);
      if (!ok || !std::isfinite(components[i]) || components[i] < 0.0 ||
          components[i] > 1.0) {
        return false;
      }
    }

    *color = Colord(components[0], components[1], components[2]);
    return true;
  }

  bool parseRasterRect(const QString& value, Recti* rect) {
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    if (parts.size() != 4) {
      return false;
    }

    int components[4];
    for (int i = 0; i < 4; ++i) {
      bool ok = false;
      components[i] = parts[i].trimmed().toInt(&ok);
      if (!ok) {
        return false;
      }
    }

    if (components[2] < 0 || components[3] < 0) {
      return false;
    }

    *rect = Recti(components[0], components[1], components[2], components[3]);
    return true;
  }
}

class Renderer {
public:
  enum CommandLineParseResult {
    CommandLineOk,
    CommandLineError,
    CommandLineVersionRequested,
    CommandLineHelpRequested
  };

  Renderer();
  void render() const;
  CommandLineParseResult parseCommandLine(QString* errorMessage);
  std::shared_ptr<render::Sampler> sampler() const;
  QImage bufferToImage(const Buffer<unsigned int>& buffer) const;

  QCommandLineParser parser;

private:
  QString m_filename;
  QString m_output;

  int m_maximumRecursionDepth;
  int m_width;
  int m_height;
  QString m_sampler;
  int m_samplesPerPixel;
  int m_threads;
  int m_queueSize;
  bool m_threadsSet;
  bool m_queueSizeSet;
  QString m_tonemap;
  QString m_engine;
  int m_wireframeLod;
  QString m_rasterCullMode;
  int m_rasterMsaaSamples;
  QString m_rasterPostProcessAA;
  std::uint8_t m_rasterColorWriteMask;
  bool m_rasterBlending;
  engine::raster::Rasterizer::BlendFactor m_rasterBlendSourceFactor;
  engine::raster::Rasterizer::BlendFactor m_rasterBlendDestinationFactor;
  engine::raster::Rasterizer::BlendOp m_rasterBlendOp;
  Colord m_rasterBlendConstantColor;
  double m_rasterBlendConstantAlpha;
  bool m_rasterViewportSet;
  Recti m_rasterViewport;
  bool m_rasterScissorSet;
  Recti m_rasterScissor;
  double m_rasterDepthBias;
  bool m_rasterShadowMaps;
  int m_rasterShadowMapSize;
  int m_rasterShadowCascadeCount;
  double m_rasterShadowCascadeSplitLambda;
  double m_rasterShadowBias;
  double m_rasterShadowSlopeBias;
  int m_rasterShadowFilterRadius;
  QString m_rasterShadowFilterMode;
  int m_repeat;
  bool m_timing;
  int m_frame;
  bool m_frameSet;
  bool m_animation;
  int m_frameStart;
  int m_frameEnd;
  double m_fps;
  bool m_frameStartSet;
  bool m_frameEndSet;
  bool m_fpsSet;

  std::unique_ptr<Scene> loadScene() const;
  std::vector<double> renderScene(const Scene& scene, const QString& output) const;
  void renderAnimation(const Scene& scene) const;
  QString outputForFrame(int frame) const;
  static bool hasFramePlaceholder(const QString& pattern, QString* errorMessage);
};

Renderer::Renderer()
    : m_maximumRecursionDepth(10),
      m_width(640),
      m_height(480),
      m_sampler("Regular"),
      m_samplesPerPixel(1),
      m_threads(QThread::idealThreadCount()),
      m_queueSize(m_width * m_height * m_samplesPerPixel / 1024),
      m_threadsSet(false),
      m_queueSizeSet(false),
      m_tonemap("Linear"),
      m_engine("raytracer"),
      m_wireframeLod(0),
      m_rasterCullMode("both"),
      m_rasterMsaaSamples(1),
      m_rasterPostProcessAA("none"),
      m_rasterColorWriteMask(engine::raster::Rasterizer::ColorWriteAll),
      m_rasterBlending(false),
      m_rasterBlendSourceFactor(engine::raster::Rasterizer::BlendFactor::One),
      m_rasterBlendDestinationFactor(engine::raster::Rasterizer::BlendFactor::Zero),
      m_rasterBlendOp(engine::raster::Rasterizer::BlendOp::Add),
      m_rasterBlendConstantColor(Colord::white()),
      m_rasterBlendConstantAlpha(1.0),
      m_rasterViewportSet(false),
      m_rasterViewport(),
      m_rasterScissorSet(false),
      m_rasterScissor(),
      m_rasterDepthBias(0.0),
      m_rasterShadowMaps(false),
      m_rasterShadowMapSize(256),
      m_rasterShadowCascadeCount(1),
      m_rasterShadowCascadeSplitLambda(0.5),
      m_rasterShadowBias(1e-3),
      m_rasterShadowSlopeBias(0.0),
      m_rasterShadowFilterRadius(0),
      m_rasterShadowFilterMode("pcf"),
      m_repeat(1),
      m_timing(false),
      m_frame(0),
      m_frameSet(false),
      m_animation(false),
      m_frameStart(0),
      m_frameEnd(0),
      m_fps(0.0),
      m_frameStartSet(false),
      m_frameEndSet(false),
      m_fpsSet(false) {
  parser.setApplicationDescription(
    QCoreApplication::translate("rendercli", "Command line renderer."));
}

std::unique_ptr<Scene> Renderer::loadScene() const {
  auto scene = std::make_unique<Scene>(nullptr);
  if (!scene->load(m_filename))
    throw std::runtime_error(QString("Unable to load input scene: %1").arg(m_filename).toStdString());
  return scene;
}

std::vector<double> Renderer::renderScene(const Scene& scene, const QString& output) const {
  auto raytracerScene = scene.toRaytracerScene();

  // Engine-agnostic camera setup. Both engines need a camera with a
  // view plane sized to the output buffer; the only engine-specific
  // wiring (recursion depth, threads, sampler) lives on the engine
  // construction below.
  std::shared_ptr<render::Camera> rtCamera;
  auto camera = scene.activeCamera();
  if (camera) {
    rtCamera = camera->toRaytracer();
  } else {
    qWarning("No camera found. Defaulting to Pinhole camera looking at the origin");
  }

  std::shared_ptr<render::RenderEngine> engine;

  if (m_engine == "wireframe") {
    auto wireframe = std::make_shared<engine::wireframe::Wireframe>(raytracerScene);
    if (rtCamera)
      wireframe->setCamera(rtCamera);
    wireframe->setLod(m_wireframeLod);
    engine = wireframe;
  } else if (m_engine == "raster") {
    auto raster = std::make_shared<engine::raster::Rasterizer>(raytracerScene);
    if (rtCamera)
      raster->setCamera(rtCamera);
    raster->setLod(m_wireframeLod); // shares the LOD knob with Wireframe
    if (m_rasterCullMode == "back") {
      raster->setCullMode(engine::raster::Rasterizer::CullMode::Back);
    } else if (m_rasterCullMode == "front") {
      raster->setCullMode(engine::raster::Rasterizer::CullMode::Front);
    }
    if (m_threadsSet)
      raster->setMaximumThreads(m_threads);
    if (m_queueSizeSet) {
      raster->setQueueSize(m_queueSize);
    } else if (m_threadsSet) {
      raster->setQueueSize(m_threads);
    }
    raster->setMSAASamples(m_rasterMsaaSamples);
    if (m_rasterPostProcessAA == "fxaa") {
      raster->setPostProcessAA(engine::raster::Rasterizer::PostProcessAA::FXAA);
    }
    raster->setColorWriteMask(m_rasterColorWriteMask);
    raster->setBlendingEnabled(m_rasterBlending);
    raster->setBlendFactors(m_rasterBlendSourceFactor, m_rasterBlendDestinationFactor);
    raster->setBlendOp(m_rasterBlendOp);
    raster->setBlendConstant(m_rasterBlendConstantColor, m_rasterBlendConstantAlpha);
    if (m_rasterViewportSet) {
      raster->setViewportRect(m_rasterViewport);
    }
    if (m_rasterScissorSet) {
      raster->setScissorRect(m_rasterScissor);
    }
    raster->setDepthBias(m_rasterDepthBias);
    raster->setShadowMapsEnabled(m_rasterShadowMaps);
    raster->setShadowMapSize(m_rasterShadowMapSize);
    raster->setShadowCascadeCount(m_rasterShadowCascadeCount);
    raster->setShadowCascadeSplitLambda(m_rasterShadowCascadeSplitLambda);
    raster->setShadowBias(m_rasterShadowBias);
    raster->setShadowSlopeBias(m_rasterShadowSlopeBias);
    raster->setShadowFilterRadius(m_rasterShadowFilterRadius);
    if (m_rasterShadowFilterMode == "pcss") {
      raster->setShadowFilterMode(engine::raster::Rasterizer::ShadowFilterMode::PCSS);
    }
    engine = raster;
  } else {
    auto rt = std::make_shared<engine::raytracer::Raytracer>(raytracerScene);
    // We don't need a fancy view plane, so we can optimize for fast rendering.
    rt->camera()->setViewPlane(std::make_shared<render::TiledViewPlane>());
    rt->setMaximumRecursionDepth(m_maximumRecursionDepth);
    if (rtCamera) {
      rt->setCamera(rtCamera);
    } else {
      rt->camera()->setPosition(Vector3d(0, 0, -5));
    }
    rt->camera()->viewPlane()->setSampler(sampler());
    rt->setMaximumThreads(m_threads);
    rt->setQueueSize(m_queueSize);
    engine = rt;
  }

  if (auto tonemap = render::TonemapFactory::self().createShared(m_tonemap.toStdString())) {
    engine->setTonemap(tonemap);
  } else {
    qWarning("Unknown tonemap %s; falling back to Linear.", qPrintable(m_tonemap));
  }

  Buffer<unsigned int> buffer(m_width, m_height);
  std::vector<double> timings;
  timings.reserve(static_cast<std::size_t>(m_repeat));
  for (int i = 0; i < m_repeat; ++i) {
    const auto start = Clock::now();
    engine->render(buffer);
    const auto end = Clock::now();
    timings.push_back(elapsedMilliseconds(start, end));
  }

  if (m_timing || m_repeat > 1) {
    printTimings(timings);
  }

  QImage image = bufferToImage(buffer);

  if (!image.save(output))
    throw std::runtime_error(QString("Unable to write output image: %1").arg(output).toStdString());

  return timings;
}

void Renderer::render() const {
  auto scene = loadScene();

  if (m_animation) {
    renderAnimation(*scene);
    return;
  }

  if (m_frameSet)
    scene->evaluateAnimationAtFrame(m_frame);

  const auto timings = renderScene(*scene, m_output);
  if (m_timing || m_repeat > 1) {
    printTimings(timings);
  }
}

void Renderer::renderAnimation(const Scene& scene) const {
  const auto* timeline = scene.animation();
  if (!timeline)
    throw std::runtime_error("Animation rendering requires a scene animation block");

  const int startFrame = m_frameStartSet ? m_frameStart : timeline->startFrame();
  const int endFrame = m_frameEndSet ? m_frameEnd : timeline->endFrame();
  const double fps = m_fpsSet ? m_fps : timeline->fps();
  if (endFrame < startFrame)
    throw std::runtime_error("Frame end must be greater than or equal to frame start");

  QString placeholderError;
  if (!hasFramePlaceholder(m_output, &placeholderError))
    throw std::runtime_error(placeholderError.toStdString());

  std::vector<double> frameTimings;
  frameTimings.reserve(static_cast<std::size_t>(endFrame - startFrame + 1));

  for (int frame = startFrame; frame <= endFrame; ++frame) {
    auto evaluatedScene = scene.evaluatedAtFrame(frame);
    const auto output = outputForFrame(frame);
    const auto timings = renderScene(*evaluatedScene, output);
    frameTimings.push_back(timings.front());

    std::cout << "frame " << (frame - startFrame + 1) << "/" << (endFrame - startFrame + 1)
              << " number=" << frame << " fps=" << fps << " output=" << output.toStdString()
              << " render_ms=" << std::fixed << std::setprecision(3) << timings.front()
              << '\n';
  }

  if (m_timing) {
    printTimings(frameTimings);
  }
}

QString Renderer::outputForFrame(int frame) const {
  const auto pattern = m_output.toStdString();
  const int size = std::snprintf(nullptr, 0, pattern.c_str(), frame);
  if (size < 0)
    throw std::runtime_error("Unable to format animation output filename");

  std::vector<char> formatted(static_cast<std::size_t>(size) + 1);
  std::snprintf(formatted.data(), formatted.size(), pattern.c_str(), frame);
  return QString::fromStdString(formatted.data());
}

bool Renderer::hasFramePlaceholder(const QString& pattern, QString* errorMessage) {
  int placeholderCount = 0;
  const auto text = pattern.toStdString();

  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '%')
      continue;

    ++i;
    if (i >= text.size()) {
      *errorMessage = "Animation output pattern has an incomplete printf placeholder";
      return false;
    }
    if (text[i] == '%')
      continue;

    while (i < text.size() && (text[i] == '-' || text[i] == '+' || text[i] == ' ' ||
                              text[i] == '#' || text[i] == '0')) {
      ++i;
    }
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
    if (i < text.size() && text[i] == '.') {
      ++i;
      while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        ++i;
      }
    }

    if (i >= text.size()) {
      *errorMessage = "Animation output pattern has an incomplete printf placeholder";
      return false;
    }
    if (text[i] == 'd' || text[i] == 'i') {
      ++placeholderCount;
      continue;
    }

    *errorMessage =
      "Animation output must contain exactly one printf-style signed integer placeholder such as %04d";
    return false;
  }

  if (placeholderCount != 1) {
    *errorMessage =
      "Animation output must contain exactly one printf-style signed integer placeholder such as %04d";
    return false;
  }

  return true;
}

std::shared_ptr<render::Sampler> Renderer::sampler() const {
  auto samplerClass = m_sampler.toStdString() + "Sampler";
  auto sampler = render::SamplerFactory::self().createShared(samplerClass);
  sampler->setup(m_samplesPerPixel, 83);

  return sampler;
}

QImage Renderer::bufferToImage(const Buffer<unsigned int>& buffer) const {
  QImage image(buffer.width(), buffer.height(), QImage::Format_RGB32);

  for (int i = 0; i != buffer.width(); ++i) {
    for (int j = 0; j != buffer.height(); ++j) {
      image.setPixel(i, j, buffer[j][i]);
    }
  }

  return image;
}

Renderer::CommandLineParseResult Renderer::parseCommandLine(QString* errorMessage) {
  parser.setApplicationDescription(
    QCoreApplication::translate("rendercli", "Command line renderer."));
  const QCommandLineOption helpOption = parser.addHelpOption();
  const QCommandLineOption versionOption = parser.addVersionOption();

  parser.addOptions(
    {{"width", "Output image width", "width"},
     {"height", "Output image height", "height"},
     {"depth", "Maximum recursion depth", "depth"},
     {"sampler", "Sampler type", "sampler"},
     {"samples_per_pixel", "Samples per pixel", "samples"},
     {{"j", "threads"}, "Number of threads", "threads"},
     {"queue_size", "Queue size for thread pool", "queue_size"},
     {"tonemap", "Tonemap operator (Linear, Reinhard, ACES)", "tonemap"},
     {"engine", "Render engine (raytracer, wireframe, raster)", "engine"},
     {"lod", "Tessellation level of detail for wireframe / raster engines", "lod"},
     {"cull", "Rasterizer face culling mode (both, back, front)", "mode"},
     {"msaa", "Rasterizer MSAA samples (1, 2, 4, or 8)", "samples"},
     {"post_aa", "Rasterizer post-process anti-aliasing (none, fxaa)", "mode"},
     {"color_write_mask", "Rasterizer color-write mask (rgb, r, g, b, rg, rb, gb, none)",
      "mask"},
     {"blend", "Enable rasterizer fixed-function blending"},
     {"blend_src", "Rasterizer source blend factor", "factor"},
     {"blend_dst", "Rasterizer destination blend factor", "factor"},
     {"blend_op", "Rasterizer blend operation (add, subtract, reverse_subtract, min, max)",
      "op"},
     {"blend_constant_color", "Rasterizer blend constant color as r,g,b in 0..1", "color"},
     {"blend_constant_alpha", "Rasterizer blend constant alpha in 0..1", "alpha"},
     {"viewport", "Rasterizer viewport rectangle as x,y,width,height", "rect"},
     {"scissor", "Rasterizer scissor rectangle as x,y,width,height", "rect"},
     {"depth_bias", "Rasterizer constant depth bias applied before depth test/write", "bias"},
     {"shadow_maps", "Enable rasterizer directional-light shadow maps"},
     {"shadow_map_size", "Rasterizer shadow-map resolution", "pixels"},
     {"shadow_cascades", "Rasterizer directional-light shadow cascade count", "count"},
     {"shadow_cascade_split", "Rasterizer shadow cascade split blend (0=linear, 1=log)",
      "blend"},
     {"shadow_bias", "Rasterizer shadow-map depth bias", "bias"},
     {"shadow_slope_bias", "Rasterizer slope-scaled shadow-map depth bias", "bias"},
     {"shadow_filter_radius", "Rasterizer shadow filter radius", "radius"},
     {"shadow_filter", "Rasterizer shadow filter (pcf, pcss)", "mode"},
     {"timing", "Print render-only timing information to stdout"},
     {"frame", "Evaluate the scene animation at the given frame before rendering", "frame"},
     {"animation", "Render the scene animation as an image sequence"},
     {"frame_start", "Override the first animation frame", "frame"},
     {"frame_end", "Override the last animation frame", "frame"},
     {"fps", "Override the animation frame rate used for sequence metadata/progress", "fps"},
     {"repeat", "Render the loaded scene N times and print render-only timing statistics",
      "runs"}});

  parser.addPositionalArgument("input",
                               QCoreApplication::translate("main", "Input file to render."));
  parser.addPositionalArgument("output", QCoreApplication::translate("main", "Output file."));

  if (!parser.parse(QCoreApplication::arguments())) {
    *errorMessage = parser.errorText();
    return CommandLineError;
  }

  if (parser.isSet(versionOption))
    return CommandLineVersionRequested;

  if (parser.isSet(helpOption))
    return CommandLineHelpRequested;

  if (parser.isSet("width")) {
    const QString widthValue = parser.value("width");
    m_width = widthValue.toInt();
    if (m_width <= 0) {
      *errorMessage = "Width must be > 0";
      return CommandLineError;
    }
  }

  if (parser.isSet("height")) {
    const QString heightValue = parser.value("height");
    m_height = heightValue.toInt();
    if (m_height <= 0) {
      *errorMessage = "Height must be > 0";
      return CommandLineError;
    }
  }

  if (parser.isSet("depth")) {
    const QString depthValue = parser.value("depth");
    m_maximumRecursionDepth = depthValue.toInt();
    if (m_maximumRecursionDepth <= 0) {
      *errorMessage = "Depth must be > 0";
      return CommandLineError;
    }
  }

  if (parser.isSet("sampler")) {
    m_sampler = parser.value("sampler");
  }

  if (parser.isSet("samples_per_pixel")) {
    const QString samplesPerPixelValue = parser.value("samples_per_pixel");
    m_samplesPerPixel = samplesPerPixelValue.toInt();
    if (m_samplesPerPixel <= 0) {
      *errorMessage = "Samples per pixel must be > 0";
      return CommandLineError;
    }
  }

  if (parser.isSet("threads")) {
    const QString threadsValue = parser.value("threads");
    m_threads = threadsValue.toInt();
    if (m_threads <= 0) {
      *errorMessage = "Threads must be > 0";
      return CommandLineError;
    }
    m_threadsSet = true;
  }

  if (parser.isSet("queue_size")) {
    const QString queueSizeValue = parser.value("queue_size");
    m_queueSize = queueSizeValue.toInt();
    if (m_queueSize < m_threads) {
      *errorMessage = "Queue size must be > threads";
      return CommandLineError;
    }
    m_queueSizeSet = true;
  }

  if (parser.isSet("tonemap")) {
    m_tonemap = parser.value("tonemap");
  }

  if (parser.isSet("engine")) {
    const QString engine = parser.value("engine").toLower();
    if (engine != "raytracer" && engine != "wireframe" && engine != "raster") {
      *errorMessage = "Engine must be 'raytracer', 'wireframe', or 'raster'";
      return CommandLineError;
    }
    m_engine = engine;
  }

  if (parser.isSet("lod")) {
    bool ok = false;
    m_wireframeLod = parser.value("lod").toInt(&ok);
    if (!ok || m_wireframeLod < 0) {
      *errorMessage = "LOD must be a non-negative integer";
      return CommandLineError;
    }
  }

  if (parser.isSet("cull")) {
    const QString cull = parser.value("cull").toLower();
    if (cull != "both" && cull != "back" && cull != "front") {
      *errorMessage = "Cull mode must be 'both', 'back', or 'front'";
      return CommandLineError;
    }
    m_rasterCullMode = cull;
  }

  if (parser.isSet("msaa")) {
    bool ok = false;
    m_rasterMsaaSamples = parser.value("msaa").toInt(&ok);
    if (!ok || (m_rasterMsaaSamples != 1 && m_rasterMsaaSamples != 2 && m_rasterMsaaSamples != 4 &&
                m_rasterMsaaSamples != 8)) {
      *errorMessage = "MSAA samples must be 1, 2, 4, or 8";
      return CommandLineError;
    }
  }

  if (parser.isSet("post_aa")) {
    const QString postAA = parser.value("post_aa").toLower();
    if (postAA != "none" && postAA != "fxaa") {
      *errorMessage = "Post-process AA must be 'none' or 'fxaa'";
      return CommandLineError;
    }
    m_rasterPostProcessAA = postAA;
  }

  if (parser.isSet("color_write_mask")) {
    if (!parseColorWriteMask(parser.value("color_write_mask"), &m_rasterColorWriteMask)) {
      *errorMessage = "Color write mask must contain only r, g, b, or be 'none'";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend")) {
    m_rasterBlending = true;
  }

  if (parser.isSet("blend_src")) {
    if (!parseBlendFactor(parser.value("blend_src"), &m_rasterBlendSourceFactor)) {
      *errorMessage = "Source blend factor is not recognized";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend_dst")) {
    if (!parseBlendFactor(parser.value("blend_dst"), &m_rasterBlendDestinationFactor)) {
      *errorMessage = "Destination blend factor is not recognized";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend_op")) {
    if (!parseBlendOp(parser.value("blend_op"), &m_rasterBlendOp)) {
      *errorMessage = "Blend operation must be add, subtract, reverse_subtract, min, or max";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend_constant_color")) {
    if (!parseColorTriplet(parser.value("blend_constant_color"), &m_rasterBlendConstantColor)) {
      *errorMessage = "Blend constant color must be three comma-separated values in 0..1";
      return CommandLineError;
    }
  }

  if (parser.isSet("blend_constant_alpha")) {
    bool ok = false;
    const double alpha = parser.value("blend_constant_alpha").toDouble(&ok);
    if (!ok || !std::isfinite(alpha) || alpha < 0.0 || alpha > 1.0) {
      *errorMessage = "Blend constant alpha must be a number from 0 to 1";
      return CommandLineError;
    }
    m_rasterBlendConstantAlpha = alpha;
  }

  if (parser.isSet("viewport")) {
    if (!parseRasterRect(parser.value("viewport"), &m_rasterViewport)) {
      *errorMessage =
        "Viewport must be four comma-separated integers x,y,width,height with non-negative size";
      return CommandLineError;
    }
    m_rasterViewportSet = true;
  }

  if (parser.isSet("scissor")) {
    if (!parseRasterRect(parser.value("scissor"), &m_rasterScissor)) {
      *errorMessage =
        "Scissor must be four comma-separated integers x,y,width,height with non-negative size";
      return CommandLineError;
    }
    m_rasterScissorSet = true;
  }

  if (parser.isSet("depth_bias")) {
    bool ok = false;
    m_rasterDepthBias = parser.value("depth_bias").toDouble(&ok);
    if (!ok || !std::isfinite(m_rasterDepthBias)) {
      *errorMessage = "Depth bias must be a finite number";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_maps")) {
    m_rasterShadowMaps = true;
  }

  if (parser.isSet("shadow_map_size")) {
    bool ok = false;
    m_rasterShadowMapSize = parser.value("shadow_map_size").toInt(&ok);
    if (!ok || m_rasterShadowMapSize <= 0) {
      *errorMessage = "Shadow map size must be a positive integer";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_cascades")) {
    bool ok = false;
    m_rasterShadowCascadeCount = parser.value("shadow_cascades").toInt(&ok);
    if (!ok || m_rasterShadowCascadeCount <= 0) {
      *errorMessage = "Shadow cascade count must be a positive integer";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_cascade_split")) {
    bool ok = false;
    m_rasterShadowCascadeSplitLambda = parser.value("shadow_cascade_split").toDouble(&ok);
    if (!ok || !std::isfinite(m_rasterShadowCascadeSplitLambda) ||
        m_rasterShadowCascadeSplitLambda < 0.0 || m_rasterShadowCascadeSplitLambda > 1.0) {
      *errorMessage = "Shadow cascade split blend must be a number from 0 to 1";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_bias")) {
    bool ok = false;
    m_rasterShadowBias = parser.value("shadow_bias").toDouble(&ok);
    if (!ok || m_rasterShadowBias < 0.0) {
      *errorMessage = "Shadow bias must be a non-negative number";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_slope_bias")) {
    bool ok = false;
    m_rasterShadowSlopeBias = parser.value("shadow_slope_bias").toDouble(&ok);
    if (!ok || m_rasterShadowSlopeBias < 0.0) {
      *errorMessage = "Shadow slope bias must be a non-negative number";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_filter_radius")) {
    bool ok = false;
    m_rasterShadowFilterRadius = parser.value("shadow_filter_radius").toInt(&ok);
    if (!ok || m_rasterShadowFilterRadius < 0) {
      *errorMessage = "Shadow filter radius must be a non-negative integer";
      return CommandLineError;
    }
  }

  if (parser.isSet("shadow_filter")) {
    const QString filterMode = parser.value("shadow_filter").toLower();
    if (filterMode != "pcf" && filterMode != "pcss") {
      *errorMessage = "Shadow filter must be 'pcf' or 'pcss'";
      return CommandLineError;
    }
    m_rasterShadowFilterMode = filterMode;
  }

  if (parser.isSet("timing")) {
    m_timing = true;
  }

  if (parser.isSet("animation")) {
    m_animation = true;
  }

  if (parser.isSet("repeat")) {
    bool ok = false;
    m_repeat = parser.value("repeat").toInt(&ok);
    if (!ok || m_repeat <= 0) {
      *errorMessage = "Repeat must be a positive integer";
      return CommandLineError;
    }
    m_timing = true;
  }

  if (parser.isSet("frame")) {
    bool ok = false;
    m_frame = parser.value("frame").toInt(&ok);
    if (!ok) {
      *errorMessage = "Frame must be an integer";
      return CommandLineError;
    }
    m_frameSet = true;
  }

  if (parser.isSet("frame_start")) {
    bool ok = false;
    m_frameStart = parser.value("frame_start").toInt(&ok);
    if (!ok) {
      *errorMessage = "Frame start must be an integer";
      return CommandLineError;
    }
    m_frameStartSet = true;
  }

  if (parser.isSet("frame_end")) {
    bool ok = false;
    m_frameEnd = parser.value("frame_end").toInt(&ok);
    if (!ok) {
      *errorMessage = "Frame end must be an integer";
      return CommandLineError;
    }
    m_frameEndSet = true;
  }

  if (parser.isSet("fps")) {
    bool ok = false;
    m_fps = parser.value("fps").toDouble(&ok);
    if (!ok || m_fps <= 0.0) {
      *errorMessage = "FPS must be a positive number";
      return CommandLineError;
    }
    m_fpsSet = true;
  }

  if (m_animation && m_frameSet) {
    *errorMessage = "Cannot combine --animation with --frame";
    return CommandLineError;
  }

  if (m_animation && m_repeat > 1) {
    *errorMessage = "Cannot combine --animation with --repeat";
    return CommandLineError;
  }

  const QStringList args = parser.positionalArguments();

  if (args.size() < 2) {
    *errorMessage = "Need input and output filename";
    return CommandLineError;
  } else {
    m_filename = args.at(0);
    m_output = args.at(1);
  }

  return CommandLineOk;
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName(
    QCoreApplication::translate("rendercli", "Command line renderer"));

  qRegisterMetaType<Vector3d>();
  qRegisterMetaType<Angled>();
  qRegisterMetaType<Colord>();
  qRegisterMetaType<Material*>();
  qRegisterMetaType<Texture*>();

  Renderer r;
  QString errorMessage;

  switch (r.parseCommandLine(&errorMessage)) {
  case Renderer::CommandLineOk:
    break;
  case Renderer::CommandLineError:
    fputs(qPrintable(errorMessage), stderr);
    fputs("\n\n", stderr);
    fputs(qPrintable(r.parser.helpText()), stderr);
    return 1;
  case Renderer::CommandLineVersionRequested:
    printf("%s %s\n", qPrintable(QCoreApplication::applicationName()),
           qPrintable(QCoreApplication::applicationVersion()));
    return 0;
  case Renderer::CommandLineHelpRequested:
    r.parser.showHelp();
    Q_UNREACHABLE();
  }

  try {
    r.render();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
