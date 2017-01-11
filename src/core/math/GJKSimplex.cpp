#include "core/math/GJKSimplex.h"

#include <assert.h>

GJKSimplex::GJKSimplex()
  : m_pointsLengthSquare(),
    m_maxLengthSquare(0),
    m_det(),
    m_normSquare(),
    m_bitsCurrentGJKSimplex(0),
    m_lastFound(0),
    m_lastFoundBit(0),
    m_allBits(0)
{
}

void GJKSimplex::addPoint(const Vector3d& point, const Vector3d& suppPointA, const Vector3d& suppPointB) {
  assert(!isFull());

  m_lastFound = 0;
  m_lastFoundBit = 0x1;

  // Look for the bit corresponding to one of the four point that is not in
  // the current simplex
  while (overlap(m_bitsCurrentGJKSimplex, m_lastFoundBit)) {
    m_lastFound++;
    m_lastFoundBit <<= 1;
  }

  assert(m_lastFound >= 0 && m_lastFound < 4);

  // Add the point into the simplex
  m_points[m_lastFound] = point;
  m_pointsLengthSquare[m_lastFound] = point * point;
  m_allBits = m_bitsCurrentGJKSimplex | m_lastFoundBit;

  // Update the cached values
  updateCache();
    
  // Compute the cached determinant values
  computeDeterminants();
    
  // Add the support points of objects A and B
  m_suppPointsA[m_lastFound] = suppPointA;
  m_suppPointsB[m_lastFound] = suppPointB;
}

bool GJKSimplex::isPointInGJKSimplex(const Vector3d& point) const {
  int i;
  Bits bit;

  // For each four possible points in the simplex
  for (i=0, bit = 0x1; i<4; i++, bit <<= 1) {
    // Check if the current point is in the simplex
    if (overlap(m_allBits, bit) && point == m_points[i])
      return true;
  }

  return false;
}

void GJKSimplex::updateCache() {
  int i;
  Bits bit;

  // For each of the four possible points of the simplex
  for (i=0, bit = 0x1; i<4; i++, bit <<= 1) {
    // If the current points is in the simplex
    if (overlap(m_bitsCurrentGJKSimplex, bit)) {
      // Compute the distance between two points in the possible simplex set
      m_diffLength[i][m_lastFound] = m_points[i] - m_points[m_lastFound];
      m_diffLength[m_lastFound][i] = -m_diffLength[i][m_lastFound];

      // Compute the squared length of the vector
      // distances from points in the possible simplex set
      m_normSquare[i][m_lastFound] = m_normSquare[m_lastFound][i] =
        m_diffLength[i][m_lastFound] * m_diffLength[i][m_lastFound];
    }
  }
}

void GJKSimplex::computeDeterminants() {
  m_det[m_lastFoundBit][m_lastFound] = 1.0;

  // If the current simplex is not empty
  if (!isEmpty()) {
    int i;
    Bits bitI;

    // For each possible four points in the simplex set
    for (i=0, bitI = 0x1; i<4; i++, bitI <<= 1) {
      // If the current point is in the simplex
      if (overlap(m_bitsCurrentGJKSimplex, bitI)) {
        Bits bit2 = bitI | m_lastFoundBit;

        m_det[bit2][i] = m_diffLength[m_lastFound][i] * m_points[m_lastFound];
        m_det[bit2][m_lastFound] = m_diffLength[i][m_lastFound] * m_points[i];

        int j;
        Bits bitJ;

        for (j=0, bitJ = 0x1; j<i; j++, bitJ <<= 1) {
          if (overlap(m_bitsCurrentGJKSimplex, bitJ)) {
            int k;
            Bits bit3 = bitJ | bit2;

            k = m_normSquare[i][j] < m_normSquare[m_lastFound][j] ? i : m_lastFound;
            m_det[bit3][j] = m_det[bit2][i] * m_diffLength[k][j] * m_points[i] +
                             m_det[bit2][m_lastFound] *
                             m_diffLength[k][j] * m_points[m_lastFound];

            k = m_normSquare[j][i] < m_normSquare[m_lastFound][i] ? j : m_lastFound;
            m_det[bit3][i] = m_det[bitJ | m_lastFoundBit][j] *
                             m_diffLength[k][i] * m_points[j] +
                             m_det[bitJ | m_lastFoundBit][m_lastFound] *
                             m_diffLength[k][i] * m_points[m_lastFound];

            k = m_normSquare[i][m_lastFound] < m_normSquare[j][m_lastFound] ? i : j;
            m_det[bit3][m_lastFound] = m_det[bitJ | bitI][j] *
                                       m_diffLength[k][m_lastFound] * m_points[j] +
                                       m_det[bitJ | bitI][i] *
                                       m_diffLength[k][m_lastFound] * m_points[i];
          }
        }
      }
    }

    if (m_allBits == 0xf) {
      int k;

      k = m_normSquare[1][0] < m_normSquare[2][0] ?
         (m_normSquare[1][0] < m_normSquare[3][0] ? 1 : 3) :
         (m_normSquare[2][0] < m_normSquare[3][0] ? 2 : 3);
      m_det[0xf][0] = m_det[0xe][1] * m_diffLength[k][0] * m_points[1] +
                      m_det[0xe][2] * m_diffLength[k][0] * m_points[2] +
                      m_det[0xe][3] * m_diffLength[k][0] * m_points[3];

      k = m_normSquare[0][1] < m_normSquare[2][1] ?
         (m_normSquare[0][1] < m_normSquare[3][1] ? 0 : 3) :
         (m_normSquare[2][1] < m_normSquare[3][1] ? 2 : 3);
      m_det[0xf][1] = m_det[0xd][0] * m_diffLength[k][1] * m_points[0] +
                      m_det[0xd][2] * m_diffLength[k][1] * m_points[2] +
                      m_det[0xd][3] * m_diffLength[k][1] * m_points[3];

      k = m_normSquare[0][2] < m_normSquare[1][2] ?
         (m_normSquare[0][2] < m_normSquare[3][2] ? 0 : 3) :
         (m_normSquare[1][2] < m_normSquare[3][2] ? 1 : 3);
      m_det[0xf][2] = m_det[0xb][0] * m_diffLength[k][2] * m_points[0] +
                      m_det[0xb][1] * m_diffLength[k][2] * m_points[1] +
                      m_det[0xb][3] * m_diffLength[k][2] * m_points[3];

      k = m_normSquare[0][3] < m_normSquare[1][3] ?
         (m_normSquare[0][3] < m_normSquare[2][3] ? 0 : 2) :
         (m_normSquare[1][3] < m_normSquare[2][3] ? 1 : 2);
      m_det[0xf][3] = m_det[0x7][0] * m_diffLength[k][3] * m_points[0] +
                      m_det[0x7][1] * m_diffLength[k][3] * m_points[1] +
                      m_det[0x7][2] * m_diffLength[k][3] * m_points[2];
    }
  }
}

bool GJKSimplex::isValidSubset(Bits subset) const {
  int i;
  Bits bit;

  // For each four point in the possible simplex set
  for (i=0, bit=0x1; i<4; i++, bit <<= 1) {
    if (overlap(m_allBits, bit)) {
      // If the current point is in the subset
      if (overlap(subset, bit)) {
        // If one delta(X)_i is smaller or equal to zero
        if (m_det[subset][i] <= 0.0) {
          // The subset is not valid
          return false;
        }
      }
      // If the point is not in the subset and the value delta(X U {y_j})_j
      // is bigger than zero
      else if (m_det[subset | bit][i] > 0.0) {
        // The subset is not valid
        return false;
      }
    }
  }

  return true;
}

void GJKSimplex::computeClosestPointsOfAandB(Vector3d& pA, Vector3d& pB) const {
  double deltaX = 0.0;
  pA = Vector3d::null();
  pB = Vector3d::null();
  int i;
  Bits bit;

  // For each four points in the possible simplex set
  for (i=0, bit=0x1; i<4; i++, bit <<= 1) {
    // If the current point is part of the simplex
    if (overlap(m_bitsCurrentGJKSimplex, bit)) {
      deltaX += m_det[m_bitsCurrentGJKSimplex][i];
      pA += m_det[m_bitsCurrentGJKSimplex][i] * m_suppPointsA[i];
      pB += m_det[m_bitsCurrentGJKSimplex][i] * m_suppPointsB[i];
    }
  }

  assert(deltaX > 0.0);
  double factor = double(1.0) / deltaX;
  pA *= factor;
  pB *= factor;
}

bool GJKSimplex::computeClosestPoint(Vector3d& v) {
  Bits subset;

  // For each possible simplex set
  for (subset=m_bitsCurrentGJKSimplex; subset != 0; subset--) {
    // If the simplex is a subset of the current simplex and is valid for the Johnson's
    // algorithm test
    if (isSubset(subset, m_bitsCurrentGJKSimplex) && isValidSubset(subset | m_lastFoundBit)) {
      // Add the last added point to the current simplex
      m_bitsCurrentGJKSimplex = subset | m_lastFoundBit;
      // Compute the closest point in the simplex
      v = computeClosestPointForSubset(m_bitsCurrentGJKSimplex);
      return true;
    }
  }

  // If the simplex that contains only the last added point is valid for the Johnson's algorithm test
  if (isValidSubset(m_lastFoundBit)) {
    // Set the current simplex to the set that contains only the last added point
    m_bitsCurrentGJKSimplex = m_lastFoundBit;
    // Update the maximum square length
    m_maxLengthSquare = m_pointsLengthSquare[m_lastFound];
    // The closest point of the simplex "v" is the last added point
    v = m_points[m_lastFound];
    return true;
  }

  // The algorithm failed to found a point
  return false;
}

Vector3d GJKSimplex::computeClosestPointForSubset(Bits subset) {
  // Closest point v = sum(lambda_i * points[i])
  Vector3d v;
  m_maxLengthSquare = 0.0;
  // deltaX = sum of all det[subset][i]
  double deltaX = 0.0;
  int i;
  Bits bit;

  // For each four point in the possible simplex set
  for (i=0, bit=0x1; i<4; i++, bit <<= 1) {
    // If the current point is in the subset
    if (overlap(subset, bit)) {
      // deltaX = sum of all det[subset][i]
      deltaX += m_det[subset][i];

      if (m_maxLengthSquare < m_pointsLengthSquare[i]) {
        m_maxLengthSquare = m_pointsLengthSquare[i];
      }

      // Closest point v = sum(lambda_i * points[i])
      v += m_det[subset][i] * m_points[i];
    }
  }

  assert(deltaX > 0.0);

  // Return the closest point "v" in the convex hull for the given subset
  return (double(1.0) / deltaX) * v;
}
