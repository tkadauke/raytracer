#include <gtest/gtest.h>
#include "core/math/Vector.h"
#include "core/math/Matrix.h"
#include "core/math/Quaternion.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace HashFormatterTest {

// --- std::hash: Vector3 ---

TEST(Vector3HashTest, DistinctVectorsProduceDistinctHashes) {
  std::hash<Vector3<float>> h;
  EXPECT_NE(h(Vector3<float>(1, 0, 0)), h(Vector3<float>(0, 1, 0)));
  EXPECT_NE(h(Vector3<float>(1, 0, 0)), h(Vector3<float>(0, 0, 1)));
  EXPECT_NE(h(Vector3<float>(0, 1, 0)), h(Vector3<float>(0, 0, 1)));
}

TEST(Vector3HashTest, EqualVectorsProduceSameHash) {
  std::hash<Vector3<float>> h;
  EXPECT_EQ(h(Vector3<float>(1, 2, 3)), h(Vector3<float>(1, 2, 3)));
}

TEST(Vector3HashTest, WorksAsUnorderedMapKey) {
  std::unordered_map<Vector3<float>, int> map;
  map[Vector3<float>(1, 0, 0)] = 1;
  map[Vector3<float>(0, 1, 0)] = 2;
  map[Vector3<float>(0, 0, 1)] = 3;

  EXPECT_EQ(map[Vector3<float>(1, 0, 0)], 1);
  EXPECT_EQ(map[Vector3<float>(0, 1, 0)], 2);
  EXPECT_EQ(map[Vector3<float>(0, 0, 1)], 3);
  EXPECT_EQ(map.size(), 3u);
}

TEST(Vector3HashTest, WorksAsUnorderedSetElement) {
  std::unordered_set<Vector3<double>> set;
  set.insert(Vector3<double>(1, 0, 0));
  set.insert(Vector3<double>(0, 1, 0));
  set.insert(Vector3<double>(1, 0, 0));  // duplicate

  EXPECT_EQ(set.size(), 2u);
}

TEST(Vector3HashTest, HashDistributionForTypicalPopulation) {
  std::unordered_set<Vector3<float>> set;
  const float vals[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
  for (float x : vals)
    for (float y : vals)
      for (float z : vals)
        set.insert(Vector3<float>(x, y, z));

  // 125 distinct vectors should produce at least 100 distinct buckets
  EXPECT_GE(set.size(), 100u);
}

// --- std::hash: Vector4 ---

TEST(Vector4HashTest, DistinctVectorsProduceDistinctHashes) {
  std::hash<Vector4<float>> h;
  EXPECT_NE(h(Vector4<float>(1, 0, 0, 0)), h(Vector4<float>(0, 1, 0, 0)));
  EXPECT_NE(h(Vector4<float>(1, 0, 0, 0)), h(Vector4<float>(0, 0, 1, 0)));
  EXPECT_NE(h(Vector4<float>(1, 0, 0, 0)), h(Vector4<float>(0, 0, 0, 1)));
}

TEST(Vector4HashTest, EqualVectorsProduceSameHash) {
  std::hash<Vector4<double>> h;
  EXPECT_EQ(h(Vector4<double>(1, 2, 3, 4)), h(Vector4<double>(1, 2, 3, 4)));
}

TEST(Vector4HashTest, WorksAsUnorderedMapKey) {
  std::unordered_map<Vector4<float>, std::string> map;
  map[Vector4<float>(1, 0, 0, 0)] = "x";
  map[Vector4<float>(0, 1, 0, 0)] = "y";
  map[Vector4<float>(0, 0, 1, 0)] = "z";
  map[Vector4<float>(0, 0, 0, 1)] = "w";

  EXPECT_EQ(map[Vector4<float>(1, 0, 0, 0)], "x");
  EXPECT_EQ(map[Vector4<float>(0, 0, 0, 1)], "w");
  EXPECT_EQ(map.size(), 4u);
}

// --- std::hash: Matrix4 ---

TEST(Matrix4HashTest, IdentityMatrixHashIsStable) {
  std::hash<Matrix4<float>> h;
  EXPECT_EQ(h(Matrix4<float>()), h(Matrix4<float>()));
}

TEST(Matrix4HashTest, DistinctMatricesProduceDistinctHashes) {
  std::hash<Matrix4<float>> h;
  Matrix4<float> identity;
  Matrix4<float> translated = Matrix4<float>::translate(1, 0, 0);
  EXPECT_NE(h(identity), h(translated));
}

TEST(Matrix4HashTest, WorksAsUnorderedMapKey) {
  std::unordered_map<Matrix4<double>, int> map;
  Matrix4<double> id;
  Matrix4<double> tx = Matrix4<double>::translate(1, 0, 0);
  Matrix4<double> ty = Matrix4<double>::translate(0, 1, 0);

  map[id] = 0;
  map[tx] = 1;
  map[ty] = 2;

  EXPECT_EQ(map[id], 0);
  EXPECT_EQ(map[tx], 1);
  EXPECT_EQ(map[ty], 2);
  EXPECT_EQ(map.size(), 3u);
}

// --- std::hash: Quaternion ---

TEST(QuaternionHashTest, IdentityQuaternionHashIsStable) {
  std::hash<Quaternion<float>> h;
  EXPECT_EQ(h(Quaternion<float>()), h(Quaternion<float>()));
}

TEST(QuaternionHashTest, DistinctQuaternionsProduceDistinctHashes) {
  std::hash<Quaternion<float>> h;
  EXPECT_NE(h(Quaternion<float>(1, 0, 0, 0)), h(Quaternion<float>(0, 1, 0, 0)));
  EXPECT_NE(h(Quaternion<float>(1, 0, 0, 0)), h(Quaternion<float>(0, 0, 1, 0)));
  EXPECT_NE(h(Quaternion<float>(1, 0, 0, 0)), h(Quaternion<float>(0, 0, 0, 1)));
}

TEST(QuaternionHashTest, EqualQuaternionsProduceSameHash) {
  std::hash<Quaternion<double>> h;
  EXPECT_EQ(h(Quaternion<double>(1, 2, 3, 4)), h(Quaternion<double>(1, 2, 3, 4)));
}

TEST(QuaternionHashTest, WorksAsUnorderedSetElement) {
  std::unordered_set<Quaternion<float>> set;
  set.insert(Quaternion<float>(1, 0, 0, 0));
  set.insert(Quaternion<float>(0, 1, 0, 0));
  set.insert(Quaternion<float>(0, 0, 1, 0));
  set.insert(Quaternion<float>(1, 0, 0, 0));  // duplicate

  EXPECT_EQ(set.size(), 3u);
}

// --- std::formatter tests (C++20 only) ---

#if __cplusplus >= 202002L && __has_include(<format>)
#include <format>
#ifdef __cpp_lib_format

TEST(Vector3FormatterTest, FormatsVector3Float) {
  Vector3<float> v(1.0f, 2.0f, 3.0f);
  EXPECT_EQ(std::format("{}", v), "(1, 2, 3)");
}

TEST(Vector3FormatterTest, FormatsZeroVector) {
  Vector3<float> v(0.0f, 0.0f, 0.0f);
  EXPECT_EQ(std::format("{}", v), "(0, 0, 0)");
}

TEST(Vector4FormatterTest, FormatsVector4Float) {
  Vector4<float> v(1.0f, 2.0f, 3.0f, 4.0f);
  EXPECT_EQ(std::format("{}", v), "(1, 2, 3, 4)");
}

TEST(Matrix4FormatterTest, FormatsIdentityMatrix4) {
  Matrix4<float> m;
  std::string result = std::format("{}", m);
  // Identity: each row is "1 0 0 0 \n" etc.
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("1"), std::string::npos);
}

TEST(QuaternionFormatterTest, FormatsIdentityQuaternion) {
  Quaternion<float> q;
  EXPECT_EQ(std::format("{}", q), "[1, 0 0 0]");
}

TEST(QuaternionFormatterTest, FormatsArbitraryQuaternion) {
  Quaternion<float> q(4, 1, 2, 3);
  EXPECT_EQ(std::format("{}", q), "[4, 1 2 3]");
}

#endif  // __cpp_lib_format
#endif  // __cplusplus >= 202002L

}  // namespace HashFormatterTest
