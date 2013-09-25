#ifndef __MATITI_GRADE_H__
#define __MATITI_GRADE_H__

#include <Node.h>
#include <Types.h>
#include <Geomerty/Point3D.h>
#include <Geometry/Vector3D.h>
#include <iostream>
#include <cmath>

namespace Matiti  {

   class Grade  :  public Node

public:






inline void mass(double& mass) {d_mass = mass;}
inline double mass() const {return d_mass = density()*volume();} 

inline void momentum(Vector3D& momentum) {d_momentum = momentum;}
inline Vector3D momentum() const {return d_momentum = velocity().operator*(density()*volume());} 























private

double d_mass;


Vector3D d_momentum;


#endif
