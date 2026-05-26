#include "core/formats/ldraw/LDrawCommand.h"

#include <algorithm>
#include <array>

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
