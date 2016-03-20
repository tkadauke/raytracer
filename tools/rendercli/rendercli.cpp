#include <QCoreApplication>
#include <QCommandLineParser>
#include <QImage>

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"
#include "world/objects/Material.h"
#include "world/objects/Texture.h"

#include "raytracer/lights/PointLight.h"
#include "raytracer/Raytracer.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/cameras/Camera.h"
#include "raytracer/samplers/SamplerFactory.h"
#include "raytracer/viewplanes/TiledViewPlane.h"

#include "core/Buffer.h"

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
  std::shared_ptr<raytracer::Sampler> sampler() const;
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
};

Renderer::Renderer()
  : m_maximumRecursionDepth(10),
    m_width(640),
    m_height(480),
    m_sampler("Regular"),
    m_samplesPerPixel(1)
{
  parser.setApplicationDescription(QCoreApplication::translate("rendercli", "Command line renderer."));
}

void Renderer::render() const {
  auto scene = new Scene(nullptr);
  scene->load(m_filename);
  
  auto raytracerScene = scene->toRaytracerScene();
  
  auto raytracer = std::make_shared<raytracer::Raytracer>(raytracerScene);
  // We don't need a fancy view plane, so we can optimize for fast rendering.
  raytracer->camera()->setViewPlane(std::make_shared<raytracer::TiledViewPlane>());
  raytracer->setMaximumRecursionDepth(m_maximumRecursionDepth);
  
  auto camera = scene->activeCamera();
  if (camera) {
    raytracer->setCamera(camera->toRaytracer());
  } else {
    qWarning("No camera found. Defaulting to Pinhole camera looking at the origin");
    raytracer->camera()->setPosition(Vector3d(0, 0, -5));
  }
  
  raytracer->camera()->viewPlane()->setSampler(sampler());
  
  Buffer<unsigned int> buffer(m_width, m_height);
  raytracer->render(buffer);
  
  QImage image = bufferToImage(buffer);
  
  image.save(m_output);
}

std::shared_ptr<raytracer::Sampler> Renderer::sampler() const {
  auto samplerClass = m_sampler.toStdString() + "Sampler";
  auto sampler = std::shared_ptr<raytracer::Sampler>(
    raytracer::SamplerFactory::self().create(samplerClass)
  );
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
    {"samples_per_pixel", "Samples per pixel", "samples"}
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
