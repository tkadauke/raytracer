#include "core/formats/ply/PlyProperty.h"
#include "core/formats/ply/PlyParseError.h"

using namespace std;

PlyProperty::PlyProperty(istream& is)
  : m_list(false)
{
  parse(is);
}

void PlyProperty::parse(istream& is) {
  string type;
  is >> type;
  
  if (type == "list") {
    string countType;
    is >> countType >> type;
    m_countType = mapType(countType);
    m_list = true;
  }
  m_elementType = mapType(type);
  
  is >> m_name;
}

PlyProperty::Type PlyProperty::mapType(const string& type) {
  if (type == "int8" || type == "char") {
    return INT8;
  } else if (type == "int16" || type == "short") {
    return INT16;
  } else if (type == "int32" || type == "int") {
    return INT32;
  } else if (type == "uint8" || type == "uchar") {
    return UINT8;
  } else if (type == "uint16" || type == "ushort") {
    return UINT16;
  } else if (type == "uint32" || type == "uint") {
    return UINT32;
  } else if (type == "float32" || type == "float") {
    return FLOAT32;
  } else if (type == "float64" || type == "double") {
    return FLOAT64;
  }
  throw PlyParseError(__FILE__, __LINE__);
}
