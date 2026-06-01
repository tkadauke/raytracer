#include <QImage>
#include <QImageReader>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

namespace {

  constexpr std::uint64_t fnvOffsetBasis = 14695981039346656037ull;
  constexpr std::uint64_t fnvPrime = 1099511628211ull;

  class ImageReport {
  public:
    struct Stats {
      std::uint64_t nonzeroPixels = 0;
      std::uint64_t warmPixels = 0;
      std::uint64_t coolPixels = 0;
      std::uint64_t neutralPixels = 0;
      std::uint64_t hash = fnvOffsetBasis;
      std::unordered_set<std::uint32_t> uniqueColors;

      void addPixel(const std::array<std::uint8_t, 4>& pixel);
    };

    explicit ImageReport(QImage rgba)
        : m_rgba(std::move(rgba)) {
    }

    static std::unique_ptr<ImageReport> load(const char* path, std::string* errorMessage) {
      QImageReader reader(QString::fromLocal8Bit(path));
      QImage image = reader.read();
      if (image.isNull()) {
        *errorMessage =
          "Unable to read image '" + std::string(path) + "': " + reader.errorString().toStdString();
        return nullptr;
      }

      return std::make_unique<ImageReport>(image.convertToFormat(QImage::Format_RGBA8888));
    }

    int width() const {
      return m_rgba.width();
    }
    int height() const {
      return m_rgba.height();
    }

    Stats stats() const {
      Stats stats;
      stats.hash = fnv1aAppendUint32(stats.hash, static_cast<std::uint32_t>(m_rgba.width()));
      stats.hash = fnv1aAppendUint32(stats.hash, static_cast<std::uint32_t>(m_rgba.height()));

      for (int y = 0; y != m_rgba.height(); ++y) {
        const auto* row = m_rgba.constScanLine(y);
        for (int x = 0; x != m_rgba.width(); ++x) {
          const int offset = x * 4;
          const std::array<std::uint8_t, 4> pixel = {
            row[offset + 0],
            row[offset + 1],
            row[offset + 2],
            row[offset + 3],
          };
          stats.addPixel(pixel);
        }
      }

      return stats;
    }

    void printStats(std::ostream& output) const {
      const Stats imageStats = stats();
      output << "width=" << width() << " height=" << height()
             << " nonzero_pixels=" << imageStats.nonzeroPixels
             << " unique_colors=" << imageStats.uniqueColors.size()
             << " warm_pixels=" << imageStats.warmPixels << " cool_pixels=" << imageStats.coolPixels
             << " neutral_pixels=" << imageStats.neutralPixels << " hash=" << std::hex
             << std::setw(16) << std::setfill('0') << imageStats.hash << std::dec << "\n";
    }

    bool printComparisonTo(const ImageReport& actual, std::ostream& output,
                           std::string* errorMessage) const {
      if (width() != actual.width() || height() != actual.height()) {
        *errorMessage = "Image dimensions differ: expected " + std::to_string(width()) + "x" +
                        std::to_string(height()) + ", actual " + std::to_string(actual.width()) +
                        "x" + std::to_string(actual.height());
        return false;
      }

      double squaredDelta = 0.0;
      double maxDelta = 0.0;
      std::uint64_t differingPixels = 0;
      for (int y = 0; y != height(); ++y) {
        const auto* expectedRow = m_rgba.constScanLine(y);
        const auto* actualRow = actual.m_rgba.constScanLine(y);
        for (int x = 0; x != width(); ++x) {
          const int offset = x * 4;
          bool pixelDiffers = false;
          for (int channel = 0; channel != 3; ++channel) {
            const double delta =
              static_cast<double>(expectedRow[offset + channel] - actualRow[offset + channel]) /
              255.0;
            squaredDelta += delta * delta;
            maxDelta = std::max(maxDelta, std::abs(delta));
            pixelDiffers =
              pixelDiffers || expectedRow[offset + channel] != actualRow[offset + channel];
          }
          if (pixelDiffers)
            ++differingPixels;
        }
      }

      const double sampleCount = static_cast<double>(width()) * static_cast<double>(height()) * 3.0;
      const double rmsDelta = sampleCount > 0.0 ? std::sqrt(squaredDelta / sampleCount) : 0.0;
      output << "width=" << width() << " height=" << height() << " rms_delta=" << std::fixed
             << std::setprecision(10) << rmsDelta << " max_delta=" << maxDelta
             << " differing_pixels=" << differingPixels << "\n";
      return true;
    }

  private:
    QImage m_rgba;

    static std::uint64_t fnv1aAppend(std::uint64_t hash, std::uint8_t byte) {
      hash ^= byte;
      hash *= fnvPrime;
      return hash;
    }

    static std::uint64_t fnv1aAppendUint32(std::uint64_t hash, std::uint32_t value) {
      for (int shift = 0; shift != 32; shift += 8)
        hash = fnv1aAppend(hash, static_cast<std::uint8_t>((value >> shift) & 0xffu));
      return hash;
    }
  };

  void ImageReport::Stats::addPixel(const std::array<std::uint8_t, 4>& pixel) {
    if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0)
      ++nonzeroPixels;
    const auto red = static_cast<int>(pixel[0]);
    const auto green = static_cast<int>(pixel[1]);
    const auto blue = static_cast<int>(pixel[2]);
    const auto brightest = std::max(red, std::max(green, blue));
    const auto darkest = std::min(red, std::min(green, blue));
    if (red > 48 && red > green * 13 / 10 && red > blue * 13 / 10)
      ++warmPixels;
    if (blue > 48 && blue > red * 13 / 10 && blue > green * 11 / 10)
      ++coolPixels;
    if (brightest > 64 && brightest - darkest <= 48)
      ++neutralPixels;
    uniqueColors.insert((static_cast<std::uint32_t>(pixel[0]) << 24u) |
                        (static_cast<std::uint32_t>(pixel[1]) << 16u) |
                        (static_cast<std::uint32_t>(pixel[2]) << 8u) |
                        static_cast<std::uint32_t>(pixel[3]));
    for (const std::uint8_t channel : pixel)
      hash = fnv1aAppend(hash, channel);
  }

  class ImageProbeProgram {
  public:
    int run(int argc, char** argv) const {
      if (argc == 2)
        return reportImage(argv[1]);
      if (argc == 4 && std::string(argv[1]) == "--compare")
        return compareImages(argv[2], argv[3]);

      printUsage(argv[0]);
      return 2;
    }

  private:
    int reportImage(const char* path) const {
      std::string errorMessage;
      const auto image = ImageReport::load(path, &errorMessage);
      if (!image) {
        std::cerr << errorMessage << "\n";
        return 1;
      }

      image->printStats(std::cout);
      return 0;
    }

    int compareImages(const char* expectedPath, const char* actualPath) const {
      std::string errorMessage;
      const auto expected = ImageReport::load(expectedPath, &errorMessage);
      if (!expected) {
        std::cerr << errorMessage << "\n";
        return 1;
      }

      const auto actual = ImageReport::load(actualPath, &errorMessage);
      if (!actual) {
        std::cerr << errorMessage << "\n";
        return 1;
      }

      if (!expected->printComparisonTo(*actual, std::cout, &errorMessage)) {
        std::cerr << errorMessage << "\n";
        return 1;
      }

      return 0;
    }

    void printUsage(const char* program) const {
      std::cerr << "Usage: " << program << " <image>\n"
                << "       " << program << " --compare <expected-image> <actual-image>\n";
    }
  };

} // namespace

int main(int argc, char** argv) {
  return ImageProbeProgram().run(argc, argv);
}
