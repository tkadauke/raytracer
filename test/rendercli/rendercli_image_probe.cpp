#include <QImage>
#include <QImageReader>

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {

constexpr std::uint64_t fnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t fnvPrime = 1099511628211ull;

std::uint64_t fnv1aAppend(std::uint64_t hash, std::uint8_t byte) {
  hash ^= byte;
  hash *= fnvPrime;
  return hash;
}

std::uint64_t fnv1aAppendUint32(std::uint64_t hash, std::uint32_t value) {
  for (int shift = 0; shift != 32; shift += 8)
    hash = fnv1aAppend(hash, static_cast<std::uint8_t>((value >> shift) & 0xffu));
  return hash;
}

void printUsage(const char* program) {
  std::cerr << "Usage: " << program << " <image>\n";
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    printUsage(argv[0]);
    return 2;
  }

  QImageReader reader(QString::fromLocal8Bit(argv[1]));
  QImage image = reader.read();
  if (image.isNull()) {
    std::cerr << "Unable to read image '" << argv[1] << "': "
              << reader.errorString().toStdString() << "\n";
    return 1;
  }

  const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
  std::uint64_t nonzeroPixels = 0;
  std::uint64_t hash = fnvOffsetBasis;
  hash = fnv1aAppendUint32(hash, static_cast<std::uint32_t>(rgba.width()));
  hash = fnv1aAppendUint32(hash, static_cast<std::uint32_t>(rgba.height()));

  for (int y = 0; y != rgba.height(); ++y) {
    const auto* row = rgba.constScanLine(y);
    for (int x = 0; x != rgba.width(); ++x) {
      const int offset = x * 4;
      const std::array<std::uint8_t, 4> pixel = {
        row[offset + 0],
        row[offset + 1],
        row[offset + 2],
        row[offset + 3],
      };
      if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0)
        ++nonzeroPixels;
      for (const std::uint8_t channel : pixel)
        hash = fnv1aAppend(hash, channel);
    }
  }

  std::cout << "width=" << rgba.width()
            << " height=" << rgba.height()
            << " nonzero_pixels=" << nonzeroPixels
            << " hash=" << std::hex << std::setw(16) << std::setfill('0') << hash
            << std::dec << "\n";

  return 0;
}
