/*
 * DistanceMarcher.h
 *
 *  Created on: 5/12/2013
 *      Author: banerjee
 *      Original version: scikit-fmm/skfmm/distance_marcher.h
 */

#ifndef DISTANCEMARCHER_H_
#define DISTANCEMARCHER_H_

#include <Contact/FastMarching/BaseMarcher.h>

namespace BrMPM
{

  class DistanceMarcher: public BaseMarcher
  {
  public:
    DistanceMarcher(Double3D* phi, const Vector3D& dx, Int3D* flag,
                    Double3D* distance, int ndim, const Double3DSizeType* shape,
                    bool self_test, int order);
    virtual ~DistanceMarcher();

  protected:

    virtual double solveQuadratic(int ii, const double& aa, const double& bb, double& cc);

    virtual void initializeFrozen();
    virtual double updatePointSecondOrder(int ii);
    virtual double updatePointFirstOrder(int ii);
  };

} /* namespace BrMPM */
#endif /* DISTANCEMARCHER_H_ */