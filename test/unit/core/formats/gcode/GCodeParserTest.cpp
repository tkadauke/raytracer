#include <gtest/gtest.h>
#include "core/formats/gcode/GCodeParser.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

namespace GCodeParserTest {
  TEST(GCodeParser, ShouldParseAbsoluteMotionExtrusionAndFeedRate) {
    istringstream stream("G90\n"
                         "M82\n"
                         "G1 X10 Y5 Z0.2 F1800\n"
                         "G1 X12.5 E0.75 F1200 ; perimeter\n");

    const auto program = GCodeParser().parse(stream);

    ASSERT_EQ(2u, program.motions.size());
    EXPECT_EQ(Vector3d(0, 0, 0), program.motions[0].start);
    EXPECT_EQ(Vector3d(10, 5, 0.2), program.motions[0].end);
    EXPECT_EQ(1800.0, program.motions[0].feedRate);
    EXPECT_TRUE(program.motions[0].isTravel());
    EXPECT_EQ(Vector3d(10, 5, 0.2), program.motions[1].start);
    EXPECT_EQ(Vector3d(12.5, 5, 0.2), program.motions[1].end);
    EXPECT_DOUBLE_EQ(0.75, program.motions[1].extrusionDelta);
    EXPECT_EQ(1200.0, program.motions[1].feedRate);
    EXPECT_EQ("perimeter", program.motions[1].comment);
    EXPECT_FALSE(program.diagnostics.hasErrors());
  }

  TEST(GCodeParser, ShouldParseRelativeMotionAndRelativeExtrusion) {
    istringstream stream("G90\n"
                         "G1 X10 Y10 Z0.3\n"
                         "G91\n"
                         "M83\n"
                         "G1 X1 Y-2 E0.4\n"
                         "G1 E-0.2\n");

    const auto program = GCodeParser().parse(stream);

    ASSERT_EQ(3u, program.motions.size());
    EXPECT_EQ(Vector3d(10, 10, 0.3), program.motions[1].start);
    EXPECT_EQ(Vector3d(11, 8, 0.3), program.motions[1].end);
    EXPECT_DOUBLE_EQ(0.4, program.motions[1].extrusionDelta);
    EXPECT_TRUE(program.motions[1].isExtruding());
    EXPECT_EQ(Vector3d(11, 8, 0.3), program.motions[2].start);
    EXPECT_EQ(Vector3d(11, 8, 0.3), program.motions[2].end);
    EXPECT_DOUBLE_EQ(-0.2, program.motions[2].extrusionDelta);
    EXPECT_TRUE(program.motions[2].isTravel());
  }

  TEST(GCodeParser, ShouldResetExtruderWithG92) {
    istringstream stream("M82\n"
                         "G1 E3\n"
                         "G92 E0\n"
                         "G1 E0.5\n");

    const auto program = GCodeParser().parse(stream);

    ASSERT_EQ(2u, program.motions.size());
    EXPECT_DOUBLE_EQ(3.0, program.motions[0].extrusionDelta);
    EXPECT_DOUBLE_EQ(0.5, program.motions[1].extrusionDelta);
  }

  TEST(GCodeParser, ShouldTrackLayersAndSlicerFeatureComments) {
    istringstream stream(";LAYER:4\n"
                         ";TYPE:WALL-INNER\n"
                         "G1 Z0.8 F900\n"
                         "G1 X1 E0.1\n"
                         ";LAYER_CHANGE\n"
                         ";Z:1.0\n"
                         "G1 Z1.0\n");

    const auto program = GCodeParser().parse(stream);

    ASSERT_GE(program.layers.size(), 3u);
    EXPECT_EQ(4, program.layers[0].index);
    EXPECT_EQ("LAYER", program.metadata[0].key);
    ASSERT_EQ(3u, program.motions.size());
    EXPECT_EQ(4, program.motions[1].layerIndex);
    EXPECT_EQ("WALL-INNER", program.motions[1].featureType);
    EXPECT_EQ(1.0, program.layers.back().z);
  }

  TEST(GCodeParser, ShouldPreserveTemperatureAndToolMetadata) {
    istringstream stream("T1\n"
                         "M104 S205\n"
                         "M109 S210 T0\n"
                         "M140 S60\n"
                         "M190 S65\n");

    const auto program = GCodeParser().parse(stream);

    ASSERT_EQ(1u, program.toolChanges.size());
    EXPECT_EQ(1, program.toolChanges[0].tool);
    ASSERT_EQ(4u, program.temperatures.size());
    EXPECT_EQ(GCodeTemperatureTarget::Tool, program.temperatures[0].target);
    EXPECT_FALSE(program.temperatures[0].wait);
    EXPECT_EQ(1, program.temperatures[0].tool);
    EXPECT_EQ(205.0, program.temperatures[0].temperature);
    EXPECT_TRUE(program.temperatures[1].wait);
    EXPECT_EQ(0, program.temperatures[1].tool);
    EXPECT_EQ(GCodeTemperatureTarget::Bed, program.temperatures[2].target);
    EXPECT_TRUE(program.temperatures[3].wait);
  }

  TEST(GCodeParser, ShouldIgnoreUnknownCommandsWithDiagnostics) {
    istringstream stream("G1 X1\n"
                         "M486 S0\n"
                         "Q12 A1\n"
                         "bad\n"
                         "G1 X2\n");

    const auto program = GCodeParser().parse(stream);

    ASSERT_EQ(2u, program.motions.size());
    ASSERT_EQ(3u, program.diagnostics.entries().size());
    EXPECT_EQ(GCodeDiagnosticCode::UnsupportedCommand, program.diagnostics.entries()[0].code);
    EXPECT_EQ("M486", program.diagnostics.entries()[0].command);
    EXPECT_EQ(GCodeDiagnosticCode::UnknownCommand, program.diagnostics.entries()[1].code);
    EXPECT_EQ("Q12", program.diagnostics.entries()[1].command);
    EXPECT_EQ(GCodeDiagnosticCode::InvalidNumber, program.diagnostics.entries()[2].code);
  }

  TEST(GCodeParser, ShouldParseHandAuthoredFixture) {
    ifstream stream("test/fixtures/gcode/absolute_layers.gcode");
    ASSERT_TRUE(stream.good());

    const auto program = GCodeParser().parse(stream);

    ASSERT_EQ(5u, program.motions.size());
    EXPECT_EQ(2u, program.temperatures.size());
    EXPECT_EQ(1u, program.toolChanges.size());
    ASSERT_GE(program.layers.size(), 2u);
    EXPECT_NE(program.layers.end(), find_if(program.layers.begin(), program.layers.end(),
                                            [](const auto& layer) { return layer.index == 0; }));
    EXPECT_NE(program.layers.end(), find_if(program.layers.begin(), program.layers.end(),
                                            [](const auto& layer) { return layer.index == 1; }));
    EXPECT_EQ("SKIRT", program.motions[1].featureType);
    EXPECT_TRUE(program.motions[1].isExtruding());
    EXPECT_TRUE(program.motions[2].isTravel());
    EXPECT_EQ("WALL-INNER", program.motions.back().featureType);
  }

  TEST(GCodeParser, ShouldParseRelativeExtrusionFixture) {
    ifstream stream("test/fixtures/gcode/relative_extrusion.gcode");
    ASSERT_TRUE(stream.good());

    const auto program = GCodeParser().parse(stream);

    ASSERT_EQ(4u, program.motions.size());
    EXPECT_EQ(Vector3d(10, 10, 0.3), program.motions[1].start);
    EXPECT_EQ(Vector3d(11, 10, 0.3), program.motions[1].end);
    EXPECT_DOUBLE_EQ(0.25, program.motions[1].extrusionDelta);
    EXPECT_EQ(Vector3d(11, 11, 0.3), program.motions[2].end);
    EXPECT_DOUBLE_EQ(0.25, program.motions[2].extrusionDelta);
    EXPECT_DOUBLE_EQ(-0.1, program.motions[3].extrusionDelta);
  }
}
