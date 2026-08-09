#include "core/formats/ldraw/LDrawCommand.h"

#include <algorithm>
#include <array>

Matrix4d LDrawSubfileReference::toMatrix() const {
  const auto& m = matrix;
  return Matrix4d(m[0], m[1], m[2], translation.x(), m[3], m[4], m[5], translation.y(), m[6],
                  m[7], m[8], translation.z(), 0.0, 0.0, 0.0, 1.0);
}

bool LDrawMetaCommand::isComment() const {
  return keyword.empty();
}

bool LDrawMetaCommand::isGeometryDirective() const {
  return keyword == "BFC" || keyword == "STEP" || keyword == "!COLOUR";
}

bool LDrawMetaCommand::isInformational() const {
  if (isComment())
    return true;

  static constexpr std::array<const char*, 13> metadataKeywords = {
    "FILE",  "NOFILE",    "Name:",     "Author:", "!LDRAW_ORG", "!LICENSE", "!HISTORY",
    "!HELP", "!CATEGORY", "!KEYWORDS", "!THEME",  "!CMDLINE",   "!PREVIEW",
  };
  if (std::find(metadataKeywords.begin(), metadataKeywords.end(), keyword) !=
      metadataKeywords.end()) {
    return true;
  }

  return keyword[0] != '!' && !isGeometryDirective();
}
