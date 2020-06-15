/*
 * The MIT License
 *
 * Copyright (c) 2013-2014 Callaghan Innovation, New Zealand
 * Copyright (c) 2015-2020 Parresia Research Limited, New Zealand
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

#ifndef __CNH_MMS_CONSTITUTIVE_MODEL_H__
#define __CNH_MMS_CONSTITUTIVE_MODEL_H__

#include <CCA/Components/MPM/ConstitutiveModel/ConstitutiveModel.h>
#include <Core/Disclosure/TypeDescription.h>
#include <Core/Math/Matrix3.h>
#include <math.h>
#include <vector>

namespace Uintah {
class CNH_MMS : public ConstitutiveModel
{

public:
  // Create datatype for storing model parameters
  struct CMData
  {
    double Bulk;
    double Shear;
  };

  const VarLabel* bElBarLabel;
  const VarLabel* bElBarLabel_preReloc;

protected:
  CMData d_initialData;
  bool d_useModifiedEOS;
  int d_8or27;

public:
  // constructors
  CNH_MMS(ProblemSpecP& ps, MPMFlags* flag);
  CNH_MMS(const CNH_MMS* cm);
  CNH_MMS& operator=(const CNH_MMS& cm) = delete;

  // destructor
  ~CNH_MMS() override;

  ModelType modelType() const override
  {
    return ModelType::TOTAL_FORM;
  }

  void outputProblemSpec(ProblemSpecP& ps, bool output_cm_tag = true) override;

  // clone
  CNH_MMS* clone() override;

  // compute stable timestep for this patch
  virtual void computeStableTimestep(const Patch* patch,
                                     const MPMMaterial* matl,
                                     DataWarehouse* new_dw);

  // compute stress at each particle in the patch
  void computeStressTensor(const PatchSubset* patches, const MPMMaterial* matl,
                           DataWarehouse* old_dw,
                           DataWarehouse* new_dw) override;

  // carry forward CM data for RigidMPM
  void carryForward(const PatchSubset* patches, const MPMMaterial* matl,
                    DataWarehouse* old_dw, DataWarehouse* new_dw) override;

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

#endif // __CNH_MMS_CONSTITUTIVE_MODEL_H__