/*
 * The MIT License
 *
 * Copyright (c) 1997-2019 Center for the Simulation of Accidental Fires and
 * Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI),
 * University of Utah.
 * Copyright (c) 2015-2020 Parresia Research Limited, New Zealand
 *
 * License for the specific language governing rights and limitations under
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __MPM_CONSTITUTIVEMODEL_MOHRCOULOMB_H__
#define __MPM_CONSTITUTIVEMODEL_MOHRCOULOMB_H__

#include "CCA/Components/MPM/Materials/ConstitutiveModel/ConstitutiveModel.h"
#include <Core/Grid/Variables/VarLabel.h>
#include <Core/Math/Matrix3.h>

#include <cmath>
#include <memory>
#include <vector>

namespace Uintah {

class MohrCoulomb : public ConstitutiveModel
{
public:

  std::vector<const VarLabel*> ISVLabels;
  std::vector<const VarLabel*> ISVLabels_preReloc;

  MohrCoulomb(ProblemSpecP& ps, MPMFlags* flag);
  MohrCoulomb(const MohrCoulomb* cm);

  MohrCoulomb& operator=(const MohrCoulomb& cm) = delete;

  virtual ~MohrCoulomb() override;

  virtual void outputProblemSpec(ProblemSpecP& ps, bool output_cm_tag = true);

  MohrCoulomb* clone() override;

  void addParticleState(std::vector<const VarLabel*>& from,
                        std::vector<const VarLabel*>& to) override;

  void addInitialComputesAndRequires(Task* task,
                                     const MPMMaterial* matl,
                                     const PatchSet* patches) const override;

  void initializeCMData(const Patch* patch, const MPMMaterial* matl,
                        DataWarehouse* new_dw) override;

  void computeStableTimestep(const Patch* patch,
                             const MPMMaterial* matl,
                             DataWarehouse* new_dw) override;

  // compute stress at each particle in the patch
  virtual void computeStressTensor(const PatchSubset* patches,
                                   const MPMMaterial* matl,
                                   DataWarehouse* old_dw,
                                   DataWarehouse* new_dw);

  // carry forward CM data for RigidMPM
  virtual void carryForward(const PatchSubset* patches, const MPMMaterial* matl,
                            DataWarehouse* old_dw, DataWarehouse* new_dw);


  virtual void allocateCMDataAddRequires(Task* task, const MPMMaterial* matl,
                                         const PatchSet* patch,
                                         MPMLabel* lb) const;


  virtual void allocateCMDataAdd(DataWarehouse* new_dw,
                                 ParticleSubset* subset,
                                 map<const VarLabel*,
                                 ParticleVariableBase*>* newState,
                                 ParticleSubset* delset,
                                 DataWarehouse* old_dw);

  virtual void addComputesAndRequires(Task* task, const MPMMaterial* matl,
                                      const PatchSet* patches) const;

  virtual void addComputesAndRequires(Task* task, const MPMMaterial* matl,
                                      const PatchSet* patches,
                                      const bool recursion) const;

  virtual double computeRhoMicroCM(double pressure, const double p_ref,
                                   const MPMMaterial* matl, double temperature,
                                   double rho_guess);

  virtual void computePressEOSCM(double rho_m, double& press_eos, double p_ref,
                                 double& dp_drho, double& ss_new,
                                 const MPMMaterial* matl, double temperature);

  virtual double getCompressibility();


private:

  struct ModelFlags
  {
    bool useWaterRetention;
    bool useUndrainedShearTransition;
    bool useVariableElasticModulus;
    bool useLinearlyVaryingCohesion;
    bool useSoftening;
    bool useRegularizedNonlocalSoftening;
    bool useNonlocalCorrection;
  };

  struct ModelParams
  {
    double G, K, c, phi, psi, pMin;
    double initialSuction, phi_b;
    double waterRetentionParams[4];
    double waterInfluenceA1, waterInfluenceB1, waterInfluenceW;
    double betaStrainRate, refStrainRate, shearStrainRate;
    double variableModulusM, variableModulusNuY, variableModulusShearStrain;
    double linearCohesionA, linearCohesionYRef;
    double softeningSt, softeningStrain95;
    double regularizationTFE, regularizationTShear;
    double nonlocalN, nonlocalL;
    RetentionModel retentionModel;
  };

  struct Integration
  {
    int maxIter;
    double alfaCheck, d_alfaChange, d_alfaRatio;
    double d_yieldTol, d_integrationTol, d_betaFactor; 
    double d_minMeanStress, d_suctionTol; 
    std::string driftCorrection, tolMethod, solutionAlgorithm;
  };

  std::string d_modelType; // options: classic, sheng
  std::unique_ptr<MohrCoulombBase> d_modelP;

  ModelFlags d_flags;
  ModelParams d_params;
  Integration d_int;

  void getInputParameters(ProblemSpecP& ps);
  void getModelParameters(ProblemSpecP& ps);
  void getIntegrationParameters(ProblemSpecP& ps);
  void checkModelParameters() const;

  void setIntegrationParameters();

  void initializeLocalMPMLabels();

  void CalculateStress(int& nblk, int& ninsv, double& dt, double UI[],
                       double stress[], double D[], double svarg[], double& USM,
                       double shear_strain_nonlocal, double shear_strain_rate);


  double GetSr(double UseWaterRetention, double Suction, double* WTRParam);
  double GetSuction(double UseWaterRetention, double Sr, double* WTRParam);

  void outputModelProblemSpec(ProblemSpecP& ps) const;
  void outputIntegrationProblemSpec(ProblemSpecP& ps) const;

};

} // End namespace Uintah

#endif // __MPM_CONSTITUTIVEMODEL_MOHRCOULOMB_H__
