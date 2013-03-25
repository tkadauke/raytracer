#include "test/helpers/ShapeRecognition.h"
#include "core/Color.h"

using namespace testing;
using namespace std;

vector<int> ShapeRecognition::lines(const Buffer<unsigned int>& buffer) {
  vector<int> lineLengths;
  unsigned int red = Colord(1, 0, 0).rgb();
  
  for (int j = 0; j != buffer.height(); ++j) {
    int start = -1;
    for (int i = 0; i != buffer.width(); ++i) {
      if (start == -1 && buffer[j][i] == red) {
        start = i;
      } else if (start != -1 && buffer[j][i] != red) {
        lineLengths.push_back(i - start);
        break;
      }
    }
  }
  
  return lineLengths;
}

bool ShapeRecognition::recognizeRect(const Buffer<unsigned int>& buffer) {
  vector<int> l = lines(buffer);
  if (l.size() == 0)
    return false;
  
  int length = l.front();
  for (vector<int>::iterator i = l.begin(); i != l.end(); ++i) {
    if (*i != length)
      return false;
  }
  return true;
}

bool ShapeRecognition::recognizeCircle(const Buffer<unsigned int>& buffer) {
  vector<int> l = lines(buffer);
  if (l.size() == 0)
    return false;
  
  int diameter = l.size();
  int radius = diameter / 2;
  
  int index = 0;
  int previous = -1;
  for (vector<int>::iterator i = l.begin(); i != l.end(); ++i, ++index) {
    if (index < radius && *i < previous) {
      return false;
    } else if (index > diameter - radius && *i > previous) {
      return false;
    }
    previous = *i;
  }
  
  // Make sure that rectangles are not recognized as circles
  if (diameter > 5) {
    if (l.front() >= l[radius] || l.back() >= l[radius])
      return false;
  }
  
  return true;
}
