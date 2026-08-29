#include <gtest/gtest.h>

#include "core/formats/BinaryRead.h"

#include <QByteArray>

#include <array>
#include <cstdint>

namespace BinaryReadTest {

  TEST(BinaryRead, ReadsVector2fLittleEndianFromPointer) {
    const std::array<std::uint8_t, 8> bytes{
      0x00, 0x00, 0xc0, 0x3f, // 1.5f
      0x00, 0x00, 0x20, 0xc0, // -2.5f
    };

    const Vector2d value = core::formats::readVector2fLe(bytes.data());

    EXPECT_EQ(Vector2d(1.5, -2.5), value);
  }

  TEST(BinaryRead, ReadsVector3fLittleEndianFromPointer) {
    const std::array<std::uint8_t, 12> bytes{
      0x00, 0x00, 0xc0, 0x3f, // 1.5f
      0x00, 0x00, 0x20, 0xc0, // -2.5f
      0x00, 0x00, 0x88, 0x40, // 4.25f
    };

    const Vector3d value = core::formats::readVector3fLe(bytes.data());

    EXPECT_EQ(Vector3d(1.5, -2.5, 4.25), value);
  }

  TEST(BinaryRead, ReadsVector3fLittleEndianFromQByteArrayOffset) {
    const std::array<std::uint8_t, 16> bytes{
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xc0, 0x3f, // 1.5f
      0x00, 0x00, 0x20, 0xc0,                         // -2.5f
      0x00, 0x00, 0x88, 0x40,                         // 4.25f
    };
    const QByteArray array(reinterpret_cast<const char*>(bytes.data()),
                           static_cast<qsizetype>(bytes.size()));

    const Vector3d value = core::formats::readVector3fLe(array, 4);

    EXPECT_EQ(Vector3d(1.5, -2.5, 4.25), value);
  }

}
