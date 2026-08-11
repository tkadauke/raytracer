#include "core/formats/threemf/ThreeMfPackage.h"

#include "core/formats/BinaryRead.h"

#include <QFile>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <zlib.h>

namespace core::threemf {
  namespace {
    constexpr std::uint32_t endOfCentralDirectorySignature = 0x06054b50;
    constexpr std::uint32_t centralDirectoryHeaderSignature = 0x02014b50;
    constexpr std::uint32_t localFileHeaderSignature = 0x04034b50;

    std::uint16_t read16(const QByteArray& bytes, qsizetype offset) {
      if (offset < 0 || offset + 2 > bytes.size())
        throw ThreeMfPackageError("Unexpected end of ZIP data");
      return core::formats::readUint16Le(bytes, offset);
    }

    std::uint32_t read32(const QByteArray& bytes, qsizetype offset) {
      if (offset < 0 || offset + 4 > bytes.size())
        throw ThreeMfPackageError("Unexpected end of ZIP data");
      return core::formats::readUint32Le(bytes, offset);
    }

    qsizetype findEndOfCentralDirectory(const QByteArray& bytes) {
      const qsizetype maxComment = 0xffff;
      const qsizetype first = std::max<qsizetype>(0, bytes.size() - maxComment - 22);
      for (qsizetype offset = bytes.size() - 22; offset >= first; --offset) {
        if (read32(bytes, offset) == endOfCentralDirectorySignature)
          return offset;
      }
      throw ThreeMfPackageError("3MF ZIP container is missing a central directory");
    }

    QByteArray inflateRaw(const QByteArray& compressed, std::uint32_t uncompressedSize) {
      QByteArray output;
      output.resize(static_cast<qsizetype>(uncompressedSize));

      z_stream stream{};
      stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));
      stream.avail_in = static_cast<uInt>(compressed.size());
      stream.next_out = reinterpret_cast<Bytef*>(output.data());
      stream.avail_out = static_cast<uInt>(output.size());

      if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        throw ThreeMfPackageError("Unable to initialize ZIP deflate decoder");

      const int result = inflate(&stream, Z_FINISH);
      inflateEnd(&stream);
      if (result != Z_STREAM_END || stream.total_out != uncompressedSize)
        throw ThreeMfPackageError("Unable to inflate ZIP entry");

      return output;
    }

    QByteArray extractEntry(const QByteArray& bytes, qsizetype localHeaderOffset,
                            std::uint16_t method, std::uint32_t compressedSize,
                            std::uint32_t uncompressedSize) {
      if (read32(bytes, localHeaderOffset) != localFileHeaderSignature)
        throw ThreeMfPackageError("ZIP local file header is invalid");

      const auto nameLength = read16(bytes, localHeaderOffset + 26);
      const auto extraLength = read16(bytes, localHeaderOffset + 28);
      const qsizetype dataOffset = localHeaderOffset + 30 + nameLength + extraLength;
      if (dataOffset < 0 || dataOffset + compressedSize > bytes.size())
        throw ThreeMfPackageError("ZIP entry extends past end of file");

      const QByteArray compressed = bytes.mid(dataOffset, compressedSize);
      if (method == 0)
        return compressed;
      if (method == 8)
        return inflateRaw(compressed, uncompressedSize);

      std::ostringstream message;
      message << "Unsupported ZIP compression method " << method;
      throw ThreeMfPackageError(message.str());
    }
  }

  ThreeMfPackageError::ThreeMfPackageError(const std::string& message)
      : std::runtime_error(message) {
  }

  ThreeMfPackage ThreeMfPackage::read(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
      throw ThreeMfPackageError("Unable to read 3MF package");

    const QByteArray bytes = file.readAll();
    const qsizetype eocd = findEndOfCentralDirectory(bytes);
    const auto entryCount = read16(bytes, eocd + 10);
    qsizetype directoryOffset = read32(bytes, eocd + 16);

    ThreeMfPackage package;
    for (std::uint16_t i = 0; i != entryCount; ++i) {
      if (read32(bytes, directoryOffset) != centralDirectoryHeaderSignature)
        throw ThreeMfPackageError("ZIP central directory entry is invalid");

      const auto flags = read16(bytes, directoryOffset + 8);
      if ((flags & 0x0008) != 0)
        throw ThreeMfPackageError("ZIP data descriptors are not supported in 3MF packages");

      const auto method = read16(bytes, directoryOffset + 10);
      const auto compressedSize = read32(bytes, directoryOffset + 20);
      const auto uncompressedSize = read32(bytes, directoryOffset + 24);
      const auto nameLength = read16(bytes, directoryOffset + 28);
      const auto extraLength = read16(bytes, directoryOffset + 30);
      const auto commentLength = read16(bytes, directoryOffset + 32);
      const auto localHeaderOffset = read32(bytes, directoryOffset + 42);

      const QString name =
        normalizedPartName(QString::fromUtf8(bytes.mid(directoryOffset + 46, nameLength)));
      if (!name.endsWith('/')) {
        package.m_parts[name] =
          extractEntry(bytes, localHeaderOffset, method, compressedSize, uncompressedSize);
      }

      directoryOffset += 46 + nameLength + extraLength + commentLength;
    }

    return package;
  }

  bool ThreeMfPackage::contains(const QString& partName) const {
    return m_parts.find(normalizedPartName(partName)) != m_parts.end();
  }

  QByteArray ThreeMfPackage::part(const QString& partName) const {
    const auto found = m_parts.find(normalizedPartName(partName));
    if (found == m_parts.end())
      throw ThreeMfPackageError("3MF package part is missing");
    return found->second;
  }

  QStringList ThreeMfPackage::partNames() const {
    QStringList names;
    for (const auto& [name, data] : m_parts)
      names.push_back(name);
    return names;
  }

}
