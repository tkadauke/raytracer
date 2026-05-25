#include <gtest/gtest.h>
#include "core/formats/ldraw/LDrawParseError.h"
#include "core/formats/ldraw/LDrawParser.h"

#include <sstream>
#include <string>
#include <variant>

using namespace std;

namespace LDrawParserTest {
  using namespace ::testing;

  template<class T>
  const T& getCommand(const LDrawCommand& command) {
    return get<T>(command);
  }

  TEST(LDrawParser, ShouldParseEmptyLines) {
    istringstream stream("\n   \t  \n");
    const auto commands = LDrawParser().parse(stream);

    ASSERT_EQ(2u, commands.size());
    ASSERT_TRUE(holds_alternative<LDrawEmptyLine>(commands[0]));
    ASSERT_EQ(2, getCommand<LDrawEmptyLine>(commands[1]).lineNumber);
  }

  TEST(LDrawParser, ShouldParseComments) {
    const auto command = LDrawParser().parseLine("0 // a plain comment", 7);
    const auto& comment = getCommand<LDrawMetaCommand>(command);

    ASSERT_TRUE(comment.isComment());
    ASSERT_EQ(7, comment.lineNumber);
    ASSERT_EQ("// a plain comment", comment.text);
    ASSERT_TRUE(comment.arguments.empty());
  }

  TEST(LDrawParser, ShouldParseGenericMetaCommands) {
    const auto command = LDrawParser().parseLine("0 BFC CERTIFY CCW", 1);
    const auto& meta = getCommand<LDrawMetaCommand>(command);

    ASSERT_FALSE(meta.isComment());
    ASSERT_EQ("BFC", meta.keyword);
    ASSERT_EQ(2u, meta.arguments.size());
    ASSERT_EQ("CERTIFY", meta.arguments[0]);
    ASSERT_EQ("CCW", meta.arguments[1]);
    ASSERT_EQ("BFC CERTIFY CCW", meta.text);
  }

  TEST(LDrawParser, ShouldPreserveUnknownLineTypes) {
    const auto command = LDrawParser().parseLine("9 extension data", 4);
    const auto& unknown = getCommand<LDrawUnknownCommand>(command);

    ASSERT_EQ(4, unknown.lineNumber);
    ASSERT_EQ("9", unknown.lineType);
    ASSERT_EQ("9 extension data", unknown.text);
  }

  TEST(LDrawParser, ShouldParseSubfileReferences) {
    const auto command =
      LDrawParser().parseLine("1 16 10 20 30 1 0 0 0 1 0 0 0 1 sub folder/my part.dat", 3);
    const auto& subfile = getCommand<LDrawSubfileReference>(command);

    ASSERT_EQ(3, subfile.lineNumber);
    ASSERT_EQ(16, subfile.color);
    ASSERT_EQ(Vector3d(10, 20, 30), subfile.translation);
    ASSERT_EQ(1, subfile.matrix[0]);
    ASSERT_EQ(0, subfile.matrix[1]);
    ASSERT_EQ(1, subfile.matrix[4]);
    ASSERT_EQ(1, subfile.matrix[8]);
    ASSERT_EQ("sub folder/my part.dat", subfile.filename);
  }

  TEST(LDrawParser, ShouldParseEdgeLines) {
    const auto command = LDrawParser().parseLine("2 24 0 1 2 3 4 5", 1);
    const auto& edge = getCommand<LDrawEdgeLine>(command);

    ASSERT_EQ(24, edge.color);
    ASSERT_EQ(Vector3d(0, 1, 2), edge.points[0]);
    ASSERT_EQ(Vector3d(3, 4, 5), edge.points[1]);
  }

  TEST(LDrawParser, ShouldParseTriangles) {
    const auto command = LDrawParser().parseLine("3 16 0 0 0 1 0 0 0 1 0", 1);
    const auto& triangle = getCommand<LDrawTriangle>(command);

    ASSERT_EQ(16, triangle.color);
    ASSERT_EQ(Vector3d(0, 0, 0), triangle.points[0]);
    ASSERT_EQ(Vector3d(1, 0, 0), triangle.points[1]);
    ASSERT_EQ(Vector3d(0, 1, 0), triangle.points[2]);
  }

  TEST(LDrawParser, ShouldParseQuads) {
    const auto command = LDrawParser().parseLine("4 16 0 0 0 1 0 0 1 1 0 0 1 0", 1);
    const auto& quad = getCommand<LDrawQuad>(command);

    ASSERT_EQ(16, quad.color);
    ASSERT_EQ(Vector3d(0, 0, 0), quad.points[0]);
    ASSERT_EQ(Vector3d(1, 0, 0), quad.points[1]);
    ASSERT_EQ(Vector3d(1, 1, 0), quad.points[2]);
    ASSERT_EQ(Vector3d(0, 1, 0), quad.points[3]);
  }

  TEST(LDrawParser, ShouldParseOptionalLines) {
    const auto command = LDrawParser().parseLine("5 24 0 0 0 1 0 0 0 1 0 1 1 0", 1);
    const auto& optionalLine = getCommand<LDrawOptionalLine>(command);

    ASSERT_EQ(24, optionalLine.color);
    ASSERT_EQ(Vector3d(0, 0, 0), optionalLine.points[0]);
    ASSERT_EQ(Vector3d(1, 0, 0), optionalLine.points[1]);
    ASSERT_EQ(Vector3d(0, 1, 0), optionalLine.points[2]);
    ASSERT_EQ(Vector3d(1, 1, 0), optionalLine.points[3]);
  }

  TEST(LDrawParser, ShouldParseMultipleCommandsFromStream) {
    istringstream stream("0 FILE sample.ldr\n2 24 0 0 0 1 0 0\n");
    const auto commands = LDrawParser().parse(stream);

    ASSERT_EQ(2u, commands.size());
    ASSERT_TRUE(holds_alternative<LDrawMetaCommand>(commands[0]));
    ASSERT_TRUE(holds_alternative<LDrawEdgeLine>(commands[1]));
  }

  TEST(LDrawParser, ShouldSplitMpdInputIntoNamedFileBlocks) {
    istringstream stream(
      "0 FILE main.ldr\n"
      "1 16 0 0 0 1 0 0 0 1 0 0 0 1 sub.dat\n"
      "0 FILE sub.dat\n"
      "3 4 0 0 0 1 0 0 0 1 0\n");

    const auto document = LDrawParser().parseDocument(stream);

    ASSERT_TRUE(document.isMultipart());
    ASSERT_EQ(2u, document.files.size());
    EXPECT_EQ("main.ldr", document.mainFile().filename);
    ASSERT_EQ(1u, document.mainFile().commands.size());
    EXPECT_TRUE(holds_alternative<LDrawSubfileReference>(document.mainFile().commands[0]));
    EXPECT_EQ("sub.dat", document.files[1].filename);
    ASSERT_EQ(1u, document.files[1].commands.size());
    EXPECT_TRUE(holds_alternative<LDrawTriangle>(document.files[1].commands[0]));
  }

  TEST(LDrawParser, ShouldCloseMpdBlocksAtNofileUntilNextFile) {
    istringstream stream(
      "0 FILE main.ldr\n"
      "2 24 0 0 0 1 0 0\n"
      "0 NOFILE\n"
      "3 4 0 0 0 1 0 0 0 1 0\n"
      "0 FILE sub.dat\n"
      "4 4 0 0 0 1 0 0 1 1 0 0 1 0\n"
      "0 NOFILE\n"
      "5 24 0 0 0 1 0 0 0 1 0 1 1 0\n");

    const auto document = LDrawParser().parseDocument(stream);

    ASSERT_EQ(2u, document.files.size());
    ASSERT_EQ(1u, document.files[0].commands.size());
    EXPECT_TRUE(holds_alternative<LDrawEdgeLine>(document.files[0].commands[0]));
    ASSERT_EQ(1u, document.files[1].commands.size());
    EXPECT_TRUE(holds_alternative<LDrawQuad>(document.files[1].commands[0]));
  }

  TEST(LDrawParser, ShouldThrowLDrawParseErrorForInvalidTypeOneNumber) {
    try {
      LDrawParser().parseLine("1 16 0 nope 0 1 0 0 0 1 0 0 0 1 part.dat", 42);
      FAIL() << "Expected LDrawParseError";
    } catch (const LDrawParseError& error) {
      ASSERT_EQ(42, error.sourceLineNumber());
      ASSERT_NE(string::npos, error.message().find("line 42"));
      ASSERT_NE(string::npos, error.message().find("translation.y"));
    }
  }

  TEST(LDrawParser, ShouldThrowLDrawParseErrorForInvalidEdgeLine) {
    ASSERT_THROW(LDrawParser().parseLine("2 24 0 0 0 1 0 bad", 1), LDrawParseError);
  }

  TEST(LDrawParser, ShouldThrowLDrawParseErrorForInvalidTriangle) {
    ASSERT_THROW(LDrawParser().parseLine("3 16 0 0 0 1 0 0 0 1 bad", 1), LDrawParseError);
  }

  TEST(LDrawParser, ShouldThrowLDrawParseErrorForInvalidQuad) {
    ASSERT_THROW(LDrawParser().parseLine("4 16 0 0 0 1 0 0 1 1 0 0 1 bad", 1), LDrawParseError);
  }

  TEST(LDrawParser, ShouldThrowLDrawParseErrorForInvalidOptionalLine) {
    ASSERT_THROW(LDrawParser().parseLine("5 24 0 0 0 1 0 0 0 1 0 1 1 bad", 1), LDrawParseError);
  }

  TEST(LDrawParser, ShouldThrowLDrawParseErrorForMissingSubfileFilename) {
    ASSERT_THROW(LDrawParser().parseLine("1 16 0 0 0 1 0 0 0 1 0 0 0 1", 1), LDrawParseError);
  }
}
