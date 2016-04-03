#pragma once

#include "core/math/Vector.h"

/**
  * This class implements the GJKSimplex for the GJK algorithm.
  * The implementation of this class is borrowed from
  * https://github.com/DanielChappuis/reactphysics3d
  */
class GJKSimplex {
public:
  /**
    * Default constructor.
    */
  GJKSimplex();

  /**
    * @returns true if the simplex contains all 4 points, false otherwise.
    */
  inline bool isFull() const {
    return (m_bitsCurrentGJKSimplex == 0xf);
  }

  /**
    * @returns true if the simplex is empty, false otherwise.
    */
  inline bool isEmpty() const {
    return (m_bitsCurrentGJKSimplex == 0);
  }

  /**
    * Add a new support point of \f$(A-B)\f$ into the simplex.
    * 
    * @param suppPointA support point of object A in a direction -v
    * @param suppPointB support point of object B in a direction v
    * @param point support point of object (A-B) => point = suppPointA - suppPointB
    */
  void addPoint(const Vector3d& point, const Vector3d& suppPointA, const Vector3d& suppPointB);

  /**
    * @returns true if the point is in the simplex, false otherwise.
    */
  bool isPointInGJKSimplex(const Vector3d& point) const;

  /**
    * Compute the closest points "pA" and "pB" of object A and B.
    */
  void computeClosestPointsOfAandB(Vector3d& pA, Vector3d& pB) const;

  /**
    * Compute the closest point "v" to the origin of the current simplex.
    * This method executes the Jonhnson's algorithm for computing the point
    * "v" of simplex that is closest to the origin. The method returns true
    * if a closest point has been found.
    */
  bool computeClosestPoint(Vector3d& v);

private:
  typedef unsigned int Bits;

  // Returns true if some bits of "a" overlap with bits of "b"
  inline bool overlap(Bits a, Bits b) const {
    return ((a & b) != 0);
  }

  // Returns true if the bits of "b" is a subset of the bits of "a"
  inline bool isSubset(Bits a, Bits b) const {
    return ((a & b) == a);
  }

  // Returns true if the subset is a valid one for the closest point
  // computation.
  // A subset X is valid if :
  //    1. delta(X)_i > 0 for each "i" in I_x and
  //    2. delta(X U {y_j})_j <= 0 for each "j" not in I_x_
  bool isValidSubset(Bits subset) const;

  // Update the cached values used during the GJK algorithm
  void updateCache();

  // Compute the cached determinant values
  void computeDeterminants();

  // Returns the closest point "v" in the convex hull of the points in the
  // subset represented by the bits "subset"
  Vector3d computeClosestPointForSubset(Bits subset);

  Vector3d m_points[4];
  double m_pointsLengthSquare[4];
  double m_maxLengthSquare;
  Vector3d m_suppPointsA[4];
  Vector3d m_suppPointsB[4];
  Vector3d m_diffLength[4][4];
  double m_det[16][4];
  double m_normSquare[4][4];
  Bits m_bitsCurrentGJKSimplex;
  Bits m_lastFound;
  Bits m_lastFoundBit;
  Bits m_allBits;
};
