#include "test/helpers/ShapeRecognition.h"
#include "core/Color.h"

using namespace testing;
using namespace std;

vector<int> ShapeRecognition::lines(const Buffer<unsigned int>& buffer) const {
  vector<int> lineLengths;
  unsigned int red = Colord(1, 0, 0).rgb();
  
  for (int j = 0; j != buffer.height(); ++j) {
    int start = -1;
    for (int i = 0; i != buffer.width(); ++i) {
      if (start == -1 && buffer[j][i] == red) {
        start = i;
        break;
      }
    }

    int end = -1;
    for (int i = buffer.width() - 1; i != start; --i) {
      if (end == -1 && buffer[j][i] == red) {
        end = i;
        break;
      }
    }
    
    if (start != -1 && end != -1) {
      lineLengths.push_back(end - start);
    }
  }
  
  return lineLengths;
}

bool ShapeRecognition::recognizeRect(const Buffer<unsigned int>& buffer) const {
  vector<int> l = lines(buffer);
  if (l.size() == 0)
    return false;
  
  int length = l.front();
  for (auto& i : l) {
    if (i != length)
      return false;
  }
  return true;
}

bool ShapeRecognition::recognizeCircle(const Buffer<unsigned int>& buffer) const {
  vector<int> l = lines(buffer);
  if (l.size() == 0)
    return false;
  
  int diameter = l.size();
  int radius = diameter / 2;
  
  int index = 0;
  int previous = -1;
  for (auto& i : l) {
    if (index < radius && i < previous) {
      return false;
    } else if (index > diameter - radius && i > previous) {
      return false;
    }
    previous = i;
    index++;
  }
  
  // Make sure that rectangles are not recognized as circles
  if (diameter > 5) {
    if (l.front() >= l[radius] || l.back() >= l[radius])
      return false;
  }
  
  return true;
}
