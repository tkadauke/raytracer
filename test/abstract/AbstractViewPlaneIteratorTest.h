#ifndef ABSTRACT_VIEW_PLANE_ITERATOR_TEST_H
#define ABSTRACT_VIEW_PLANE_ITERATOR_TEST_H

#include "gtest.h"

namespace testing {
  template<class VP>
  struct AbstractViewPlaneIteratorTest : public Test {
    inline virtual void SetUp() {
      fullRect = Rect(0, 0, 8, 6);
      quarterRect = Rect(0, 0, 4, 3);
      plane.setup(Matrix4d(), fullRect);
    }
  
    VP plane;
    Rect fullRect;
    Rect quarterRect;
  };

  TYPED_TEST_CASE_P(AbstractViewPlaneIteratorTest);

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldAdvance) {
    typename TypeParam::Iterator iterator = this->plane.begin(this->fullRect);
    ++iterator;
    ASSERT_NE(*this->plane.begin(this->fullRect), *iterator);
  }

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldEndUpInLastRowAfterIterating) {
    typename TypeParam::Iterator iterator = this->plane.begin(this->fullRect);
    for (; iterator != this->plane.end(this->fullRect); ++iterator) {}
    ASSERT_EQ(this->plane.height(), iterator.row());
  }

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldEndUpInColumnZeroAfterIterating) {
    typename TypeParam::Iterator iterator = this->plane.begin(this->fullRect);
    for (; iterator != this->plane.end(this->fullRect); ++iterator) {}
    ASSERT_EQ(0, iterator.column());
  }

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldIterateThroughPixels) {
    int count = 0;
    for (typename TypeParam::Iterator iterator = this->plane.begin(this->fullRect); iterator != this->plane.end(this->fullRect); ++iterator) {
      ++count;
    }
    ASSERT_EQ(8*6, count);
  }
  
  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldIterateThroughRect) {
    int count = 0;
    for (typename TypeParam::Iterator iterator = this->plane.begin(this->quarterRect); iterator != this->plane.end(this->quarterRect); ++iterator) {
      ++count;
    }
    ASSERT_EQ(3*4, count);
  }

  TYPED_TEST_P(AbstractViewPlaneIteratorTest, ShouldPointAtEndIteratorAfterIterating) {
    typename TypeParam::Iterator iterator = this->plane.begin(this->fullRect);
    for (; iterator != this->plane.end(this->fullRect); ++iterator) {}
    ASSERT_EQ(this->plane.end(this->fullRect).row(), iterator.row());
    ASSERT_EQ(this->plane.end(this->fullRect).column(), iterator.column());
  }

  REGISTER_TYPED_TEST_CASE_P(
    AbstractViewPlaneIteratorTest,
    ShouldAdvance,
    ShouldEndUpInLastRowAfterIterating,
    ShouldEndUpInColumnZeroAfterIterating,
    ShouldIterateThroughPixels,
    ShouldIterateThroughRect,
    ShouldPointAtEndIteratorAfterIterating
  );
}

#endif
