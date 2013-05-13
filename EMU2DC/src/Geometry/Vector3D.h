#ifndef __EMU2DC_VECTOR3D_H__
#define __EMU2DC_VECTOR3D_H__

#include <Types.h>
#include <Geometry/Point3D.h>

namespace Emu2DC {

  class Vector3D {

  public:

    friend std::ostream& operator<<(std::ostream& os, const Emu2DC::Vector3D& p);

  public:

    Vector3D(): d_vec({{std::numeric_limits<double>::max(), 
                        std::numeric_limits<double>::max(), 
                        std::numeric_limits<double>::max()}}) {}
    Vector3D(double x, double y, double z): d_vec({{x, y, z}}) {}
    Vector3D(const Vector3D& vec);
    Vector3D(const Point3D& start, const Point3D& end);

    ~Vector3D() {}

    //  **WARNING** Not checking index.  Do checking outside.
    double operator[](int index) const {return d_vec[index];}
    double& operator[](int index) {return d_vec[index];}

    void x(const double xx) {d_vec[0] = xx;}
    double x() const {return d_vec[0];}
    void y(const double yy) {d_vec[1] = yy;}
    double y() const {return d_vec[1];}
    void z(const double zz) {d_vec[2] = zz;}
    double z() const {return d_vec[2];}
    
    double length() const;
    double lengthSq() const;

    double dot(const Vector3D& vec) const;
    Vector3D cross(const Vector3D& vec) const;

    bool operator==(const Vector3D& vec) const;
    bool operator!=(const Vector3D& vec) const;

    Vector3D& operator=(const Vector3D& vec);

    Vector3D operator*(const double) const;
    Vector3D operator/(const double) const;
    Vector3D operator+(const Vector3D& vec) const;
    Vector3D operator-(const Vector3D& vec) const;

    Vector3D& operator*=(const double);
    Vector3D& operator/=(const double);
    Vector3D& operator+=(const Vector3D& vec);
    Vector3D& operator-=(const Vector3D& vec);

    void reset() {d_vec[0] = 0.0; d_vec[1] = 0.0; d_vec[2] = 0.0;}
  private:

    //double d_x, d_y, d_z;
    Array3 d_vec;

  }; // end class

} // end namespace

#endif

