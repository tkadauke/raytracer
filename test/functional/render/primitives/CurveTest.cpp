#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/geometry/AttributeColorMap.h"
#include "core/geometry/Polyline.h"
#include "engine/raster/Rasterizer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <memory>
#include <stdexcept>
#include <string>

namespace CurveFunctionalTest {
  using namespace render;

  core::Curve::AttributeValue attributeValueFromJson(const QJsonValue& value) {
    if (value.isBool())
      return value.toBool();
    if (value.isDouble())
      return value.toDouble();
    if (value.isString())
      return value.toString().toStdString();
    if (value.isArray()) {
      const QJsonArray array = value.toArray();
      if (array.size() != 3)
        throw std::runtime_error("vector attribute must contain three values");
      return Vector3d(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
    }
    throw std::runtime_error("unsupported curve fixture attribute value");
  }

  void readAttributes(const QJsonObject& object, core::Curve::AttributeMap& attributes) {
    for (auto it = object.begin(); it != object.end(); ++it)
      attributes[it.key().toStdString()] = attributeValueFromJson(it.value());
  }

  core::Polyline loadPolylineFixture(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      throw std::runtime_error(QString("could not open %1").arg(path).toStdString());

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
      throw std::runtime_error(error.errorString().toStdString());

    const QJsonObject root = document.object();
    if (root.value("kind").toString() != "polyline")
      throw std::runtime_error("curve fixture kind must be polyline");

    std::vector<Vector3d> points;
    const QJsonArray pointArray = root.value("points").toArray();
    for (const auto& value : pointArray) {
      const QJsonArray point = value.toArray();
      if (point.size() != 3)
        throw std::runtime_error("curve fixture points must be three-dimensional");
      points.emplace_back(point[0].toDouble(), point[1].toDouble(), point[2].toDouble());
    }

    core::Polyline polyline(points);
    core::Curve::AttributeMap curveAttributes;
    readAttributes(root.value("attributes").toObject(), curveAttributes);
    for (const auto& attribute : curveAttributes)
      polyline.setAttribute(attribute.first, attribute.second);

    const QJsonArray segments = root.value("segments").toArray();
    if (segments.size() != static_cast<int>(polyline.segmentCount()))
      throw std::runtime_error("curve fixture segment count must match point count - 1");

    for (int i = 0; i != segments.size(); ++i) {
      core::Curve::AttributeMap segmentAttributes;
      readAttributes(segments[i].toObject().value("attributes").toObject(), segmentAttributes);
      for (const auto& attribute : segmentAttributes)
        polyline.setSegmentAttribute(static_cast<std::size_t>(i), attribute.first,
                                     attribute.second);
    }

    return polyline;
  }

  std::shared_ptr<PinholeCamera> headOnCamera() {
    return std::make_shared<PinholeCamera>(Vector3d::null, Vector3d::forward());
  }

  std::shared_ptr<MatteMaterial> matte(const Colord& color) {
    return std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color));
  }

  int countPixels(const Buffer<Colord>& buffer, const Colord& color) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x)
        if (buffer[y][x] == color)
          ++count;
    return count;
  }

  int countNonBackground(const Buffer<Colord>& buffer, const Colord& background) {
    return buffer.width() * buffer.height() - countPixels(buffer, background);
  }

  std::shared_ptr<Scene> sceneWithCurve(std::shared_ptr<Curve> curve) {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->setBackground(Colord::black());
    curve->setMaterial(matte(Colord::red()));
    scene->add(std::move(curve));
    return scene;
  }

  TEST(CurveFixture, PlainPolylineFixtureLoadsPointOnlyPath) {
    const core::Polyline polyline =
      loadPolylineFixture("test/fixtures/curves/plain_polyline.json");

    EXPECT_EQ(4u, polyline.pointCount());
    EXPECT_EQ(3u, polyline.segmentCount());
    ASSERT_NE(nullptr, polyline.attributeAs<std::string>("domain"));
    EXPECT_EQ("route", *polyline.attributeAs<std::string>("domain"));
    for (std::size_t i = 0; i != polyline.segmentCount(); ++i)
      EXPECT_TRUE(polyline.segmentAttributes(i).empty());
  }

  TEST(CurveFixture, AttributedFixtureLoadsScalarAndCategoricalSegmentData) {
    const core::Polyline polyline =
      loadPolylineFixture("test/fixtures/curves/attributed_toolpath.json");

    EXPECT_EQ(5u, polyline.pointCount());
    EXPECT_EQ(4u, polyline.segmentCount());
    ASSERT_NE(nullptr, polyline.attributeAs<std::string>("units"));
    EXPECT_EQ("mm", *polyline.attributeAs<std::string>("units"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<std::string>(0, "move_type"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<double>(1, "feed_rate"));
    EXPECT_EQ("travel", *polyline.segmentAttributeAs<std::string>(0, "move_type"));
    EXPECT_DOUBLE_EQ(1200.0, *polyline.segmentAttributeAs<double>(1, "feed_rate"));
  }

  TEST(CurveRenderSmoke, RasterizerRendersPlainFixtureAsRibbon) {
    auto curve = std::make_shared<Curve>(
      loadPolylineFixture("test/fixtures/curves/plain_polyline.json"), 0.25,
      Curve::TessellationMode::Ribbon);
    engine::raster::Rasterizer engine(headOnCamera(), sceneWithCurve(curve));
    Buffer<Colord> buffer(128, 96);

    engine.render(buffer);

    EXPECT_GT(countNonBackground(buffer, Colord::black()), 0);
  }

  TEST(CurveRenderSmoke, RasterizerRendersAttributedFixtureAsColoredTube) {
    auto curve = std::make_shared<Curve>(
      loadPolylineFixture("test/fixtures/curves/attributed_toolpath.json"), 0.22,
      Curve::TessellationMode::Tube);
    curve->setSegmentColorMap(core::AttributeColorMap::scalar(
      "feed_rate", 900.0, 3000.0, Colord::blue(), Colord::red()));
    engine::raster::Rasterizer engine(headOnCamera(), sceneWithCurve(curve));
    Buffer<Colord> buffer(128, 96);

    engine.render(buffer);

    EXPECT_GT(countNonBackground(buffer, Colord::black()), 0);
  }

  TEST(CurveRenderSmoke, WireframeOverlayRendersZeroWidthAttributedFixture) {
    auto curve = std::make_shared<Curve>(
      loadPolylineFixture("test/fixtures/curves/attributed_toolpath.json"), 0.0,
      Curve::TessellationMode::Ribbon);
    auto colorMap = core::AttributeColorMap::categorical("move_type");
    colorMap.setCategoryColor(std::string("travel"), Colord::blue());
    colorMap.setCategoryColor(std::string("print"), Colord::green());
    colorMap.setCategoryColor(std::string("retract"), Colord::red());
    curve->setSegmentColorMap(colorMap);

    engine::wireframe::Wireframe engine(headOnCamera(), sceneWithCurve(curve));
    engine.setGeometryMode(engine::wireframe::Wireframe::GeometryMode::CurveOverlay);
    Buffer<Colord> buffer(128, 96);

    engine.render(buffer);

    EXPECT_GT(countNonBackground(buffer, Colord::black()), 0);
    EXPECT_GT(countPixels(buffer, Colord::blue()), 0);
    EXPECT_GT(countPixels(buffer, Colord::green()), 0);
    EXPECT_GT(countPixels(buffer, Colord::red()), 0);
  }
}
