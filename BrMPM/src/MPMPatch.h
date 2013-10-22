#ifndef __MATITI_MPMPATCH_H__
#define __MATITI_MPMPATCH_H__

#include <Domain.h>
#include <Types.h>
#include <BodySP.h>
#include <Geometry/Point3D.h>
#include <Geometry/Vector3D.h>
#include <VelocityBCSPArray.h>
#include <Core/ProblemSpec/ProblemSpecP.h>
#include <iostream>

namespace BrMPM {

  class MPMPatch
 {

  public:  

//    friend std::ostream& operator<<(std::ostream& out, const Matiti::Domain& domain);

  public:  

    MPMPatch() ;
     ~MPMPatch();

    MPMPatch(const Point3D& lower, const Point3D& upper);
    MPMPatch(const Point3D& lower, const Point3D& upper, const IntArray3& numCells);
    MPMPatch(const Uintah::ProblemSpecP& ps);

 //   Domain(const Point3D& lower, const Point3D& upper, const IntArray3& numCells);
    
 //   Domain(const Point3D& lower, const Point3D& upper, const double& horizon);

    void initialize(const Uintah::ProblemSpecP& ps);

  
    const int& ghost() const {return d_nof_ghost;}
 //   const double& thick() const {return d_thick;}
    const int& particlesperelement() const {return d_nof_particles_per_cell;}
   // const IntArray3& numGrids() const;
    const double totalGrids() const;

    const Point3D lower() const {return d_lower;}
    const Point3D upper() const {return d_upper;}

    const Vector3D& cellSize()  {return d_cellsize;}
    const Vector3D& numGrids()  {return d_num_grids;}
    const std::vector<Point3D> gridsPosition()  {return d_gridsPosition;}

    void findGradeIndex(const Point3D& point,
                       IntArray3& cell) const;
    void findGradeIndex(const long64& cell_key,
                       IntArray3& cell) const;

    bool insidePatch(const Point3D& point) const;
    bool allInsidePatch(const std::vector<Point3D> points) const;

 //   void applyVelocityBC(BodySP& body) const;

 //   bool intersection(const Point3D& point, const Vector3D& ray,
 //                     Point3D& hitPoint) const;

  private:

 //   Point3D d_lower;
 //   Point3D d_upper;
   
    Point3D d_lower;
    Point3D d_upper;
    Vector3D d_node_counts;
    Vector3D d_cellsize;
    
   // int d_t_initial;
   // int d_t_final;
    int d_nof_ghost;
    double d_tol;                    //tolerance
   // double d_thick;
    int d_nof_particles_per_cell;

    
    double xcoord, ycoord, zcoord;
    

    Vector3D d_num_grids;
    Point3D d_grids;

    std::vector<Point3D>  d_gridsPosition;  
 //   VelocityBCSPArray d_vel_BC;
   
  };  // end class
}  // end namespace
#endif
