#include <gtest/gtest.h>

#include "core/geometry/Polyline.h"

#include <string>
#include <vector>

namespace PolylineTest {

  TEST(Polyline, EmptyPolylineHasNoPointsSegmentsOrValidBounds) {
    const core::Polyline polyline;

    EXPECT_TRUE(polyline.empty());
    EXPECT_EQ(0u, polyline.pointCount());
    EXPECT_EQ(0u, polyline.segmentCount());
    EXPECT_EQ(polyline.begin(), polyline.end());
    EXPECT_FALSE(polyline.bounds().isValid());
  }

  TEST(Polyline, SinglePointHasNoSegmentsAndZeroSizeBounds) {
    const core::Polyline polyline({Vector3d(1.0, -2.0, 3.0)});

    EXPECT_FALSE(polyline.empty());
    EXPECT_EQ(1u, polyline.pointCount());
    EXPECT_EQ(0u, polyline.segmentCount());
    EXPECT_EQ(polyline.begin(), polyline.end());
    EXPECT_EQ(BoundingBoxd(Vector3d(1.0, -2.0, 3.0), Vector3d(1.0, -2.0, 3.0)),
              polyline.bounds());
  }

  TEST(Polyline, StoresOrderedThreeDimensionalPoints) {
    const std::vector<Vector3d> points{Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 2.0, 3.0),
                                       Vector3d(-4.0, 5.0, -6.0)};
    const core::Polyline polyline(points);

    ASSERT_EQ(points.size(), polyline.pointCount());
    EXPECT_EQ(points, polyline.points());
    EXPECT_EQ(points[0], polyline.point(0));
    EXPECT_EQ(points[1], polyline.point(1));
    EXPECT_EQ(points[2], polyline.point(2));
  }

  TEST(Polyline, MultiSegmentBoundsIncludeEveryPoint) {
    const core::Polyline polyline({Vector3d(2.0, -3.0, 4.0), Vector3d(-1.0, 5.0, 0.5),
                                   Vector3d(7.0, 1.0, -2.0), Vector3d(0.0, 9.0, 3.0)});

    EXPECT_EQ(BoundingBoxd(Vector3d(-1.0, -3.0, -2.0), Vector3d(7.0, 9.0, 4.0)),
              polyline.bounds());
  }

  TEST(Polyline, IteratesSegmentsInPointOrder) {
    const core::Polyline polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0),
                                   Vector3d(1.0, 2.0, 0.0), Vector3d(1.0, 2.0, 3.0)});

    std::vector<core::Polyline::Segment> segments;
    for (const auto segment : polyline)
      segments.push_back(segment);

    ASSERT_EQ(3u, segments.size());
    EXPECT_EQ(0u, segments[0].index);
    EXPECT_EQ(Vector3d(0.0, 0.0, 0.0), segments[0].start);
    EXPECT_EQ(Vector3d(1.0, 0.0, 0.0), segments[0].end);
    EXPECT_EQ(1u, segments[1].index);
    EXPECT_EQ(Vector3d(1.0, 0.0, 0.0), segments[1].start);
    EXPECT_EQ(Vector3d(1.0, 2.0, 0.0), segments[1].end);
    EXPECT_EQ(2u, segments[2].index);
    EXPECT_EQ(Vector3d(1.0, 2.0, 0.0), segments[2].start);
    EXPECT_EQ(Vector3d(1.0, 2.0, 3.0), segments[2].end);
  }

  TEST(Polyline, StoresTypedCurveAttributes) {
    core::Polyline polyline;
    polyline.setAttribute("name", std::string("toolpath"));
    polyline.setAttribute("visible", true);
    polyline.setAttribute("weight", 2.5);
    polyline.setAttribute("origin", Vector3d(1.0, 2.0, 3.0));

    ASSERT_TRUE(polyline.hasAttribute("name"));
    ASSERT_NE(nullptr, polyline.attribute("visible"));
    ASSERT_NE(nullptr, polyline.attributeAs<std::string>("name"));
    ASSERT_NE(nullptr, polyline.attributeAs<bool>("visible"));
    ASSERT_NE(nullptr, polyline.attributeAs<double>("weight"));
    ASSERT_NE(nullptr, polyline.attributeAs<Vector3d>("origin"));
    EXPECT_EQ("toolpath", *polyline.attributeAs<std::string>("name"));
    EXPECT_TRUE(*polyline.attributeAs<bool>("visible"));
    EXPECT_EQ(2.5, *polyline.attributeAs<double>("weight"));
    EXPECT_EQ(Vector3d(1.0, 2.0, 3.0), *polyline.attributeAs<Vector3d>("origin"));
    EXPECT_EQ(nullptr, polyline.attributeAs<int>("name"));

    EXPECT_TRUE(polyline.removeAttribute("visible"));
    EXPECT_FALSE(polyline.hasAttribute("visible"));
    EXPECT_FALSE(polyline.removeAttribute("missing"));
  }

  TEST(Polyline, StoresIndependentTypedSegmentAttributes) {
    core::Polyline polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0),
                             Vector3d(1.0, 1.0, 0.0)});

    polyline.setSegmentAttribute(0, "feed_rate", 1200);
    polyline.setSegmentAttribute(0, "label", std::string("rapid"));
    polyline.setSegmentAttribute(1, "feed_rate", 300.5);

    ASSERT_NE(nullptr, polyline.segmentAttributeAs<int>(0, "feed_rate"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<std::string>(0, "label"));
    ASSERT_NE(nullptr, polyline.segmentAttributeAs<double>(1, "feed_rate"));
    EXPECT_EQ(1200, *polyline.segmentAttributeAs<int>(0, "feed_rate"));
    EXPECT_EQ("rapid", *polyline.segmentAttributeAs<std::string>(0, "label"));
    EXPECT_EQ(300.5, *polyline.segmentAttributeAs<double>(1, "feed_rate"));
    EXPECT_EQ(nullptr, polyline.segmentAttribute(1, "label"));
    EXPECT_EQ(nullptr, polyline.segmentAttributeAs<double>(0, "feed_rate"));
  }

  TEST(Polyline, SegmentIterationExposesSegmentAttributes) {
    core::Polyline polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0),
                             Vector3d(1.0, 1.0, 0.0)});
    polyline.setSegmentAttribute(1, "kind", std::string("cut"));

    auto it = polyline.begin();
    ++it;
    const auto segment = *it;

    ASSERT_EQ(1u, segment.index);
    ASSERT_EQ(1u, segment.attributes.size());
    ASSERT_NE(segment.attributes.end(), segment.attributes.find("kind"));
    EXPECT_EQ("cut", std::get<std::string>(segment.attributes.find("kind")->second));
  }

  TEST(Polyline, SegmentAttributesFollowSegmentCountWhenPointsChange) {
    core::Polyline polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0),
                             Vector3d(2.0, 0.0, 0.0)});
    polyline.setSegmentAttribute(0, "kept", true);
    polyline.setSegmentAttribute(1, "removed", true);

    polyline.setPoints({Vector3d(3.0, 0.0, 0.0), Vector3d(4.0, 0.0, 0.0)});
    ASSERT_EQ(1u, polyline.segmentCount());
    EXPECT_TRUE(polyline.hasSegmentAttribute(0, "kept"));

    polyline.addPoint(Vector3d(5.0, 0.0, 0.0));
    ASSERT_EQ(2u, polyline.segmentCount());
    EXPECT_TRUE(polyline.segmentAttributes(1).empty());
  }

  TEST(Polyline, SegmentMetadataRejectsMissingSegments) {
    core::Polyline polyline({Vector3d(0.0, 0.0, 0.0)});

    EXPECT_THROW((void)polyline.segmentAttributes(0), std::out_of_range);
    EXPECT_THROW(polyline.setSegmentAttribute(0, "missing", true), std::out_of_range);
    EXPECT_THROW(polyline.removeSegmentAttribute(0, "missing"), std::out_of_range);
    EXPECT_THROW(polyline.clearSegmentAttributes(0), std::out_of_range);
  }

} // namespace PolylineTest
