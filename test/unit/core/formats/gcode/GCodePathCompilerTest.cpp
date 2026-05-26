#include <gtest/gtest.h>

#include "core/formats/gcode/GCodeParser.h"
#include "core/formats/gcode/GCodePathCompiler.h"

#include <fstream>
#include <sstream>

namespace GCodePathCompilerTest {
  TEST(GCodePathCompiler, GroupsVisibleMovesByLayerToolFeatureAndMoveType) {
    std::istringstream stream("T1\n"
                              "G90\n"
                              "M82\n"
                              ";LAYER:0\n"
                              ";TYPE:SKIRT\n"
                              "G1 X0 Y0 Z0.2 F1200\n"
                              "G1 X10 Y0 E0.5 F900\n"
                              "G0 X12 Y3 F3000\n"
                              ";TYPE:WALL-INNER\n"
                              "G1 X12 Y8 E0.8 F1000\n"
                              ";LAYER:1\n"
                              "G1 Z0.4 F900\n"
                              "G1 X20 Y8 E1.2 F1100\n");

    const auto program = GCodeParser().parse(stream);
    const auto paths = GCodePathCompiler().compile(program);

    ASSERT_EQ(2u, paths.layers.size());
    EXPECT_EQ(0, paths.layers[0].index);
    EXPECT_EQ(1, paths.layers[1].index);
    ASSERT_EQ(4u, paths.layers[0].paths.size());

    const auto& layerZeroTravel = paths.layers[0].paths[0];
    EXPECT_EQ(1, layerZeroTravel.tool);
    EXPECT_EQ(GCodePathMoveType::Travel, layerZeroTravel.moveType);
    EXPECT_EQ("SKIRT", layerZeroTravel.featureType);

    const auto& layerZeroExtrusion = paths.layers[0].paths[1];
    EXPECT_EQ(GCodePathMoveType::Extrusion, layerZeroExtrusion.moveType);
    ASSERT_EQ(1u, layerZeroExtrusion.polyline.segmentCount());
    EXPECT_EQ("extrusion",
              *layerZeroExtrusion.polyline.segmentAttributeAs<std::string>(0, "move_type"));
    EXPECT_DOUBLE_EQ(900.0, *layerZeroExtrusion.polyline.segmentAttributeAs<double>(0, "speed"));
    EXPECT_DOUBLE_EQ(
      0.5, *layerZeroExtrusion.polyline.segmentAttributeAs<double>(0, "extrusion_amount"));
    EXPECT_EQ(1, *layerZeroExtrusion.polyline.segmentAttributeAs<int>(0, "tool"));

    const auto& layerOne = paths.layers[1];
    ASSERT_EQ(2u, layerOne.paths.size());
    EXPECT_EQ(GCodePathMoveType::Travel, layerOne.paths[0].moveType);
    EXPECT_EQ(GCodePathMoveType::Extrusion, layerOne.paths[1].moveType);
    EXPECT_DOUBLE_EQ(0.4, layerOne.z);
  }

  TEST(GCodePathCompiler, CompilesFixtureIntoDistinctTravelAndExtrusionCurves) {
    std::ifstream stream("test/fixtures/gcode/absolute_layers.gcode");
    ASSERT_TRUE(stream.good());

    const auto program = GCodeParser().parse(stream);
    const auto paths = GCodePathCompiler().compile(program);

    ASSERT_EQ(2u, paths.layers.size());
    ASSERT_EQ(3u, paths.layers[0].paths.size());
    EXPECT_EQ(GCodePathMoveType::Travel, paths.layers[0].paths[0].moveType);
    EXPECT_EQ(GCodePathMoveType::Extrusion, paths.layers[0].paths[1].moveType);
    EXPECT_EQ(GCodePathMoveType::Travel, paths.layers[0].paths[2].moveType);
    ASSERT_EQ(2u, paths.layers[1].paths.size());
    EXPECT_EQ(GCodePathMoveType::Travel, paths.layers[1].paths[0].moveType);
    EXPECT_EQ(GCodePathMoveType::Extrusion, paths.layers[1].paths[1].moveType);
  }
}
