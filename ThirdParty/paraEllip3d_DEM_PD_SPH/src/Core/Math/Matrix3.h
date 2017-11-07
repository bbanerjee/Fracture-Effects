/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
 * Copyright (c) 2013-2014 Callaghan Innovation, New Zealand
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __ELLIP3D_CORE_MATH_MATRIX3_H__
#define __ELLIP3D_CORE_MATH_MATRIX3_H__

#include <Core/Math/Vec.h>

#include <cmath>

#include <iosfwd>
#include <vector>
#include <string>

namespace dem {

  class Matrix3 {

  private:
    double mat3[3][3];

  public:
    inline Matrix3();
    inline Matrix3(double value);
    inline Matrix3(double v00, double v01, double v02,
                   double v10, double v11, double v12,
                   double v20, double v21, double v22);
    inline Matrix3(const Matrix3&);
    inline Matrix3(const Vec& v1, const Vec& v2);
    // Create a rotation matrix using rotation angle and axis 
    inline Matrix3(double angle, const Vec& axis);

    // assign a value to all components of the Matrix3
    inline void set(double val);

    // assign a value to components of the Matrix3
     void set(int i, int j, double val);

    // assignment operator
    inline void operator = (const Matrix3 &m3);

    // access operator
    inline double operator() (int i,int j) const;
    inline double & operator() (int i,int j);

    // multiply Matrix3 by a constant: Mat * 3  
    inline Matrix3 operator * (const double value) const;
    // multiply constant by Matrix3:   3 * Mat
     friend Matrix3 operator * (double c, const Matrix3 &m3);

    // divide Matrix3 by a constant
    inline Matrix3 operator / (const double value) const;
 
    // modify by adding right hand side
    inline void operator += (const Matrix3 &m3);

    // modify by subtracting right hand side
    inline void operator -= (const Matrix3 &m3);

    // add two Matrix3s
    inline Matrix3 operator + (const Matrix3 &m3) const;

    // multiply two Matrix3s
    inline Matrix3 operator * (const Matrix3 &m3) const;

    // multiply Vec by Matrix3
    inline Vec operator * (const Vec& V) const;  
  
    // subtract two Matrix3s
    inline Matrix3 operator - (const Matrix3 &m3) const;

    // compare two Matrix3s
    inline bool operator==(const Matrix3 &m3) const;
    inline bool operator!=(const Matrix3 &m3) const
      { return !(*this == m3); }

    // multiply by constant
    inline void operator *= (const double value);

    // divide by constant
    inline void operator /= (const double value);

    //Determinant
    inline double Determinant() const;

    //Determinant
    inline bool Orthogonal() const;

    // Solve using Cramer's rule
    inline bool solveCramer(Vec& rhs, Vec& x) const;

    //Inverse
    Matrix3 Inverse() const;

    //Trace
    inline double Trace() const;

    //Norm, sqrt(M:M)
    inline double Norm() const;

    //NormSquared, M:M
    inline double NormSquared() const;

    //Contraction of two tensors
    inline double Contract(const Matrix3& mat) const;

    //Maximum element absolute value
    inline double MaxAbsElem() const;
  
    //Identity
    inline void Identity();

    //Transpose
    inline Matrix3 Transpose() const;

    // Exponential of a matrix
    Matrix3 Exponential(int num_terms) const;
    
    // Logarithm of a matrix
    Matrix3 Logarithm(int num_terms) const;
    
    // Get a left or right polar decomposition of a non-singular square matrix
    // Returns right stretch and rotation if rightFlag == true
    // Returns left stretch and rotation if rightFlag == false
    void polarDecomposition(Matrix3& stretch,
                            Matrix3& rotation,
                            double tolerance,
                            bool rightFlag) const;

    void polarDecompositionRMB(Matrix3& U, Matrix3& R) const;

    void polarRotationRMB(Matrix3& R) const;

    void polarDecompositionAFFinvTran(Matrix3& U, Matrix3& R) const;

    void polarRotationAFFinvTran(Matrix3& R) const;

    // Output in nice format
    void prettyPrint(std::ostream &out_file) const;

    // Use the Gershgorin circle theorem (see, e.g. Wikipedia) to find
    // the lower and upper bounds of the eigenvalues
    void gershgorinBounds(double &min, double& max);

    // Returns number of real, unique eigen values and passes
    // back the values.  If it returns 1, the value is passed back
    // in e1.  If it returns 2, the values are passed back in e1
    // and e2 such that e1 > e2.  If 3, then e1 > e2 > e3.
    int getEigenValues(double& e1, double& e2, double& e3) const;

    // Get the eigen values of the 2x2 sub-matrix specified by
    // the appropriate plane, returning the number of different real
    // values.  If there is only 1 (unique) eigenvalue, it is
    // passed back in e1.  If 2, then e1 > e2.
    int getXYEigenValues(double& e1, double& e2) const;
    int getYZEigenValues(double& e1, double& e2) const;
    int getXZEigenValues(double& e1, double& e2) const;
  
    // Returns an array of eigenvectors that form the basis
    // of eigenvectors corresponding to the given eigenvalue.
    // There may be 0 (if eigen_value is not a true eigen value),
    // 1, 2, or 3 ({1,0,0},{0,1,0},{0,0,1}) of these eigenvectors
    // (> 1 if eigen value has degeneracies I believe) and they
    // will NOT necessarily be normalized.
    // The relative_scale is used to determine what constitutes
    // values small enough to be considered zero.  The suggested
    // value for relative_scale is the maximum eigen value (e1) or
    // MaxAbsElem().
    inline std::vector<Vec> getEigenVecs(double eigen_value,
                                               double relative_scale /* e1 suggested */) const;

    // Solves for a single particular solution (possible arbitrary) to
    // the equation system: Ax = rhs where A is this Matrix.
    // Returns false if there is no solution, otherwise a particular
    // solution is passed back via xp.
    // The relative_scale is used to determine what constitutes values
    // small enough to be considered zero.  The suggested value for
    // relative_scale is MaxAbsElem() or the maximum eigen-value.
    inline bool solveParticular(Vec rhs, Vec& xp,
                                double relative_scale /* MaxAbsElem() suggested */) const;
  
    // Solves for the space of solutions for Ax = 0 where A is this Matrix.
    // The solution is any linear combination of the resulting array
    // of vectors.  That is, if result = {xg1, xg2, ...}, then
    // x = a*xg1 + b*xg2 + ... where a, b, etc. are arbitrary scalars.
    // The relative_scale is used to determine what constitutes values
    // small enough to be considered zero.  The suggested value for
    // relative_scale is MaxAbsElem() or the maximum eigen-value.
    // Result Possibilities:
    // Single Solution: result.size() = 0, {0,0,0} is only solution
    // Line of Solutions: result.size() = 1
    // Plane of Solutions: result.size() = 2
    // Solution everywhere: result = {{1,0,0},{0,1,0},{0,0,1}}
    inline std::vector<Vec> solveHomogenous(double relative_scale /* =
                                                                        MaxAbsElem() suggested */) const;

    // Solves for the space of solutions for Ax = rhs where A is this Matrix.
    // This is a more efficient combination of solveParticular and
    // solveHomogenous (where the homogenous results are passed back via
    // xg_basis).
    // The relative_scale is used to determine what constitutes values small
    // enough to be considered zero.  The suggested value for relative_scale
    // is MaxAbsElem() or the maximum eigen value.
    inline bool solve(Vec rhs, Vec& xp,
                      std::vector<Vec>& xg_basis,
                      double relative_scale /* MaxAbsElem() suggested */) const;

  private:
    // Reduce the matrix and rhs, representing the equation system:
    // A*x = y = rhs, to a matrix in upper triangular form with
    // corresponding rhs for an equivalent equation systme.
    // The guarantee for this new matrix is that the first non-zero
    // column of a row is to the right (greater) of the first non-zero
    // column in the row above it and the first non-zero column of
    // any row has zeroes in every other row and a one in that row.
    // If rhs == NULL, then the rhs is assumed to be
    // the zero vector and thus will not need to change.
     static void triangularReduce(Matrix3& A, Vec* rhs, int& num_zero_rows,
                                 double relative_scale);

    // solveHomogenous for a Matrix that has already by triangularReduced
     std::vector<Vec> solveHomogenousReduced(int num_zero_rows) const;

    // solveHomogenous for a Matrix that has already by triangularReduced
     bool solveParticularReduced(const Vec& rhs, Vec& xp,
                                int num_zero_rows) const;

     friend std::ostream & operator << (std::ostream &out_file, const Matrix3 &m3);

  }; // end class Matrix3

  inline double Matrix3::Trace() const {
      double trace = 0.0;
      for (int i = 0; i< 3; i++) {
        trace += mat3[i][i];
      }
      return trace;
  }

  inline Matrix3::Matrix3()
    {
      // Default Constructor
      // Initialization to 0.0
      for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
          mat3[i][j] = 0.0;
        }
      }
    }

  inline Matrix3::Matrix3(double value)
    {
      // Constructor
      // With initialization to a single value

      for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
          mat3[i][j] = value;
        }
      }
    }


  inline Matrix3::Matrix3(double a00,double a01,double a02,
                          double a10,double a11,double a12,
                          double a20,double a21,double a22)
    {
      // Constructor
      // With full initialization

      mat3[0][0] = a00; mat3[0][1] = a01; mat3[0][2] = a02;
      mat3[1][0] = a10; mat3[1][1] = a11; mat3[1][2] = a12;
      mat3[2][0] = a20; mat3[2][1] = a21; mat3[2][2] = a22;

    }

  inline Matrix3::Matrix3(const Matrix3& copy)
    {
      // Copy Constructor

      mat3[0][0] = copy.mat3[0][0]; mat3[0][1] = copy.mat3[0][1]; mat3[0][2] = copy.mat3[0][2];
      mat3[1][0] = copy.mat3[1][0]; mat3[1][1] = copy.mat3[1][1]; mat3[1][2] = copy.mat3[1][2];
      mat3[2][0] = copy.mat3[2][0]; mat3[2][1] = copy.mat3[2][1]; mat3[2][2] = copy.mat3[2][2];

    }

  // Create a matrix using dyadic multiplication M = a*b^T
  inline Matrix3::Matrix3(const Vec& a, const Vec& b)
    {
      // Dyadic multiplication of two vectors a and b
      mat3[0][0] = a[0]*b[0]; mat3[0][1] = a[0]*b[1]; mat3[0][2] = a[0]*b[2];
      mat3[1][0] = a[1]*b[0]; mat3[1][1] = a[1]*b[1]; mat3[1][2] = a[1]*b[2];
      mat3[2][0] = a[2]*b[0]; mat3[2][1] = a[2]*b[1]; mat3[2][2] = a[2]*b[2];
    }

  // Create a rotation matrix using the angle of rotation and the
  // axis of rotation
  inline Matrix3::Matrix3(double angle, const Vec& axis)
    {
      // Create matrix A  = [[0 -axis3 axis2];[axis3 0 -axis1];[-axis2 axis1 0]]
      // Calculate the dyad aa
      // Calculate the rotation matrix
      // R = (I - aa)*cos(angle) + aa - A*sin(angle);
      double ca = cos(angle); double sa = sin(angle);
      mat3[0][0] = (1.0 - axis[0]*axis[0])*ca + axis[0]*axis[0];
      mat3[0][1] = (- axis[0]*axis[1])*ca + axis[0]*axis[1] + axis[2]*sa;
      mat3[0][2] = (- axis[0]*axis[2])*ca + axis[0]*axis[2] - axis[1]*sa;
      mat3[1][0] = (- axis[1]*axis[0])*ca + axis[1]*axis[0] - axis[2]*sa;
      mat3[1][1] = (1.0 - axis[1]*axis[1])*ca + axis[1]*axis[1]; 
      mat3[1][2] = (- axis[1]*axis[2])*ca + axis[1]*axis[2] + axis[0]*sa;
      mat3[2][0] = (- axis[2]*axis[0])*ca + axis[2]*axis[0] + axis[1]*sa;
      mat3[2][1] = (- axis[2]*axis[1])*ca + axis[2]*axis[1] - axis[0]*sa; 
      mat3[2][2] = (1.0 - axis[2]*axis[2])*ca + axis[2]*axis[2];
    }

  inline double Matrix3::NormSquared() const
    {
      // Return the norm of a 3x3 matrix

      double norm = 0.0;

      for (int i = 0; i< 3; i++) {
        for(int j=0;j<3;j++){
          norm += mat3[i][j]*mat3[i][j];
        }
      }
      return norm;
    }

  inline double Matrix3::Norm() const
    {
      return sqrt(NormSquared());
    }

  inline double Matrix3::Contract(const Matrix3& mat) const
    {
      // Return the contraction of this matrix with another 

      double contract = 0.0;

      for (int i = 0; i< 3; i++) {
        for(int j=0;j<3;j++){
          contract += mat3[i][j]*(mat.mat3[i][j]);
        }
      }
      return contract;
    }

  inline double Matrix3::MaxAbsElem() const
    {
      double max = 0;
      double absval;
      for (int i = 0; i< 3; i++) {
        for(int j=0;j<3;j++){
          absval = fabs(mat3[i][j]);
          if (absval > max) max = absval;
        }
      }
      return max;
    }

  inline Matrix3 Matrix3::Transpose() const
    {
      // Return the transpose of a 3x3 matrix

      return Matrix3(mat3[0][0],mat3[1][0],mat3[2][0],
                     mat3[0][1],mat3[1][1],mat3[2][1],
                     mat3[0][2],mat3[1][2],mat3[2][2]);

    }

  inline void Matrix3::Identity()
    {
      // Set a matrix3 to the identity

      mat3[0][0] = mat3[1][1] = mat3[2][2] = 1.0;
      mat3[0][1] = mat3[0][2] = mat3[1][0] = 0.0;
      mat3[1][2] = mat3[2][0] = mat3[2][1] = 0.0;

    }

  inline void Matrix3::operator = (const Matrix3 &m3)
    {
      // Copy value from right hand side of assignment

      for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
          mat3[i][j] = m3(i,j);
        }
      }

    }

  inline void Matrix3::set(double value)
    {
      // Assign the Matrix3 the value components
      for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
          mat3[i][j] = value;
        }
      }
    }

  inline void Matrix3::operator *= (const double value)
    {
      // Multiply each component of the Matrix3 by the value

      for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
          mat3[i][j] *= value;
        }
      }
    }

  inline void Matrix3::operator += (const Matrix3 &m3)
    {
      // += operator 

      for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
          mat3[i][j] += m3(i,j);
        }
      }

    }

  inline void Matrix3::operator -= (const Matrix3 &m3)
    {
      // -= operator 

      for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
          mat3[i][j] -= m3(i,j);
        }
      }

    }

  inline void Matrix3::operator /= (const double value)
    {
      // Divide each component of the Matrix3 by the value

      assert(value != 0.);
      double ivalue = 1./value;
      for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
          mat3[i][j] *= ivalue;
        }
      }
    }

  inline Matrix3 Matrix3::operator * (const double value) const
    {
      //   Multiply a Matrix3 by a constant

      return Matrix3(mat3[0][0]*value,mat3[0][1]*value,mat3[0][2]*value,
                     mat3[1][0]*value,mat3[1][1]*value,mat3[1][2]*value, 
                     mat3[2][0]*value,mat3[2][1]*value,mat3[2][2]*value); 
    }

  inline Matrix3 operator * ( double c, const Matrix3 &m3 )
    {
      return m3 * c;
    }

  inline Matrix3 Matrix3::operator * (const Matrix3 &m3) const
    {
      //   Multiply a Matrix3 by a Matrix3

      return Matrix3(mat3[0][0]*m3(0,0)+mat3[0][1]*m3(1,0)+mat3[0][2]*m3(2,0),
                     mat3[0][0]*m3(0,1)+mat3[0][1]*m3(1,1)+mat3[0][2]*m3(2,1),
                     mat3[0][0]*m3(0,2)+mat3[0][1]*m3(1,2)+mat3[0][2]*m3(2,2),

                     mat3[1][0]*m3(0,0)+mat3[1][1]*m3(1,0)+mat3[1][2]*m3(2,0),
                     mat3[1][0]*m3(0,1)+mat3[1][1]*m3(1,1)+mat3[1][2]*m3(2,1),
                     mat3[1][0]*m3(0,2)+mat3[1][1]*m3(1,2)+mat3[1][2]*m3(2,2),

                     mat3[2][0]*m3(0,0)+mat3[2][1]*m3(1,0)+mat3[2][2]*m3(2,0),
                     mat3[2][0]*m3(0,1)+mat3[2][1]*m3(1,1)+mat3[2][2]*m3(2,1),
                     mat3[2][0]*m3(0,2)+mat3[2][1]*m3(1,2)+mat3[2][2]*m3(2,2));
    }

  inline Vec Matrix3::operator * (const Vec& V) const
    {
      return Vec(mat3[0][0] * V[0] + mat3[0][1] * V[1] + mat3[0][2] * V[2],
                    mat3[1][0] * V[0] + mat3[1][1] * V[1] + mat3[1][2] * V[2],
                    mat3[2][0] * V[0] + mat3[2][1] * V[1] + mat3[2][2] * V[2]);
    }


  inline Matrix3 Matrix3::operator + (const Matrix3 &m3) const
    {
      //   Add a Matrix3 to a Matrix3

      return Matrix3(mat3[0][0] + m3(0,0),mat3[0][1] + m3(0,1),mat3[0][2] + m3(0,2),
                     mat3[1][0] + m3(1,0),mat3[1][1] + m3(1,1),mat3[1][2] + m3(1,2),
                     mat3[2][0] + m3(2,0),mat3[2][1] + m3(2,1),mat3[2][2] + m3(2,2));
    }

  inline Matrix3 Matrix3::operator - (const Matrix3 &m3) const
    {
      //   Subtract a Matrix3 from a Matrix3

      return Matrix3(mat3[0][0] - m3(0,0),mat3[0][1] - m3(0,1),mat3[0][2] - m3(0,2),
                     mat3[1][0] - m3(1,0),mat3[1][1] - m3(1,1),mat3[1][2] - m3(1,2),
                     mat3[2][0] - m3(2,0),mat3[2][1] - m3(2,1),mat3[2][2] - m3(2,2));
    }

  inline bool Matrix3::operator==(const Matrix3 &m3) const
    {
      return
        mat3[0][0] == m3(0,0) && mat3[0][1] == m3(0,1) && mat3[0][2] == m3(0,2) &&
        mat3[1][0] == m3(1,0) && mat3[1][1] == m3(1,1) && mat3[1][2] == m3(1,2) &&
        mat3[2][0] == m3(2,0) && mat3[2][1] == m3(2,1) && mat3[2][2] == m3(2,2);
    }

  inline Matrix3 Matrix3::operator / (const double value) const
    {
      //   Divide a Matrix3 by a constant

      assert(value != 0.);
      double ivalue = 1.0/value;

      return Matrix3(mat3[0][0]*ivalue,mat3[0][1]*ivalue,mat3[0][2]*ivalue,
                     mat3[1][0]*ivalue,mat3[1][1]*ivalue,mat3[1][2]*ivalue, 
                     mat3[2][0]*ivalue,mat3[2][1]*ivalue,mat3[2][2]*ivalue); 
    }

  inline double Matrix3::Determinant() const
    {
      // Return the determinant of a 3x3 matrix

      double temp = 0.0;

      temp= mat3[0][0]*mat3[1][1]*mat3[2][2] +
        mat3[0][1]*mat3[1][2]*mat3[2][0] +
        mat3[0][2]*mat3[1][0]*mat3[2][1] -
        mat3[0][2]*mat3[1][1]*mat3[2][0] -
        mat3[0][1]*mat3[1][0]*mat3[2][2] -
        mat3[0][0]*mat3[1][2]*mat3[2][1];


      // return result

      return temp;
    }

  inline bool Matrix3::Orthogonal() const
    {
      // Identity Matrix
      Matrix3 I;  I.Identity();

      // Calculate Matrix*(Matrix.Transpose)
      Matrix3 R(*this);
      Matrix3 RRT = R*(R.Transpose());

      // Check that RRT = I
      if (RRT != I) return false;
      return true;
    }

  inline bool Matrix3::solveCramer(Vec& b, Vec& x) const
    {
      double det = this->Determinant();
      if (det == 0.0) return false;
      double odet = 1.0/det;
      x[0] = odet*(b[0]*(mat3[1][1]*mat3[2][2] - mat3[2][1]*mat3[1][2])
                   - mat3[0][1]*(b[1]*mat3[2][2] - b[2]*mat3[1][2])
                   + mat3[0][2]*(b[1]*mat3[2][1] - b[2]*mat3[1][1]));
 
      x[1] = odet*(mat3[0][0]*(b[1]*mat3[2][2] - b[2]*mat3[1][2])
                   - b[0]*(mat3[1][0]*mat3[2][2] - mat3[2][0]*mat3[1][2])
                   + mat3[0][2]*(mat3[1][0]*b[2]  - mat3[2][0]*b[1]));
 
      x[2] = odet*(mat3[0][0]*(mat3[1][1]*b[2]  - mat3[2][1]*b[1])
                   - mat3[0][1]*(mat3[1][0]*b[2]  - mat3[2][0]*b[1])
                   + b[0]*(mat3[1][0]*mat3[2][1] - mat3[2][0]*mat3[1][1]));
      return true;
    }

  inline double Matrix3::operator () (int i, int j) const
    {
      // Access the i,j component
      return mat3[i][j];
    }

  inline double &Matrix3::operator () (int i, int j)
    {
      // Access the i,j component
      return mat3[i][j];
    }

  inline std::vector<Vec> Matrix3::getEigenVecs(double eigen_value,
                                                      double relative_scale) const
    {
      // A*x = e*x
      // (A - e*I)*x = 0
      Matrix3 A_sub_eI(mat3[0][0] - eigen_value, mat3[0][1], mat3[0][2],
                       mat3[1][0], mat3[1][1] - eigen_value, mat3[1][2],
                       mat3[2][0], mat3[2][1], mat3[2][2] - eigen_value);
      int num_zero_rows;
      triangularReduce(A_sub_eI, NULL, num_zero_rows, relative_scale);
      return A_sub_eI.solveHomogenousReduced(num_zero_rows);
    }

  inline bool Matrix3::solveParticular(Vec rhs, Vec& xp,
                                       double relative_scale) const
    {
      int num_zero_rows;
      Matrix3 A(*this);
      triangularReduce(A, &rhs, num_zero_rows, relative_scale);
      return A.solveParticularReduced(rhs, xp, num_zero_rows);
    }

  inline std::vector<Vec> Matrix3::solveHomogenous(double relative_scale)
    const
    {
      int num_zero_rows;
      Matrix3 A(*this);
      triangularReduce(A, NULL, num_zero_rows, relative_scale);
      return A.solveHomogenousReduced(num_zero_rows);
    }

  inline bool Matrix3::solve(Vec rhs, Vec& xp,
                             std::vector<Vec>& xg_basis, double relative_scale) const
    {
      int num_zero_rows;
      Matrix3 A(*this);
      triangularReduce(A, &rhs, num_zero_rows, relative_scale);
      if (A.solveParticularReduced(rhs, xp, num_zero_rows)) {
        xg_basis = A.solveHomogenousReduced(num_zero_rows);
        return true;
      }
      xg_basis.resize(0);
      return false;
    }

  // This is backwards: if the vector comes first then it should
  // multiply the matrix columnwise instead of rowwise.  For now,
  // I won't fix it because changing it may break other code. -- witzel
  inline Vec operator*(const Vec& v, const Matrix3& m3) {
    // Right multiply a Vec by a Matrix3

    double x = v.x()*m3(0,0)+v.y()*m3(0,1)+v.z()*m3(0,2);
    double y = v.x()*m3(1,0)+v.y()*m3(1,1)+v.z()*m3(1,2);
    double z = v.x()*m3(2,0)+v.y()*m3(2,1)+v.z()*m3(2,2);

    return Vec(x, y, z);
  }

} // End namespace dem


#endif  // __ELLIP3D_CORE_MATH_MATRIX3_H__
