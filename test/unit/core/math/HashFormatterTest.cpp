#include <gtest/gtest.h>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include "core/math/Vector.h"
#include "core/math/Matrix.h"
#include "core/math/BoundingBox.h"
#include "core/math/Quaternion.h"

namespace HashFormatterTest {

  // ---------------------------------------------------------------------------
  // Vector hash
  // ---------------------------------------------------------------------------

  TEST(VectorHashTest, EqualVectors3dHashEqual) {
    std::hash<Vector3d> h;
    EXPECT_EQ(h(Vector3d(1.0, 2.0, 3.0)), h(Vector3d(1.0, 2.0, 3.0)));
    EXPECT_EQ(h(Vector3d(0.0, 0.0, 0.0)), h(Vector3d(0.0, 0.0, 0.0)));
  }

  TEST(VectorHashTest, EqualVectors2fHashEqual) {
    std::hash<Vector2f> h;
    EXPECT_EQ(h(Vector2f(1.0f, 2.0f)), h(Vector2f(1.0f, 2.0f)));
  }

  TEST(VectorHashTest, EqualVectors4fHashEqual) {
    std::hash<Vector4f> h;
    EXPECT_EQ(h(Vector4f(1.0f, 2.0f, 3.0f, 4.0f)), h(Vector4f(1.0f, 2.0f, 3.0f, 4.0f)));
  }

  TEST(VectorHashTest, EqualVectors3fHashEqual) {
    std::hash<Vector3f> h;
    EXPECT_EQ(h(Vector3f(1.0f, 2.0f, 3.0f)), h(Vector3f(1.0f, 2.0f, 3.0f)));
  }

  TEST(VectorHashTest, DifferentVectors3dLikelyHashDifferent) {
    std::hash<Vector3d> h;
    EXPECT_NE(h(Vector3d(1.0, 2.0, 3.0)), h(Vector3d(4.0, 5.0, 6.0)));
  }

  // Vertex deduplication: the acceptance criterion from the issue.
  TEST(VectorHashTest, VertexDeduplicationViaUnorderedMap) {
    std::unordered_map<Vector3d, int> vertexMap;

    Vector3d v1(1.0, 2.0, 3.0);
    Vector3d v2(4.0, 5.0, 6.0);
    Vector3d v3(1.0, 2.0, 3.0); // duplicate of v1

    vertexMap[v1] = 0;
    vertexMap[v2] = 1;
    vertexMap[v3] = 2; // overwrites v1's entry

    EXPECT_EQ(2u, vertexMap.size());
    EXPECT_EQ(2, vertexMap[v1]);
    EXPECT_EQ(1, vertexMap[v2]);
  }

  TEST(VectorHashTest, UnorderedSetDeduplication) {
    std::unordered_set<Vector3d> s;
    s.insert(Vector3d(1.0, 2.0, 3.0));
    s.insert(Vector3d(4.0, 5.0, 6.0));
    s.insert(Vector3d(1.0, 2.0, 3.0)); // duplicate

    EXPECT_EQ(2u, s.size());
  }

  // ---------------------------------------------------------------------------
  // Matrix hash
  // ---------------------------------------------------------------------------

  TEST(MatrixHashTest, EqualMatrices3dHashEqual) {
    std::hash<Matrix3d> h;
    Matrix3d identity; // default ctor produces the identity matrix
    EXPECT_EQ(h(identity), h(identity));
  }

  TEST(MatrixHashTest, EqualMatrices4dHashEqual) {
    std::hash<Matrix4d> h;
    Matrix4d identity; // default ctor produces the identity matrix
    EXPECT_EQ(h(identity), h(identity));
  }

  TEST(MatrixHashTest, DifferentMatrices3dLikelyHashDifferent) {
    std::hash<Matrix3d> h;
    Matrix3d identity;
    Matrix3d m = identity;
    m[0][0] = 2.0;
    EXPECT_NE(h(identity), h(m));
  }

  TEST(MatrixHashTest, Matrix4UsableAsUnorderedMapKey) {
    std::unordered_map<Matrix4d, int> table;
    Matrix4d identity;
    table[identity] = 42;
    EXPECT_EQ(1u, table.size());
    EXPECT_EQ(42, table[identity]);
  }

  // ---------------------------------------------------------------------------
  // BoundingBox hash
  // ---------------------------------------------------------------------------

  TEST(BoundingBoxHashTest, EqualBoxesHashEqual) {
    std::hash<BoundingBoxd> h;
    BoundingBoxd b1(Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 1.0, 1.0));
    BoundingBoxd b2(Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 1.0, 1.0));
    EXPECT_EQ(h(b1), h(b2));
  }

  TEST(BoundingBoxHashTest, EqualBoxesfHashEqual) {
    std::hash<BoundingBoxf> h;
    BoundingBoxf b1(Vector3f(0.0f, 0.0f, 0.0f), Vector3f(1.0f, 1.0f, 1.0f));
    BoundingBoxf b2(Vector3f(0.0f, 0.0f, 0.0f), Vector3f(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(h(b1), h(b2));
  }

  TEST(BoundingBoxHashTest, DifferentBoxesLikelyHashDifferent) {
    std::hash<BoundingBoxd> h;
    BoundingBoxd b1(Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 1.0, 1.0));
    BoundingBoxd b2(Vector3d(1.0, 1.0, 1.0), Vector3d(2.0, 2.0, 2.0));
    EXPECT_NE(h(b1), h(b2));
  }

  // ---------------------------------------------------------------------------
  // Quaternion hash
  // ---------------------------------------------------------------------------

  TEST(QuaternionHashTest, EqualQuaternionsdHashEqual) {
    std::hash<Quaterniond> h;
    Quaterniond q1(1.0, 0.0, 0.0, 0.0);
    Quaterniond q2(1.0, 0.0, 0.0, 0.0);
    EXPECT_EQ(h(q1), h(q2));
  }

  TEST(QuaternionHashTest, EqualQuaternionsfHashEqual) {
    std::hash<Quaternionf> h;
    Quaternionf q1(1.0f, 0.0f, 0.0f, 0.0f);
    Quaternionf q2(1.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(h(q1), h(q2));
  }

  TEST(QuaternionHashTest, DifferentQuaternionsLikelyHashDifferent) {
    std::hash<Quaterniond> h;
    Quaterniond q1(1.0, 0.0, 0.0, 0.0);
    Quaterniond q2(0.0, 1.0, 0.0, 0.0);
    EXPECT_NE(h(q1), h(q2));
  }

  TEST(QuaternionHashTest, UsableAsUnorderedSetKey) {
    std::unordered_set<Quaterniond> s;
    s.insert(Quaterniond(1.0, 0.0, 0.0, 0.0));
    s.insert(Quaterniond(0.0, 1.0, 0.0, 0.0));
    s.insert(Quaterniond(1.0, 0.0, 0.0, 0.0)); // duplicate

    EXPECT_EQ(2u, s.size());
  }

  // ---------------------------------------------------------------------------
  // operator<< smoke tests (always available, even without C++20 <format>)
  // ---------------------------------------------------------------------------

  TEST(FormatterTest, Vector3dStreamable) {
    std::ostringstream oss;
    oss << Vector3d(1.0, 2.0, 3.0);
    EXPECT_EQ("(1, 2, 3)", oss.str());
  }

  TEST(FormatterTest, Matrix3dStreamable) {
    std::ostringstream oss;
    oss << Matrix3d(); // default ctor produces identity
    // Row-major output: "1 0 0 \n0 1 0 \n0 0 1 \n"
    EXPECT_NE("", oss.str());
  }

  TEST(FormatterTest, BoundingBoxdStreamable) {
    std::ostringstream oss;
    oss << BoundingBoxd(Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 1.0, 1.0));
    EXPECT_EQ("(0, 0, 0)-(1, 1, 1)", oss.str());
  }

  TEST(FormatterTest, QuaterniondStreamable) {
    std::ostringstream oss;
    oss << Quaterniond(1.0, 0.0, 0.0, 0.0);
    EXPECT_EQ("[1, 0 0 0]", oss.str());
  }

} // namespace HashFormatterTest
