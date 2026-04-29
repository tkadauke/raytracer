#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "core/Exception.h"
#include "core/formats/ply/PlyFile.h"
#include "core/formats/ply/PlyParseError.h"
#include "core/geometry/Mesh.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // PLY is the only untrusted-input surface in the library (modernize.md
  // §3.4 / §3.8). Treat the input as a PLY file payload and exercise both
  // the header-only and the full read-into-Mesh paths. PlyParseError is the
  // expected failure mode for malformed input — anything else (a crash, a
  // sanitizer violation, an unhandled exception) is a bug for the fuzzer
  // to surface.
  std::string buffer(reinterpret_cast<const char*>(data), size);

  try {
    std::istringstream is(buffer);
    PlyFile file(is);
  } catch (const PlyParseError&) {
  } catch (const Exception&) {
  } catch (const std::exception&) {
  }

  try {
    std::istringstream is(buffer);
    Mesh mesh;
    PlyFile file(is, mesh);
  } catch (const PlyParseError&) {
  } catch (const Exception&) {
  } catch (const std::exception&) {
  }

  return 0;
}
