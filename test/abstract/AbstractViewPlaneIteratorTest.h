#ifndef ABSTRACT_VIEW_PLANE_ITERATOR_TEST_H
#define ABSTRACT_VIEW_PLANE_ITERATOR_TEST_H

#include "gtest.h"

namespace testing {
  template<class VP>
  struct AbstractViewPlaneIteratorTest : public Test {
    virtual void SetUp() {
      plane.setup(Matrix4d(), 8, 6);
    }
  
    VP plane;
  };

  TYPED_TEST_CASE_P(AbstractViewPlaneIteratorTest);

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldAdvance) {
    typename TypeParam::Iterator iterator = this->plane.begin();
    ++iterator;
    ASSERT_NE(*this->plane.begin(), *iterator);
  }

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldEndUpInLastRowAfterIterating) {
    typename TypeParam::Iterator iterator = this->plane.begin();
    for (; iterator != this->plane.end(); ++iterator) {}
    ASSERT_EQ(this->plane.height(), iterator.row());
  }

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldEndUpInColumnZeroAfterIterating) {
    typename TypeParam::Iterator iterator = this->plane.begin();
    for (; iterator != this->plane.end(); ++iterator) {}
    ASSERT_EQ(0, iterator.column());
  }

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldIterateThroughPixels) {
    int count = 0;
    for (typename TypeParam::Iterator iterator = this->plane.begin(); iterator != this->plane.end(); ++iterator) {
      ++count;
    }
    ASSERT_EQ(8*6, count);
  }

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldPointAtEndIteratorAfterIterating) {
    typename TypeParam::Iterator iterator = this->plane.begin();
    for (; iterator != this->plane.end(); ++iterator) {}
    ASSERT_EQ(this->plane.end().row(), iterator.row());
    ASSERT_EQ(this->plane.end().column(), iterator.column());
  }

  REGISTER_TYPED_TEST_CASE_P(AbstractViewPlaneIteratorTest,
                             ShouldAdvance,
                             ShouldEndUpInLastRowAfterIterating,
                             ShouldEndUpInColumnZeroAfterIterating,
                             ShouldIterateThroughPixels,
                             ShouldPointAtEndIteratorAfterIterating);
}

#endif
