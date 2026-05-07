#include <QCoreApplication>
#include <QCommandLineParser>
#include <QImage>

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"
#include "world/objects/Material.h"
#include "world/objects/Texture.h"

#include "raytracer/lights/PointLight.h"
#include "raytracer/RenderEngine.h"
#include "raytracer/Raytracer.h"
#include "raytracer/WireframeEngine.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/cameras/Camera.h"
#include "render/samplers/SamplerFactory.h"
#include "render/tonemap/TonemapFactory.h"
#include "render/viewplanes/TiledViewPlane.h"

#include "core/Buffer.h"

#include <QThread>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

class Renderer {
public:
  enum CommandLineParseResult
  {
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
  QString m_tonemap;
  QString m_engine;
  int m_wireframeLod;
};

Renderer::Renderer()
  : m_maximumRecursionDepth(10),
    m_width(640),
    m_height(480),
    m_sampler("Regular"),
    m_samplesPerPixel(1),
    m_threads(QThread::idealThreadCount()),
    m_queueSize(m_width * m_height * m_samplesPerPixel / 1024),
    m_tonemap("Linear"),
    m_engine("raytracer"),
    m_wireframeLod(0)
{
  parser.setApplicationDescription(QCoreApplication::translate("rendercli", "Command line renderer."));
}

void Renderer::render() const {
  auto scene = new Scene(nullptr);
  scene->load(m_filename);

  auto raytracerScene = scene->toRaytracerScene();

  // Engine-agnostic camera setup. Both engines need a camera with a
  // view plane sized to the output buffer; the only engine-specific
  // wiring (recursion depth, threads, sampler) lives on the engine
  // construction below.
  std::shared_ptr<raytracer::Camera> rtCamera;
  auto camera = scene->activeCamera();
  if (camera) {
    rtCamera = camera->toRaytracer();
  } else {
    qWarning("No camera found. Defaulting to Pinhole camera looking at the origin");
  }

  std::shared_ptr<raytracer::RenderEngine> engine;

  if (m_engine == "wireframe") {
    auto wireframe = std::make_shared<raytracer::WireframeEngine>(raytracerScene);
    if (rtCamera) wireframe->setCamera(rtCamera);
    wireframe->setLod(m_wireframeLod);
    engine = wireframe;
  } else {
    auto rt = std::make_shared<raytracer::Raytracer>(raytracerScene);
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
  engine->render(buffer);

  QImage image = bufferToImage(buffer);

  image.save(m_output);
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

Renderer::CommandLineParseResult Renderer::parseCommandLine(QString *errorMessage) {
  parser.setApplicationDescription("Test helper");
  const QCommandLineOption helpOption = parser.addHelpOption();
  const QCommandLineOption versionOption = parser.addVersionOption();
  
  parser.addOptions({
    {"width", "Output image width", "width"},
    {"height", "Output image height", "height"},
    {"depth", "Maximum recursion depth", "depth"},
    {"sampler", "Sampler type", "sampler"},
    {"samples_per_pixel", "Samples per pixel", "samples"},
    {{"j", "threads"}, "Number of threads", "threads"},
    {"queue_size", "Queue size for thread pool", "queue_size"},
    {"tonemap", "Tonemap operator (Linear, Reinhard, ACES)", "tonemap"},
    {"engine", "Render engine (raytracer, wireframe)", "engine"},
    {"lod", "Tessellation level of detail for wireframe engine", "lod"}
  });
  
  parser.addPositionalArgument("input", QCoreApplication::translate("main", "Input file to render."));
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
  }
  
  if (parser.isSet("queue_size")) {
    const QString queueSizeValue = parser.value("queue_size");
    m_queueSize = queueSizeValue.toInt();
    if (m_queueSize < m_threads) {
      *errorMessage = "Queue size must be > threads";
      return CommandLineError;
    }
  }

  if (parser.isSet("tonemap")) {
    m_tonemap = parser.value("tonemap");
  }

  if (parser.isSet("engine")) {
    const QString engine = parser.value("engine").toLower();
    if (engine != "raytracer" && engine != "wireframe") {
      *errorMessage = "Engine must be 'raytracer' or 'wireframe'";
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
  QCoreApplication::setApplicationName(QCoreApplication::translate("rendercli", "Command line renderer"));
  
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
  
  r.render();
}
