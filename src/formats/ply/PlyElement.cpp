#include "formats/ply/PlyElement.h"

using namespace std;

PlyElement::PlyElement(istream& is) {
  parse(is);
}

void PlyElement::parse(istream& is) {
  is >> m_name >> m_count;
}
