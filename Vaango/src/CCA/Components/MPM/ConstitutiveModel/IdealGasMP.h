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

//  IdealGasMP.h
//  class ConstitutiveModel ConstitutiveModel data type -- 3D -
//  holds ConstitutiveModel
//  information for the FLIP technique:
//    This is for Compressible NeoHookean materials
//    Features:
//      Usage:

#ifndef __IDEALGAS_CONSTITUTIVE_MODEL_H__
#define __IDEALGAS_CONSTITUTIVE_MODEL_H__

#include "ConstitutiveModel.h"
#include <Core/Disclosure/TypeDescription.h>
#include <Core/Math/Matrix3.h>
#include <cmath>
#include <vector>

namespace Uintah {
class IdealGasMP : public ConstitutiveModel
{
private:
  // Create datatype for storing model parameters
public:
  struct CMData
  {
    double gamma;
    double cv;
    double Pref;
  };

private:
  CMData d_initialData;

  // Prevent copying of this class
  // copy constructor
  // IdealGasMP(const IdealGasMP &cm);
  IdealGasMP& operator=(const IdealGasMP& cm);

public:
  // constructors
  IdealGasMP(ProblemSpecP& ps, MPMFlags* flag);
  IdealGasMP(const IdealGasMP* cm);

  // destructor
  ~IdealGasMP() override;

  void outputProblemSpec(ProblemSpecP& ps, bool output_cm_tag = true) override;

  IdealGasMP* clone() override;

  // compute stable timestep for this patch
  virtual void computeStableTimestep(const Patch* patch,
                                     const MPMMaterial* matl,
                                     DataWarehouse* new_dw);

  // compute stress at each particle in the patch
  void computeStressTensor(const PatchSubset* patches, const MPMMaterial* matl,
                           DataWarehouse* old_dw,
                           DataWarehouse* new_dw) override;

  // initialize  each particle's constitutive model data
  void initializeCMData(const Patch* patch, const MPMMaterial* matl,
                        DataWarehouse* new_dw) override;

  void allocateCMDataAddRequires(Task* task, const MPMMaterial* matl,
                                 const PatchSet* patch,
                                 MPMLabel* lb) const override;

  void allocateCMDataAdd(DataWarehouse* new_dw, ParticleSubset* subset,
                         ParticleLabelVariableMap* newState,
                         ParticleSubset* delset,
                         DataWarehouse* old_dw) override;

  void addComputesAndRequires(Task* task, const MPMMaterial* matl,
                              const PatchSet* patches) const override;

  void addComputesAndRequires(Task* task, const MPMMaterial* matl,
                              const PatchSet* patches, const bool recursion,
                              const bool schedParent = true) const override;

  double computeRhoMicroCM(double pressure, const double p_ref,
                           const MPMMaterial* matl, double temperature,
                           double rho_guess) override;

  void computePressEOSCM(double rho_m, double& press_eos, double p_ref,
                         double& dp_drho, double& ss_new,
                         const MPMMaterial* matl, double temperature) override;

  double getCompressibility() override;

  void addParticleState(std::vector<const VarLabel*>& from,
                        std::vector<const VarLabel*>& to) override;
};
} // End namespace Uintah

#endif // __IDEALGAS_CONSTITUTIVE_MODEL_H__