#include "core/formats/ldraw/LDrawParseError.h"

#include <sstream>

using namespace std;

LDrawParseError::LDrawParseError(int lineNumber,
                                 const string& detail,
                                 const string& file,
                                 int line)
    : Exception([&]() {
        ostringstream message;
        message << "Parse error in LDraw file at line " << lineNumber << ": " << detail;
        return message.str();
      }(),
                file,
                line),
      m_sourceLineNumber(lineNumber) {
}
