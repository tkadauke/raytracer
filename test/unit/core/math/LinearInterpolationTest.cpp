#include "gtest.h"
#include "core/math/Vector.h"
#include "core/math/LinearInterpolation.h"
#include "test/helpers/VectorTestHelper.h"

#include <list>

namespace LinearInterpolationTest {
  template<class T>
  struct LinearInterpolationTest : public ::testing::Test {
    LinearInterpolationTest()
      : begin(1), end(2) {}
    T begin, end;
  };

  template<class T>
  struct LinearInterpolationTest<Vector2<T>> : public ::testing::Test {
    LinearInterpolationTest()
      : begin(1, 0), end(2, 1) {}
    Vector2<T> begin, end;
  };

  template<class T>
  struct LinearInterpolationTest<Vector3<T>> : public ::testing::Test {
    LinearInterpolationTest()
      : begin(1, 0, 0), end(2, 1, 0) {}
    Vector3<T> begin, end;
  };

  template<class T>
  struct LinearInterpolationTest<Vector4<T>> : public ::testing::Test {
    LinearInterpolationTest()
      : begin(1, 0, 0), end(2, 1, 0) {}
    Vector4<T> begin, end;
  };

  typedef ::testing::Types<
    Vector2<float>, Vector3<float>, Vector4<float>, float,
    Vector2<double>, Vector3<double>, Vector4<double>, double
  > LinearInterpolationTypes;
  
  TYPED_TEST_CASE(LinearInterpolationTest, LinearInterpolationTypes);

  TYPED_TEST(LinearInterpolationTest, ShouldReturnFirstVectorAsBeginIterator) {
    LinearInterpolation<TypeParam> interpolation(this->begin, this->end, 10);

    auto iter = interpolation.begin();
    ASSERT_EQ(this->begin, *iter);
  }

  TYPED_TEST(LinearInterpolationTest, ShouldReturnLastVectorAsEndIterator) {
    LinearInterpolation<TypeParam> interpolation(this->begin, this->end, 10);

    auto iter = interpolation.end();
    ASSERT_EQ(this->end, *iter);
  }

  TYPED_TEST(LinearInterpolationTest, ShouldReturnSameIteratorWhenPreIncrementing) {
    LinearInterpolation<TypeParam> interpolation(this->begin, this->end, 10);

    auto iter = interpolation.begin();
    const auto& incremented = ++iter;

    ASSERT_EQ(&iter, &incremented);
  }

  TYPED_TEST(LinearInterpolationTest, ShouldReturnNewIteratorWhenPostIncrementing) {
    LinearInterpolation<TypeParam> interpolation(this->begin, this->end, 10);

    auto iter = interpolation.begin();
    const auto& incremented = iter++;

    ASSERT_NE(&iter, &incremented);
  }

  TYPED_TEST(LinearInterpolationTest, ShouldCompareEqualIterators) {
    LinearInterpolation<TypeParam> interpolation(this->begin, this->end, 10);

    ASSERT_TRUE(interpolation.begin() == interpolation.begin());
    ASSERT_TRUE(interpolation.end() == interpolation.end());
  }

  TYPED_TEST(LinearInterpolationTest, ShouldCompareDifferentIterators) {
    LinearInterpolation<TypeParam> interpolation(this->begin, this->end, 10);

    ASSERT_TRUE(interpolation.begin() != interpolation.end());
  }

  TYPED_TEST(LinearInterpolationTest, ShouldYieldEveryInterpolatedVectorWhenIteratingFromBeginToEnd) {
    LinearInterpolation<TypeParam> interpolation(this->begin, this->end, 10);

    std::list<TypeParam> results;

    for (auto i : interpolation) {
      results.push_back(i);
    }

    ASSERT_EQ(10u, results.size());
  }
  
  TEST(LinearInterpolation, ShouldPreIncrementIterator) {
    Vector3f begin(1, 0, 0), end(2, 1, 0), expected(1.1, 0.1, 0);
    LinearInterpolation<Vector3f> interpolation(begin, end, 10);
  
    auto iter = interpolation.begin();
    ++iter;
  
    ASSERT_VECTOR_NEAR(expected, *iter, 0.0001);
  }

  TEST(LinearInterpolation, ShouldPostIncrementIterator) {
    Vector3f begin(1, 0, 0), end(2, 1, 0), expected(1.1, 0.1, 0);
    LinearInterpolation<Vector3f> interpolation(begin, end, 10);
  
    auto iter = interpolation.begin();
    iter++;
  
    ASSERT_VECTOR_NEAR(expected, *iter, 0.0001);
  }
  
  TEST(LinearInterpolation, ShouldReturnCurrentStep) {
    Vector3f begin(1, 0, 0), end(2, 1, 0), expected(1.1, 0.1, 0);
    LinearInterpolation<Vector3f> interpolation(begin, end, 10);
  
    auto iter = interpolation.begin();
    ASSERT_EQ(0, iter.current());
    ++iter;
    ASSERT_EQ(1, iter.current());
  }
}
