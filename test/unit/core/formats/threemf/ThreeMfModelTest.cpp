#include <gtest/gtest.h>

#include "core/formats/threemf/ThreeMfModel.h"
#include "core/formats/threemf/ThreeMfPackage.h"
#include "test/helpers/ImporterTestHelper.h"

namespace ThreeMfModelTest {

  TEST(ThreeMfPackage, ReadsDeflatedModelPartFromFixture) {
    const auto package =
      core::threemf::ThreeMfPackage::read(test::importers::importerFixturePath("3mf/minimal.3mf"));

    ASSERT_TRUE(package.contains("3D/3dmodel.model"));
    EXPECT_TRUE(package.part("3D/3dmodel.model").contains("<model"));
  }

  TEST(ThreeMfModelParser, ParsesUnitsMaterialsMeshesBuildItemsAndTransforms) {
    const auto package =
      core::threemf::ThreeMfPackage::read(test::importers::importerFixturePath("3mf/minimal.3mf"));

    const auto model = core::threemf::ThreeMfModelParser().parse(package.part("3D/3dmodel.model"));

    EXPECT_EQ(QString("millimeter"), model.unitName());
    EXPECT_DOUBLE_EQ(0.001, model.unitScaleInMeters());
    ASSERT_EQ(1u, model.materials.size());
    EXPECT_EQ(Colord(1.0, 0.0, 0.0), model.materials.begin()->second.color);

    ASSERT_EQ(1u, model.objects.size());
    const auto& object = model.objects.begin()->second;
    EXPECT_EQ(3u, object.mesh.vertices().size());
    EXPECT_EQ(1u, object.mesh.faces().size());
    ASSERT_EQ(1u, object.faceMaterials.size());
    ASSERT_TRUE(object.faceMaterials[0].has_value());
    EXPECT_EQ(5, object.faceMaterials[0]->id);
    EXPECT_EQ(0, object.faceMaterials[0]->index);

    ASSERT_EQ(1u, model.buildItems.size());
    const auto& transform = model.buildItems[0].transform;
    EXPECT_DOUBLE_EQ(100.0, transform.cell(0, 3));
    EXPECT_DOUBLE_EQ(200.0, transform.cell(1, 3));
    EXPECT_DOUBLE_EQ(300.0, transform.cell(2, 3));
  }

}
