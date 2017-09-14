/*
 * The MIT License
 *
 * Copyright (c) 2015-2017 Parresia Research Limited, New Zealand
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

// Namespace Vaango::
#include <CCA/Components/MPM/ConstitutiveModel/TabularPlasticity.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ElasticModuliModelFactory.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldConditionFactory.h>

// Namespace Uintah::
#include <CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <CCA/Ports/DataWarehouse.h>
#include <Core/Labels/MPMLabel.h>
#include <Core/Malloc/Allocator.h>
#include <Core/ProblemSpec/ProblemSpec.h>

#include <Core/Grid/Box.h>
#include <Core/Grid/Level.h>
#include <Core/Grid/Patch.h>
#include <Core/Grid/Task.h>

#include <Core/Grid/Variables/NCVariable.h>
#include <Core/Grid/Variables/NodeIterator.h>
#include <Core/Grid/Variables/ParticleVariable.h>
#include <Core/Grid/Variables/VarLabel.h>
#include <Core/Grid/Variables/VarTypes.h>

#include <Core/Exceptions/InvalidValue.h>
#include <Core/Exceptions/ParameterNotFound.h>

#include <Core/Math/Matrix3.h>
#include <Core/Math/MinMax.h>
#include <Core/Math/MiscMath.h>

#include <sci_values.h>

// Namespace std::
#include <cerrno>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>

// Boost
#include <boost/foreach.hpp>
#include <boost/range/combine.hpp>

#define USE_LOCAL_LOCALIZED_PVAR
#define CHECK_FOR_NAN
#define CLAMP_DEF_GRAD
//#define CHECK_FOR_NAN_EXTRA
//#define WRITE_YIELD_SURF
//#define CHECK_INTERNAL_VAR_EVOLUTION
//#define DEBUG_INTERNAL_VAR_EVOLUTION
//#define DEBUG_INTERNAL_VAR_EVOLUTION_COMPUTATION
//#define CHECK_HYDROSTATIC_TENSION
//#define CHECK_TENSION_STATES
//#define CHECK_TENSION_STATES_1
//#define CHECK_DAMAGE_ALGORITHM
//#define CHECK_SUBSTEP
//#define CHECK_TRIAL_STRESS
//#define CHECK_YIELD_SURFACE_NORMAL
//#define CHECK_FLOATING_POINT_OVERFLOW
//#define DEBUG_YIELD_BISECTION_R
//#define CHECK_CONSISTENCY_BISECTION_CONVERGENCE
//#define CHECK_ELASTIC_STRAIN

using namespace Vaango;
using Uintah::VarLabel;
using Uintah::Matrix3;
using std::ostringstream;
using std::endl;

const double TabularPlasticity::one_third(1.0 / 3.0);
const double TabularPlasticity::two_third(2.0 / 3.0);
const double TabularPlasticity::four_third = 4.0 / 3.0;
const double TabularPlasticity::sqrt_two = std::sqrt(2.0);
const double TabularPlasticity::one_sqrt_two = 1.0 / sqrt_two;
const double TabularPlasticity::sqrt_three = std::sqrt(3.0);
const double TabularPlasticity::one_sqrt_three = 1.0 / sqrt_three;
const double TabularPlasticity::one_sixth = 1.0 / 6.0;
const double TabularPlasticity::one_ninth = 1.0 / 9.0;
const double TabularPlasticity::pi = M_PI;
const double TabularPlasticity::pi_fourth = 0.25 * pi;
const double TabularPlasticity::pi_half = 0.5 * pi;
const Matrix3 TabularPlasticity::Identity(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
const Matrix3 TabularPlasticity::Zero(0.0);

// MPM needs three functions to interact with ICE in MPMICE
// 1) p = f(rho) 2) rho = g(p) 3) C = 1/K(rho)
// Because the TabularPlasticity bulk modulus model does not have any closed
// form expressions for these functions, we will need spcial treatment.

// Requires the necessary input parameters CONSTRUCTORS
TabularPlasticity::TabularPlasticity(Uintah::ProblemSpecP& ps, Uintah::MPMFlags* mpmFlags)
  : Uintah::ConstitutiveModel(mpmFlags)
{
  // Bulk and shear modulus models
  d_elastic = Vaango::ElasticModuliModelFactory::create(ps);
  if (!d_elastic) {
    std::ostringstream desc;
    desc << "**ERROR** Internal error while creating ElasticModuliModel."
         << std::endl;
    throw InternalError(desc.str(), __FILE__, __LINE__);
  }

  // Yield condition model
  d_yield = Vaango::YieldConditionFactory::create(ps);
  if (!d_yield) {
    std::ostringstream desc;
    desc << "**ERROR** Internal error while creating YieldConditionModel."
         << std::endl;
    throw InternalError(desc.str(), __FILE__, __LINE__);
  }

  // Algorithmic parameters
  ps->getWithDefault("yield_surface_radius_scaling_factor",
                     d_cm.yield_scale_fac, 1.0);
  ps->getWithDefault("consistency_bisection_tolerance",
                     d_cm.consistency_bisection_tolerance, 1.0e-4);
  d_cm.max_bisection_iterations =
    (int)std::ceil(-10.0 * std::log(d_cm.consistency_bisection_tolerance));
  ps->getWithDefault("subcycling_characteristic_number",
                     d_cm.subcycling_characteristic_number,
                     256); // allowable subcycles

  checkInputParameters();
  initializeLocalMPMLabels();
}

void
TabularPlasticity::checkInputParameters()
{

  if (d_cm.consistency_bisection_tolerance < 1.0e-16 ||
      d_cm.consistency_bisection_tolerance > 1.0e-2) {
    ostringstream warn;
    warn << "Consistency bisection tolerance should be in range [1.0e-16, "
            "1.0e-2].  Default = 1.0e-4"
         << endl;
    throw ProblemSetupException(warn.str(), __FILE__, __LINE__);
  }

  if (d_cm.subcycling_characteristic_number < 1) {
    ostringstream warn;
    warn << "Subcycling characteristic number should be > 1. Default = 256"
         << endl;
    throw ProblemSetupException(warn.str(), __FILE__, __LINE__);
  }

  if (d_cm.yield_scale_fac < 1.0 || d_cm.yield_scale_fac > 1.0e6) {
    ostringstream warn;
    warn << "Yield surface scaling factor should be between 1 and 1.0e6. "
            "Default = 1."
         << endl;
    throw ProblemSetupException(warn.str(), __FILE__, __LINE__);
  }
}

TabularPlasticity::TabularPlasticity(const TabularPlasticity* cm)
  : ConstitutiveModel(cm)
{
  d_elastic = Vaango::ElasticModuliModelFactory::createCopy(cm->d_elastic);
  d_yield = Vaango::YieldConditionFactory::createCopy(cm->d_yield);

  // Yield surface scaling
  d_cm.yield_scale_fac = cm->d_cm.yield_scale_fac;

  // Consistency bisection
  d_cm.consistency_bisection_tolerance =
    cm->d_cm.consistency_bisection_tolerance;
  d_cm.max_bisection_iterations = cm->d_cm.max_bisection_iterations;

  // Subcycling
  d_cm.subcycling_characteristic_number =
    cm->d_cm.subcycling_characteristic_number;

  initializeLocalMPMLabels();
}

// Initialize all labels of the particle variables associated with
// TabularPlasticity.
void
TabularPlasticity::initializeLocalMPMLabels()
{
  pElasticStrainLabel = Uintah::VarLabel::create(
    "p.elasticStrain",
    Uintah::ParticleVariable<Uintah::Matrix3>::getTypeDescription());
  pElasticStrainLabel_preReloc = Uintah::VarLabel::create(
    "p.elasticStrain+",
    Uintah::ParticleVariable<Uintah::Matrix3>::getTypeDescription());

  pElasticVolStrainLabel = VarLabel::create(
    "p.elasticVolStrain", ParticleVariable<double>::getTypeDescription());
  pElasticVolStrainLabel_preReloc = VarLabel::create(
    "p.elasticVolStrain+", ParticleVariable<double>::getTypeDescription());

  pPlasticStrainLabel = Uintah::VarLabel::create(
    "p.plasticStrain",
    Uintah::ParticleVariable<Uintah::Matrix3>::getTypeDescription());
  pPlasticStrainLabel_preReloc = Uintah::VarLabel::create(
    "p.plasticStrain+",
    Uintah::ParticleVariable<Uintah::Matrix3>::getTypeDescription());

  pPlasticCumEqStrainLabel = Uintah::VarLabel::create(
    "p.plasticCumEqStrain",
    Uintah::ParticleVariable<double>::getTypeDescription());
  pPlasticCumEqStrainLabel_preReloc = Uintah::VarLabel::create(
    "p.plasticCumEqStrain+",
    Uintah::ParticleVariable<double>::getTypeDescription());

  pPlasticVolStrainLabel = Uintah::VarLabel::create(
    "p.plasticVolStrain",
    Uintah::ParticleVariable<double>::getTypeDescription());
  pPlasticVolStrainLabel_preReloc = Uintah::VarLabel::create(
    "p.plasticVolStrain+",
    Uintah::ParticleVariable<double>::getTypeDescription());
}

// DESTRUCTOR
TabularPlasticity::~TabularPlasticity()
{
  VarLabel::destroy(pElasticStrainLabel);
  VarLabel::destroy(pElasticStrainLabel_preReloc);
  VarLabel::destroy(pElasticVolStrainLabel); // Elastic Volumetric Strain
  VarLabel::destroy(pElasticVolStrainLabel_preReloc);
  VarLabel::destroy(pPlasticStrainLabel);
  VarLabel::destroy(pPlasticStrainLabel_preReloc);
  VarLabel::destroy(pPlasticCumEqStrainLabel);
  VarLabel::destroy(pPlasticCumEqStrainLabel_preReloc);
  VarLabel::destroy(pPlasticVolStrainLabel);
  VarLabel::destroy(pPlasticVolStrainLabel_preReloc);

  delete d_yield;
  delete d_elastic;
}

// adds problem specification values to checkpoint data for restart
void
TabularPlasticity::outputProblemSpec(ProblemSpecP& ps, bool output_cm_tag)
{
  ProblemSpecP cm_ps = ps;
  if (output_cm_tag) {
    cm_ps = ps->appendChild("constitutive_model");
    cm_ps->setAttribute("type", "tabular_plasticity");
  }

  d_elastic->outputProblemSpec(cm_ps);
  d_yield->outputProblemSpec(cm_ps);

  cm_ps->appendElement("yield_surface_radius_scaling_factor",
                       d_cm.yield_scale_fac);
  cm_ps->appendElement("consistency_bisection_tolerance",
                       d_cm.consistency_bisection_tolerance);
  cm_ps->appendElement("subcycling_characteristic_number",
                       d_cm.subcycling_characteristic_number);
  cm_ps->appendElement("use_disaggregation_algorithm",
                       d_cm.use_disaggregation_algorithm);
}

TabularPlasticity*
TabularPlasticity::clone()
{
  return scinew TabularPlasticity(*this);
}

// When a particle is pushed from patch to patch, carry information needed for
// the particle
void
TabularPlasticity::addParticleState(std::vector<const VarLabel*>& from,
                        std::vector<const VarLabel*>& to)
{
  from.push_back(pElasticStrainLabel);
  to.push_back(pElasticStrainLabel_preReloc);

  from.push_back(pElasticVolStrainLabel);
  to.push_back(pElasticVolStrainLabel_preReloc);

  from.push_back(pPlasticStrainLabel);
  to.push_back(pPlasticStrainLabel_preReloc);

  from.push_back(pPlasticCumEqStrainLabel);
  to.push_back(pPlasticCumEqStrainLabel_preReloc);

  from.push_back(pPlasticVolStrainLabel);
  to.push_back(pPlasticVolStrainLabel_preReloc);

  // Add the particle state for the yield condition model
  d_yield->addParticleState(from, to);
}

/*!------------------------------------------------------------------------*/
void
TabularPlasticity::addInitialComputesAndRequires(Task* task, const MPMMaterial* matl,
                                     const PatchSet* patch) const
{
  // Add the computes and requires that are common to all explicit
  // constitutive models.  The method is defined in the ConstitutiveModel
  // base class.
  const MaterialSubset* matlset = matl->thisMaterial();

  // Other constitutive model and input dependent computes and requires
  task->computes(pElasticStrainLabel, matlset);
  task->computes(pElasticVolStrainLabel, matlset);
  task->computes(pPlasticStrainLabel, matlset);
  task->computes(pPlasticCumEqStrainLabel, matlset);
  task->computes(pPlasticVolStrainLabel, matlset);

  // Add yield function variablity computes
  d_yield->addInitialComputesAndRequires(task, matl, patch);
}

/*!------------------------------------------------------------------------*/
void
TabularPlasticity::initializeCMData(const Patch* patch, const MPMMaterial* matl,
                        DataWarehouse* new_dw)
{
  // Get the particles in the current patch
  ParticleSubset* pset = new_dw->getParticleSubset(matl->getDWIndex(), patch);

  // Get the particle volume and mass
  constParticleVariable<double> pVolume, pMass;
  new_dw->get(pVolume, lb->pVolumeLabel, pset);
  new_dw->get(pMass, lb->pMassLabel, pset);

  // Initialize variables for yield function parameter variability
  d_yield->initializeLocalVariables(patch, pset, new_dw, pVolume);

  // Now initialize the other variables
  ParticleVariable<double> pdTdt;
  ParticleVariable<Matrix3> pStress;
  ParticleVariable<int> pLocalized;

  ParticleVariable<Matrix3> pElasticStrain;
  ParticleVariable<double> pElasticVolStrain;
  ParticleVariable<Matrix3> pPlasticStrain;
  ParticleVariable<double> pPlasticCumEqStrain, pPlasticVolStrain;

  new_dw->allocateAndPut(pdTdt, lb->pdTdtLabel, pset);
  new_dw->allocateAndPut(pStress, lb->pStressLabel, pset);
  new_dw->allocateAndPut(pLocalized, lb->pLocalizedMPMLabel, pset);

  new_dw->allocateAndPut(pElasticStrain, pElasticStrainLabel, pset);
  new_dw->allocateAndPut(pElasticVolStrain, pElasticVolStrainLabel, pset);
  new_dw->allocateAndPut(pPlasticStrain, pPlasticStrainLabel, pset);
  new_dw->allocateAndPut(pPlasticCumEqStrain, pPlasticCumEqStrainLabel, pset);
  new_dw->allocateAndPut(pPlasticVolStrain, pPlasticVolStrainLabel, pset);

  for (int& iter : *pset) {
    pdTdt[iter] = 0.0;
    pStress[iter] = Identity;
    pLocalized[iter] = 0;
    pElasticStrain[iter].set(0.0);
    pElasticVolStrain[iter] = 0.0;
    pPlasticStrain[iter].set(0.0);
    pPlasticCumEqStrain[iter] = 0.0;
    pPlasticVolStrain[iter] = 0.0;
  }

  // Compute timestep
  computeStableTimestep(patch, matl, new_dw);
}

// Compute stable timestep based on both the particle velocities
// and wave speed
void
TabularPlasticity::computeStableTimestep(const Patch* patch, const MPMMaterial* matl,
                             DataWarehouse* new_dw)
{
  int matID = matl->getDWIndex();

  // Compute initial elastic moduli
  ElasticModuli moduli = d_elastic->getInitialElasticModuli();
  double bulk = moduli.bulkModulus;
  double shear = moduli.shearModulus;

  // Initialize wave speed
  double c_dil = std::numeric_limits<double>::min();
  Vector dx = patch->dCell();
  Vector WaveSpeed(c_dil, c_dil, c_dil);

  // Get the particles in the current patch
  ParticleSubset* pset = new_dw->getParticleSubset(matID, patch);

  // Get particles mass, volume, and velocity
  constParticleVariable<double> pMass, pVolume;
  constParticleVariable<long64> pParticleID;
  constParticleVariable<Vector> pVelocity;

  new_dw->get(pMass, lb->pMassLabel, pset);
  new_dw->get(pVolume, lb->pVolumeLabel, pset);
  new_dw->get(pParticleID, lb->pParticleIDLabel, pset);
  new_dw->get(pVelocity, lb->pVelocityLabel, pset);

  // loop over the particles in the patch
  for (int idx : *pset) {

    // Compute wave speed + particle velocity at each particle,
    // store the maximum
    c_dil =
      std::sqrt((bulk + four_third * shear) * (pVolume[idx] / pMass[idx]));

    // std::cout << "K = " << bulk << " G = " << shear << " c_dil = " << c_dil
    // << std::endl;
    WaveSpeed =
      Vector(Max(c_dil + std::abs(pVelocity[idx].x()), WaveSpeed.x()),
             Max(c_dil + std::abs(pVelocity[idx].y()), WaveSpeed.y()),
             Max(c_dil + std::abs(pVelocity[idx].z()), WaveSpeed.z()));
  }

  // Compute the stable timestep based on maximum value of
  // "wave speed + particle velocity"
  WaveSpeed = dx / WaveSpeed;
  double delT_new = WaveSpeed.minComponent();
  new_dw->put(delt_vartype(delT_new), lb->delTLabel, patch->getLevel());
}

/**
 * Added computes/requires for computeStressTensor
 */
void
TabularPlasticity::addComputesAndRequires(Task* task, const MPMMaterial* matl,
                              const PatchSet* patches) const
{
  // Add the computes and requires that are common to all explicit
  // constitutive models.  The method is defined in the ConstitutiveModel
  // base class.
  const MaterialSubset* matlset = matl->thisMaterial();
  addSharedCRForHypoExplicit(task, matlset, patches);
  task->requires(Task::OldDW, lb->pParticleIDLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pElasticVolStrainLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pStressQSLabel, matlset, Ghost::None);
  task->computes(pElasticVolStrainLabel_preReloc, matlset);
  task->computes(pStressQSLabel_preReloc, matlset);

  // Add yield Function computes and requires
  d_yield->addComputesAndRequires(task, matl, patches);

  // Add internal variable computes and requires
  task->requires(Task::OldDW, pElasticStrainLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pPlasticStrainLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pPlasticCumEqStrainLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pPlasticVolStrainLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pBackstressLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pPorosityLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pSaturationLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pCapXLabel, matlset, Ghost::None);
  task->computes(pElasticStrainLabel_preReloc, matlset);
  task->computes(pPlasticStrainLabel_preReloc, matlset);
  task->computes(pPlasticCumEqStrainLabel_preReloc, matlset);
  task->computes(pPlasticVolStrainLabel_preReloc, matlset);
  task->computes(pBackstressLabel_preReloc, matlset);
  task->computes(pPorosityLabel_preReloc, matlset);
  task->computes(pSaturationLabel_preReloc, matlset);
  task->computes(pCapXLabel_preReloc, matlset);

// Add damage variable computes and requires
#ifdef USE_LOCAL_LOCALIZED_PVAR
  task->requires(Task::OldDW, pLocalizedLabel, matlset, Ghost::None);
#else
  task->requires(Task::OldDW, lb->pLocalizedMPMLabel, matlset, Ghost::None);
#endif
  task->requires(Task::OldDW, pP3Label, matlset, Ghost::None);
  task->requires(Task::OldDW, pCoherenceLabel, matlset, Ghost::None);
  task->requires(Task::OldDW, pTGrowLabel, matlset, Ghost::None);

#ifdef USE_LOCAL_LOCALIZED_PVAR
  task->computes(pLocalizedLabel_preReloc, matlset);
#else
  task->computes(lb->pLocalizedMPMLabel_preReloc, matlset);
#endif
  task->computes(pP3Label_preReloc, matlset);
  task->computes(pCoherenceLabel_preReloc, matlset);
  task->computes(pTGrowLabel_preReloc, matlset);
}

// ------------------------------------- BEGIN COMPUTE STRESS TENSOR FUNCTION
/**
 *  TabularPlasticity::computeStressTensor
 *  is the core of the TabularPlasticity model which computes
 *  the updated stress at the end of the current timestep along with all other
 *  required data such plastic strain, elastic strain, cap position, etc.
 */
void
TabularPlasticity::computeStressTensor(const PatchSubset* patches, const MPMMaterial* matl,
                           DataWarehouse* old_dw, DataWarehouse* new_dw)
{
  // Get the yield parameter variable labels
  std::vector<std::string> pYieldParamVarLabels =
    d_yield->getLocalVariableLabels();

  // Global loop over each patch
  for (int p = 0; p < patches->size(); p++) {

    // Declare and initial value assignment for some variables
    const Patch* patch = patches->get(p);

    // Initialize wave speed
    double c_dil = std::numeric_limits<double>::min();
    Vector WaveSpeed(c_dil, c_dil, c_dil);
    Vector dx = patch->dCell();

    // Initialize strain energy
    double se = 0.0;

    // Get particle subset for the current patch
    int matID = matl->getDWIndex();
    ParticleSubset* pset = old_dw->getParticleSubset(matID, patch);

    // Get the yield condition parameter variables
    std::vector<constParticleVariable<double>> pYieldParamVars =
      d_yield->getLocalVariables(pset, old_dw);

    // Get the internal variables
    Uintah::constParticleVariable<double> pEpv, pEpeq_old, pCapX, pP3;
    Uintah::constParticleVariable<double> pPorosity_old, pSaturation_old;
    Uintah::constParticleVariable<Uintah::Matrix3> pEe, pEp, pBackstress_old;
    old_dw->get(pEe, pElasticStrainLabel, pset);
    old_dw->get(pEp, pPlasticStrainLabel, pset);
    old_dw->get(pEpeq_old, pPlasticCumEqStrainLabel, pset);
    old_dw->get(pEpv, pPlasticVolStrainLabel, pset);
    old_dw->get(pBackstress_old, pBackstressLabel, pset);
    old_dw->get(pPorosity_old, pPorosityLabel, pset);
    old_dw->get(pSaturation_old, pSaturationLabel, pset);
    old_dw->get(pCapX, pCapXLabel, pset);

    // Allocate and put internal variables
    ParticleVariable<Matrix3> pEe_new, pEp_new, pBackstress_new;
    ParticleVariable<double> pEpv_new, pEpeq_new, pCapX_new;
    ParticleVariable<double> pPorosity_new, pSaturation_new;
    new_dw->allocateAndPut(pEe_new, pElasticStrainLabel_preReloc, pset);
    new_dw->allocateAndPut(pEp_new, pPlasticStrainLabel_preReloc, pset);
    new_dw->allocateAndPut(pEpeq_new, pPlasticCumEqStrainLabel_preReloc, pset);
    new_dw->allocateAndPut(pEpv_new, pPlasticVolStrainLabel_preReloc, pset);
    new_dw->allocateAndPut(pBackstress_new, pBackstressLabel_preReloc, pset);
    new_dw->allocateAndPut(pPorosity_new, pPorosityLabel_preReloc, pset);
    new_dw->allocateAndPut(pSaturation_new, pSaturationLabel_preReloc, pset);
    new_dw->allocateAndPut(pCapX_new, pCapXLabel_preReloc, pset);

    // Get the damage variables
    Uintah::constParticleVariable<int> pLocalized_old;
    Uintah::constParticleVariable<double> pP3_old, pCoherence_old, pTGrow_old;
#ifdef USE_LOCAL_LOCALIZED_PVAR
    old_dw->get(pLocalized_old, pLocalizedLabel, pset);
#else
    old_dw->get(pLocalized_old, lb->pLocalizedMPMLabel, pset);
#endif
    old_dw->get(pP3_old, pP3Label, pset);
    old_dw->get(pCoherence_old, pCoherenceLabel, pset);
    old_dw->get(pTGrow_old, pTGrowLabel, pset);

    // Allocate and put the damage variables
    ParticleVariable<int> pLocalized_new;
    ParticleVariable<double> pP3_new, pCoherence_new, pTGrow_new;
#ifdef USE_LOCAL_LOCALIZED_PVAR
    new_dw->allocateAndPut(pLocalized_new, pLocalizedLabel_preReloc, pset);
#else
    new_dw->allocateAndPut(pLocalized_new, lb->pLocalizedMPMLabel_preReloc,
                           pset);
#endif
    new_dw->allocateAndPut(pP3_new, pP3Label_preReloc, pset);
    new_dw->allocateAndPut(pCoherence_new, pCoherenceLabel_preReloc, pset);
    new_dw->allocateAndPut(pTGrow_new, pTGrowLabel_preReloc, pset);

    // Get the particle variables
    delt_vartype delT;
    constParticleVariable<double> pMass, // used for stable timestep
      pElasticVolStrain;
    constParticleVariable<long64> pParticleID;
    constParticleVariable<Vector> pVelocity;
    constParticleVariable<Matrix3> pDefGrad, pStress_old, pStressQS_old;

    old_dw->get(delT, lb->delTLabel, getLevel(patches));
    old_dw->get(pMass, lb->pMassLabel, pset);
    old_dw->get(pParticleID, lb->pParticleIDLabel, pset);
    old_dw->get(pVelocity, lb->pVelocityLabel, pset);
    old_dw->get(pDefGrad, lb->pDefGradLabel, pset);
    old_dw->get(pStress_old, lb->pStressLabel, pset);

    old_dw->get(pElasticVolStrain, pElasticVolStrainLabel, pset);
    old_dw->get(pStressQS_old, pStressQSLabel, pset);

    // Get the particle variables from interpolateToParticlesAndUpdate() in
    // SerialMPM
    constParticleVariable<double> pVolume;
    constParticleVariable<Matrix3> pVelGrad_new, pDefGrad_new;
    new_dw->get(pVolume, lb->pVolumeLabel_preReloc, pset);
    new_dw->get(pVelGrad_new, lb->pVelGradLabel_preReloc, pset);
    new_dw->get(pDefGrad_new, lb->pDefGradLabel_preReloc, pset);

    // Get the particle variables from compute kinematics
    ParticleVariable<double> p_q, pdTdt;
    ParticleVariable<Matrix3> pStress_new;
    new_dw->allocateAndPut(p_q, lb->p_qLabel_preReloc, pset);
    new_dw->allocateAndPut(pdTdt, lb->pdTdtLabel_preReloc, pset);
    new_dw->allocateAndPut(pStress_new, lb->pStressLabel_preReloc, pset);

    ParticleVariable<double> pElasticVolStrain_new;
    ParticleVariable<Matrix3> pStressQS_new;
    new_dw->allocateAndPut(pElasticVolStrain_new,
                           pElasticVolStrainLabel_preReloc, pset);
    new_dw->allocateAndPut(pStressQS_new, pStressQSLabel_preReloc, pset);

    // Loop over the particles of the current patch to update particle
    // stress at the end of the current timestep along with all other
    // required data such plastic strain, elastic strain, cap position, etc.
    for (int& iter : *pset) {
      particleIndex idx = iter; // patch index
      // cout<<"pID="<<pParticleID[idx]<<endl;

      // A parameter to consider the thermal effects of the plastic work which
      // is not coded in the current source code. Further development of
      // Arenisca
      // may activate this feature.
      pdTdt[idx] = 0.0;

      // Compute the symmetric part of the velocity gradient
      // std::cout << "DefGrad = " << pDefGrad_new[idx] << std::endl;
      // std::cout << "VelGrad = " << pVelGrad_new[idx] << std::endl;
      Matrix3 DD = (pVelGrad_new[idx] + pVelGrad_new[idx].Transpose()) * .5;

      // Use polar decomposition to compute the rotation and stretch tensors
      Matrix3 FF = pDefGrad[idx];
      Matrix3 RR, UU;
      FF.polarDecompositionRMB(UU, RR);

      // Compute the unrotated symmetric part of the velocity gradient
      DD = (RR.Transpose()) * (DD * RR);
#ifdef CHECK_FOR_NAN
      // if (std::abs(DD(0,0)) < 1.0e-16 || std::isnan(DD(0, 0))) {
      if (std::isnan(DD(0, 0))) {
        std::cout << " L_new = " << pVelGrad_new[idx]
                  << " F_new = " << pDefGrad_new[idx] << " F = " << FF
                  << " R = " << RR << " U = " << UU << " D = " << DD
                  << " delT = " << delT << std::endl;
        // throw InternalError("**ERROR** Zero or Nan in rate of deformation",
        // __FILE__, __LINE__);
      }
#endif

      // To support non-linear elastic properties and to allow for the fluid
      // bulk modulus
      // model to increase elastic stiffness under compression, we allow for the
      // bulk
      // modulus to vary for each substep.  To compute the required number of
      // substeps
      // we use a conservative value for the bulk modulus (the high pressure
      // limit B0+B1)
      // to compute the trial stress and use this to subdivide the strain
      // increment into
      // appropriately sized substeps.  The strain increment is a product of the
      // strain
      // rate and time step, so we pass the strain rate and subdivided time step
      // (rather
      // than a subdivided trial stress) to the substep function.

      // Compute the unrotated stress at the start of the current timestep
      Matrix3 sigma_old = (RR.Transpose()) * (pStress_old[idx] * RR);
      Matrix3 sigmaQS_old = (RR.Transpose()) * (pStressQS_old[idx] * RR);

      // std::cout << "pStress_old = " << pStress_old[idx] << std::endl
      //          << "pStressQS_old = " << pStressQS_old[idx] << std::endl;
      // std::cout << "sigma_old = " << sigma_old << std::endl
      //          << "sigmaQS_old = " << sigmaQS_old << std::endl;

      // initial assignment for the updated values of plastic strains,
      // volumetric
      // part of the plastic strain, volumetric part of the elastic strain,
      // and the backstress. tentative assumption of elasticity
      ModelState_Tabular state_old;
      state_old.particleID = pParticleID[idx];
      state_old.capX = pCapX[idx];
      state_old.pbar_w = -pBackstress_old[idx].Trace() / 3.0;
      state_old.stressTensor = sigmaQS_old;
      state_old.elasticStrainTensor = pEe[idx];
      state_old.plasticStrainTensor = pEp[idx];
      state_old.ep_cum_eq = pEpeq_old[idx];
      state_old.porosity = pPorosity_old[idx];
      state_old.saturation = pSaturation_old[idx];
      state_old.p3 = pP3_old[idx];
      state_old.coherence = pCoherence_old[idx];
      state_old.t_grow = pTGrow_old[idx];

      // std::cout << "state_old.Stress = " << state_old.stressTensor <<
      // std::endl;

      // Get the parameters of the yield surface (for variability)
      std::string yield_param_label;
      constParticleVariable<double> yield_param_var;
      BOOST_FOREACH (boost::tie(yield_param_label, yield_param_var),
                     boost::combine(pYieldParamVarLabels, pYieldParamVars)) {
        state_old.yieldParams[yield_param_label] = yield_param_var[idx];
      }

      // Compute the elastic moduli at t = t_n
      computeElasticProperties(state_old);
      // std::cout << "State old: " << state_old << std::endl;

      //---------------------------------------------------------
      // Rate-independent plastic step
      // Divides the strain increment into substeps, and calls substep function
      ModelState_Tabular state_new;
      bool isSuccess = rateIndependentPlasticUpdate(
        DD, delT, idx, pParticleID[idx], state_old, state_new);

      if (isSuccess) {

        pStressQS_new[idx] =
          state_new.stressTensor; // unrotated stress at end of step
        pCapX_new[idx] =
          state_new.capX; // hydrostatic compressive strength at end of step
        pBackstress_new[idx] =
          Identity *
          (-state_new.pbar_w); // trace of isotropic backstress at end of step
        pEe_new[idx] =
          state_new.elasticStrainTensor; // elastic strain at end of step
        pEp_new[idx] =
          state_new.plasticStrainTensor; // plastic strain at end of step
        pEpv_new[idx] =
          pEp_new[idx].Trace(); // Plastic volumetric strain at end of step
        pEpeq_new[idx] =
          state_new.ep_cum_eq; // Equivalent plastic strain at end of step

        // Elastic volumetric strain at end of step, compute from updated
        // deformation gradient.
        // H = ln(U) => tr(H) = tr(ln(U)) = ln(det(U)) = ln(sqrt(det(FT)
        // det(F))) = ln J
        pElasticVolStrain_new[idx] =
          log(pDefGrad_new[idx].Determinant()) - pEpv_new[idx];
#ifdef CHECK_ELASTIC_STRAIN
        double pEev_integrated = pEe_new[idx].Trace();
        std::cout << "Elastic volume strain error = "
                  << (pElasticVolStrain_new[idx] - pEev_integrated)
                  << " Integrated = " << pEev_integrated
                  << " Defgrad-based = " << pElasticVolStrain_new[idx]
                  << std::endl;
#endif

        pPorosity_new[idx] = state_new.porosity;
        pSaturation_new[idx] = state_new.saturation;

        pLocalized_new[idx] = pLocalized_old[idx];
        pP3_new[idx] = pP3_old[idx];
        pCoherence_new[idx] = state_new.coherence;
        pTGrow_new[idx] = state_new.t_grow;
      } else {

        // If the updateStressAndInternalVars function can't converge it will
        // return false.
        // This indicates substepping has failed, and the particle will be
        // deleted.
        pLocalized_new[idx] = -999;
        std::cout << "** WARNING ** Bad step, deleting particle"
                  << " idx = " << idx << " particleID = " << pParticleID[idx]
                  << ":" << __FILE__ << ":" << __LINE__ << std::endl;

        pStressQS_new[idx] = pStressQS_old[idx];
        pCapX_new[idx] = state_old.capX;
        pBackstress_new[idx] = pBackstress_old[idx];
        pEe_new[idx] =
          state_old.elasticStrainTensor; // elastic strain at start of step
        pEp_new[idx] =
          state_old.plasticStrainTensor; // plastic strain at start of step
        pEpv_new[idx] = pEp_new[idx].Trace();
        pEpeq_new[idx] = pEpeq_old[idx];
        pElasticVolStrain_new[idx] = pElasticVolStrain[idx];
        pPorosity_new[idx] = pPorosity_old[idx];
        pSaturation_new[idx] = pSaturation_old[idx];

        pP3_new[idx] = pP3_old[idx];
        pCoherence_new[idx] = pCoherence_old[idx];
        pTGrow_new[idx] = pTGrow_old[idx];
      }

      //---------------------------------------------------------
      // Rate-dependent plastic step
      ModelState_Tabular stateQS_old(state_old);
      stateQS_old.stressTensor = pStressQS_old[idx];
      ModelState_Tabular stateQS_new(state_new);
      stateQS_new.stressTensor = pStressQS_new[idx];

#ifdef CHECK_TRIAL_STRESS
      std::cout << "p_qs = " << stateQS_new.stressTensor.Trace() << std::endl;
#endif

      // std::cout << "State QS old";
      computeElasticProperties(stateQS_old);
      // std::cout << "State QS new";
      computeElasticProperties(stateQS_new);

      rateDependentPlasticUpdate(DD, delT, stateQS_old, stateQS_new, state_old,
                                 pStress_new[idx]);

      //---------------------------------------------------------
      // Use polar decomposition to compute the rotation and stretch tensors.
      // These checks prevent
      // failure of the polar decomposition algorithm if [F_new] has some
      // extreme values.
      Matrix3 FF_new = pDefGrad_new[idx];
      double Fmax_new = FF_new.MaxAbsElem();
      double JJ_new = FF_new.Determinant();
      if ((Fmax_new > 1.0e16) || (JJ_new < 1.0e-16) || (JJ_new > 1.0e16)) {
        pLocalized_new[idx] = -999;
        proc0cout << "Deformation gradient component unphysical: [F] = " << FF
                  << std::endl;
        proc0cout << "Resetting [F]=[I] for this step and deleting particle"
                  << " idx = " << idx << " particleID = " << pParticleID[idx]
                  << std::endl;
        Identity.polarDecompositionRMB(UU, RR);
      } else {
        FF_new.polarDecompositionRMB(UU, RR);
      }

// Dont't allow deformation gradients greater than 5.0
#ifdef CLAMP_DEF_GRAD
      if (d_cm.do_damage) {
        if (Fmax_new > 10.0 || pEpv_new[idx] > 5.0) {
          pLocalized_new[idx] = -999;
          proc0cout << "Deformation gradient or volumetric plastic strain too "
                       "large for soils:"
                    << "[F] = " << FF << "pEpv_new = " << pEpv_new[idx]
                    << std::endl;
          proc0cout << "Deleting particle"
                    << " idx = " << idx << " particleID = " << pParticleID[idx]
                    << std::endl;
        }
      }
#endif

      // Compute the rotated dynamic and quasistatic stress at the end of the
      // current timestep
      pStress_new[idx] = (RR * pStress_new[idx]) * (RR.Transpose());
      pStressQS_new[idx] = (RR * pStressQS_new[idx]) * (RR.Transpose());

      // std::cout << "pStress_new = " << pStress_new[idx]
      //          << "pStressQS_new = " << pStressQS_new[idx] << std::endl;

      // Compute wave speed + particle velocity at each particle, store the
      // maximum
      // std::cout << "State QS new rotated";
      computeElasticProperties(stateQS_new);
      double bulk = stateQS_new.bulkModulus;
      double shear = stateQS_new.shearModulus;
      double rho_cur = pMass[idx] / pVolume[idx];
      c_dil = sqrt((bulk + four_third * shear) / rho_cur);
      // std::cout << "K = " << bulk << " G = " << shear << " c_dil = " << c_dil
      // << std::endl;
      WaveSpeed =
        Vector(Max(c_dil + std::abs(pVelocity[idx].x()), WaveSpeed.x()),
               Max(c_dil + std::abs(pVelocity[idx].y()), WaveSpeed.y()),
               Max(c_dil + std::abs(pVelocity[idx].z()), WaveSpeed.z()));

      // Compute artificial viscosity term
      if (flag->d_artificial_viscosity) {
        double dx_ave = (dx.x() + dx.y() + dx.z()) * one_third;
        double c_bulk = sqrt(bulk / rho_cur);
        p_q[idx] = artificialBulkViscosity(DD.Trace(), c_bulk, rho_cur, dx_ave);
      } else {
        p_q[idx] = 0.;
      }

      // Update p3
      if (d_cm.use_disaggregation_algorithm) {
        double phi0 = d_fluidParam.phi0;
        double p3 =
          -std::log(pMass[iter] /
                    (pVolume[iter] * (matl->getInitialDensity())) * (1 - phi0));
        double phi = 1.0 - std::exp(-p3);
        pP3_new[idx] = (phi > phi0) ? p3 : pP3_old[idx];
      }

      // Compute the averaged stress
      Matrix3 AvgStress = (pStress_new[idx] + pStress_old[idx]) * 0.5;

      // Compute the strain energy increment associated with the particle
      double e =
        (DD(0, 0) * AvgStress(0, 0) + DD(1, 1) * AvgStress(1, 1) +
         DD(2, 2) * AvgStress(2, 2) +
         2.0 * (DD(0, 1) * AvgStress(0, 1) + DD(0, 2) * AvgStress(0, 2) +
                DD(1, 2) * AvgStress(1, 2))) *
        pVolume[idx] * delT;

      // Accumulate the total strain energy
      // MH! Note the initialization of se needs to be fixed as it is currently
      // reset to 0
      se += e;

    } // End particle set loop

    // Update yield condition parameter variability
    if (d_cm.do_damage) {
      // Each particle has a different set of parameters which are scaled by
      // the coherence
      d_yield->updateLocalVariables(pset, old_dw, new_dw, pCoherence_old,
                                    pCoherence_new);
    } else {
      // Each particle has a different set of parameters which remain
      // constant through the simulation
      d_yield->copyLocalVariables(pset, old_dw, new_dw);
    }

    // Compute the stable timestep based on maximum value of "wave speed +
    // particle velocity"
    WaveSpeed =
      dx / WaveSpeed; // Variable now holds critical timestep (not speed)

    double delT_new = WaveSpeed.minComponent();

    // Put the stable timestep and total strain enrgy
    new_dw->put(delt_vartype(delT_new), lb->delTLabel, patch->getLevel());
    if (flag->d_reductionVars->accStrainEnergy ||
        flag->d_reductionVars->strainEnergy) {
      new_dw->put(sum_vartype(se), lb->StrainEnergyLabel);
    }
  }
} // -----------------------------------END OF COMPUTE STRESS TENSOR FUNCTION

/**
* Function:
*   rateIndependentPlasticUpdate
*
* Purpose:
*   Divides the strain increment into substeps, and calls substep function
*   All stress values within computeStep are quasistatic.
*/
bool
TabularPlasticity::rateIndependentPlasticUpdate(const Matrix3& D, const double& delT,
                                    particleIndex idx, long64 pParticleID,
                                    const ModelState_Tabular& state_old,
                                    ModelState_Tabular& state_new)
{
#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << "Rate independent update:" << std::endl;
  std::cout << " D = " << D << " delT = " << delT << std::endl;
  std::cout << "\t State old:" << state_old << std::endl;
#endif

  // Compute the strain increment
  Matrix3 strain_inc = D * delT;
  if (strain_inc.Norm() < 1.0e-30) {
    state_new = state_old;
    return true;
  }

  // Compute the trial stress
  Matrix3 stress_trial = computeTrialStress(state_old, strain_inc);

  // Set up a trial state, update the stress invariants, and compute elastic
  // properties
  ModelState_Tabular state_trial(state_old);
  state_trial.stressTensor = stress_trial;
  computeElasticProperties(state_trial);

#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << "\t strain_inc = " << strain_inc << std::endl;
  std::cout << "\t State trial:" << state_trial << std::endl;
#endif

  // Determine the number of substeps (nsub) based on the magnitude of
  // the trial stress increment relative to the characteristic dimensions
  // of the yield surface.  Also compare the value of the pressure dependent
  // elastic properties at sigma_old and sigma_trial and adjust nsub if
  // there is a large change to ensure an accurate solution for nonlinear
  // elasticity even with fully elastic loading.
  int nsub = computeStepDivisions(idx, pParticleID, state_old, state_trial);

  // * Upon FAILURE *
  // Delete the particle if the number of substeps is unreasonable
  // Send ParticleDelete Flag to Host Code, Store Inputs to particle data:
  // input values for sigma_new, X_new, Zeta_new, ep_new, along with error flag
  if (nsub < 0) {
    state_new = state_old;
    proc0cout << "Step Failed: Particle idx = " << idx
              << " ID = " << pParticleID << " because nsub = " << nsub
              << std::endl;
    // bool success  = false;
    return false;
  }

  // Compute a subdivided time step:
  // Loop at least once or until substepping is successful
  const int CHI_MAX = 5; // max allowed subcycle multiplier

  double dt = delT / nsub; // substep time increment

  int chi = 1; // subcycle multiplier
  double tlocal = 0.0;
  bool isSuccess = false;

  // Set up the initial states for the substeps
  ModelState_Tabular state_k_old(state_old);
  ModelState_Tabular state_k_new(state_old);
  do {

    //  Call substep function {sigma_new, ep_new, X_new, Zeta_new}
    //    = computeSubstep(D, dt, sigma_substep, ep_substep, X_substep,
    //    Zeta_substep)
    //  Repeat while substeps continue to be successful
    isSuccess = computeSubstep(D, dt, state_k_old, state_k_new);
    if (isSuccess) {

      tlocal += dt;

#ifdef WRITE_YIELD_SURF
      std::cout << "K = " << state_k_old.bulkModulus << std::endl;
      std::cout << "G = " << state_k_old.shearModulus << std::endl;
      std::cout << "capX = " << state_k_new.capX << std::endl;
      std::cout << "pbar_w = " << state_k_new.pbar_w << std::endl;
#endif

      state_k_old = state_k_new;

#ifdef WRITE_YIELD_SURF
      Matrix3 sig = state_k_new.stressTensor;
      std::cout << "sigma_new = np.array([[" << sig(0, 0) << "," << sig(0, 1)
                << "," << sig(0, 2) << "],[" << sig(1, 0) << "," << sig(1, 1)
                << "," << sig(1, 2) << "],[" << sig(2, 0) << "," << sig(2, 1)
                << "," << sig(2, 2) << "]])" << std::endl;
      std::cout << "plot_stress_state(K, G, sigma_trial, sigma_new, 'b')"
                << std::endl;
#endif

    } else {

      // Substepping has failed. Take tenth the timestep.
      dt /= 10.0;

      // Increase chi to keep track of the number of times the timstep has
      // been reduced
      chi += 1;
      if (chi > CHI_MAX) {
        state_new = state_k_old;
        proc0cout << "Substep failed because chi = " << chi << " > " << CHI_MAX
                  << std::endl;
        return isSuccess; // isSuccess = false;
      }

      proc0cout << "**WARNING** Decreasing substep time increment to " << dt
                << " because computeSubstep failed." << std::endl;
    }
#ifdef CHECK_SUBSTEP
    if (tlocal < delT) {
      std::cout << "tlocal = " << tlocal << " delT = " << delT
                << " nsub = " << nsub << std::endl;
    }
#endif
  } while (tlocal < delT);

  state_new = state_k_new;

#ifdef CHECK_INTERNAL_VAR_EVOLUTION
  // if (state_old.particleID == 3377699720593411) {
  std::cout << "rateIndependentPlasticUpdate: "
            << " pbar_w_old = " << state_old.pbar_w
            << " pbar_w_new = " << state_new.pbar_w
            << " Xbar_old = " << -state_old.capX
            << " Xbar_new = " << -state_new.capX
            << " Xeff_old = " << -state_old.capX - state_old.pbar_w
            << " Xeff_new = " << -state_new.capX - state_new.pbar_w
            << " ep_v_old = " << state_old.ep_v
            << " ep_v_new = " << state_new.ep_v << std::endl;
//}
#endif

  return isSuccess;
}

/**
 * Method: computeElasticProperties
 *
 * Purpose:
 *   Compute the bulk and shear modulus at a given state
 *
 * Side effects:
 *   **WARNING** Also computes stress invariants and plastic strain invariants
 */
void
TabularPlasticity::computeElasticProperties(ModelState_Tabular& state)
{
  state.updateStressInvariants();
  state.updatePlasticStrainInvariants();
  ElasticModuli moduli = d_elastic->getCurrentElasticModuli(&state);
  state.bulkModulus = moduli.bulkModulus;
  state.shearModulus = moduli.shearModulus;

  // Scale moduli using reference porosity (proxy for reference density)
  state.bulkModulus *= d_modulus_scale_fac;
  state.shearModulus *= d_modulus_scale_fac;

  // Modify the moduli if damage is being used
  if (d_cm.do_damage) {
    state.bulkModulus *= (state.coherence + 1.0e-16);
    state.shearModulus *= (state.coherence + 1.0e-16);
  }

  // Modify the moduli if disaggregation is being used
  if (d_cm.use_disaggregation_algorithm) {
    // double phi = 1.0 - std::exp(-state.p3);
    double phi = std::max(state.porosity, 1.0 - std::exp(-state.p3));
    double scale = (phi > d_fluidParam.phi0)
                     ? std::max((1.0 - phi) / (1.0 + phi), 0.00001)
                     : 1.0;
    /*
    if (state.particleID == 3659178992271360) {
      std::cout << "bulk modulus scale factor = " << scale << std::endl;
    }
    */
    state.bulkModulus *= scale;
    state.shearModulus *= scale;
  }
}

/**
 * Method: computeTrialStress
 * Purpose:
 *   Compute the trial stress for some increment in strain assuming linear
 * elasticity
 *   over the step.
 */
Matrix3
TabularPlasticity::computeTrialStress(const ModelState_Tabular& state_old,
                          const Matrix3& strain_inc)
{
  // Compute the trial stress
  Matrix3 stress_old = state_old.stressTensor;
  Matrix3 deps_iso = Identity * (one_third * strain_inc.Trace());
  Matrix3 deps_dev = strain_inc - deps_iso;
  Matrix3 stress_trial = stress_old + deps_iso * (3.0 * state_old.bulkModulus) +
                         deps_dev * (2.0 * state_old.shearModulus);
//#ifdef CHECK_TRIAL_STRESS
#ifdef CHECK_FOR_NAN
  if (std::isnan(stress_trial(0, 0))) {
    std::cout << " stress_old = " << stress_old
              << " stress_trial = " << stress_trial
              << " p_trial = " << stress_trial.Trace() / 3.0
              << " strain_inc = " << strain_inc << " deps_iso = " << deps_iso
              << " deps_dev = " << deps_dev << " K = " << state_old.bulkModulus
              << " G = " << state_old.shearModulus << std::endl;
    throw InternalError("**ERROR** Nan in compute trial stress.", __FILE__,
                        __LINE__);
  }
#endif
  //#endif

  return stress_trial;
}

/**
 * Method: computeStepDivisions
 * Purpose:
 *   Compute the number of step divisions (substeps) based on a comparison
 *   of the trial stress relative to the size of the yield surface, as well
 *   as change in elastic properties between sigma_n and sigma_trial.
 *
 * Caveat:  Uses the mean values of the yield condition parameters.
 */
int
TabularPlasticity::computeStepDivisions(particleIndex idx, long64 particleID,
                            const ModelState_Tabular& state_old,
                            const ModelState_Tabular& state_trial)
{

  // Get the yield parameters
  double PEAKI1;
  // double STREN;
  try {
    PEAKI1 = state_old.yieldParams.at("PEAKI1");
    // STREN = state_old.yieldParams.at("STREN");
  } catch (std::out_of_range) {
    std::ostringstream err;
    err << "**ERROR** Could not find yield parameters PEAKI1 and STREN"
        << std::endl;
    for (auto param : state_old.yieldParams) {
      err << param.first << " " << param.second << std::endl;
    }
    throw InternalError(err.str(), __FILE__, __LINE__);
  }

  // Compute change in bulk modulus:
  double bulk_old = state_old.bulkModulus;
  double bulk_trial = state_trial.bulkModulus;

  int n_bulk =
    std::max(std::ceil(std::abs(bulk_old - bulk_trial) / bulk_old), 1.0);
#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << "bulk_old = " << bulk_old << " bulk_trial = " << bulk_trial
            << " n_bulk = " << n_bulk << std::endl;
#endif

  // Compute trial stress increment relative to yield surface size:
  Matrix3 d_sigma = state_trial.stressTensor - state_old.stressTensor;
  double X_eff = state_old.capX + 3.0 * state_old.pbar_w;
  double I1_size = 0.5 * (PEAKI1 - X_eff);
  double J2_size = d_yield->evalYieldConditionMax(&state_old);
  double size = std::max(I1_size, J2_size);
  // if (STREN > 0.0){
  //  size = std::min(I1_size, STREN);
  //}
  size *= d_cm.yield_scale_fac;
  int n_yield = ceil(d_sigma.Norm() / size);

#ifdef CHECK_FOR_NAN_EXTRA
  // if (state_old.particleID == 3377699720593411) {
  proc0cout << "bulk_old = " << bulk_old << " bulk_trial = " << bulk_trial
            << " n_bulk = " << n_bulk << std::endl;
  proc0cout << "PEAKI1 = " << PEAKI1 << " capX_old = " << state_old.capX
            << " pbar_w_old = " << state_old.pbar_w << " size = " << size
            << " |dsigma| = " << d_sigma.Norm() << " n_yield = " << n_yield
            << std::endl;
//}
#endif

  // nsub is the maximum of the two values.above.  If this exceeds allowable,
  // throw warning and delete particle.
  int nsub = std::max(std::max(n_bulk, n_yield), 1);
  int nmax = d_cm.subcycling_characteristic_number;

  if (nsub > nmax) {
    proc0cout << "\n **WARNING** Too many substeps needed for particle "
              << " idx = " << idx << " particle ID = " << particleID
              << std::endl;
    proc0cout << "\t" << __FILE__ << ":" << __LINE__ << std::endl;
    proc0cout << "\t State at t_n: " << state_old;

    proc0cout << "\t Trial state at t_n+1: " << state_trial;

    proc0cout << "\t Ratio of trial bulk modulus to t_n bulk modulus " << n_bulk
              << std::endl;

    proc0cout << "\t ||sig_trial - sigma_n|| " << d_sigma.Norm() << std::endl;
    proc0cout << "\t Yield surface radius in I1-space: " << size << std::endl;
    proc0cout << "\t Ratio of ||sig_trial - sigma_n|| and "
              << d_cm.yield_scale_fac << "*y.s. radius: " << n_yield
              << std::endl;
    proc0cout << "\t PEAKI1 = " << PEAKI1 << " X_eff = " << X_eff
              << " X = " << state_old.capX << " pbar_w = " << state_old.pbar_w
              << std::endl;
    proc0cout << "\t I1_size = " << I1_size << " J2_size = " << J2_size
              << std::endl;

    proc0cout << "** BECAUSE** nsub = " << nsub << " > "
              << d_cm.subcycling_characteristic_number
              << " : Probably too much tension in the particle." << std::endl;
    nsub = -1;
  }
  return nsub;
}

/**
 * Method: computeSubstep
 *
 * Purpose:
 *   Computes the updated stress state for a substep that may be either
 *   elastic, plastic, or partially elastic.
 */
bool
TabularPlasticity::computeSubstep(const Matrix3& D, const double& dt,
                      const ModelState_Tabular& state_k_old,
                      ModelState_Tabular& state_k_new)
{
#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << "\t D = " << D << std::endl;
  std::cout << "\t dt:" << dt << std::endl;
#endif

  // Compute the trial stress
  Matrix3 deltaEps = D * dt;
  Matrix3 stress_k_trial = computeTrialStress(state_k_old, deltaEps);

#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << "\t deltaEps = " << deltaEps << std::endl;
  std::cout << "\t Stress k trial:" << stress_k_trial << std::endl;
#endif

#ifdef WRITE_YIELD_SURF
  // std::cout << "Inside computeSubstep:" << std::endl;
  std::cout << "K = " << state_k_old.bulkModulus << std::endl;
  std::cout << "G = " << state_k_old.shearModulus << std::endl;
  std::cout << "capX = " << state_k_old.capX << std::endl;
  std::cout << "pbar_w = " << state_k_old.pbar_w << std::endl;
  Matrix3 sig = stress_k_trial;
  std::cout << "sigma_trial = np.array([[" << sig(0, 0) << "," << sig(0, 1)
            << "," << sig(0, 2) << "],[" << sig(1, 0) << "," << sig(1, 1) << ","
            << sig(1, 2) << "],[" << sig(2, 0) << "," << sig(2, 1) << ","
            << sig(2, 2) << "]])" << std::endl;
  std::cout << "plot_stress_state(K, G, sigma_new, sigma_trial, 'r')"
            << std::endl;
// std::cout << "\t computeSubstep: sigma_old = " << state_k_old.stressTensor
//         << " sigma_trial = " << stress_trial
//         << " D = " << D << " dt = " << dt
//         << " deltaEps = " << deltaEps << std::endl;
#endif

  // Set up a trial state, update the stress invariants
  ModelState_Tabular state_k_trial(state_k_old);
  state_k_trial.stressTensor = stress_k_trial;

  // Compute elastic moduli at trial stress state
  // and update stress invariants
  computeElasticProperties(state_k_trial);

  // Evaluate the yield function at the trial stress:
  int yield = (int)d_yield->evalYieldCondition(&state_k_trial);

  // std::cout << "Has yielded ? 1 = Yes, -1 = No." << yield << std::endl;
  // std::cout << "computeSubstep:Elastic:sigma_new = " <<
  // state_k_new.stressTensor
  //          << " pbar_w_trial = " << state_k_trial.pbar_w
  //          << " Xbar_trial = " << -state_k_trial.capX
  //          << " Xeff_trial = " << -state_k_trial.capX - state_k_trial.pbar_w
  //          << " ep_v_trial = " << state_k_trial.ep_v
  //          << std::endl;

  // Elastic substep
  if (!(yield == 1)) {
    state_k_new = state_k_trial;
    state_k_new.elasticStrainTensor += deltaEps;

#ifdef CHECK_INTERNAL_VAR_EVOLUTION
    std::cout << "computeSubstep:Elastic:sigma_new = "
              << state_k_new.stressTensor
              << " pbar_w_trial = " << state_k_trial.pbar_w
              << " Xbar_trial = " << -state_k_trial.capX
              << " Xeff_trial = " << -state_k_trial.capX - state_k_trial.pbar_w
              << " ep_v_trial = " << state_k_trial.ep_v << std::endl;
#endif

    return true; // bool isSuccess = true;
  }

#ifdef DEBUG_YIELD_BISECTION_R
  std::cout << "before_non_hardening_return  = 1" << std::endl;
  std::cout << "I1_eff = " << state_k_old.I1_eff << std::endl;
  std::cout << "sqrt_J2 = " << state_k_old.sqrt_J2 << std::endl;
#endif
  // Elastic-plastic or fully-plastic substep
  // Compute non-hardening return to initial yield surface:
  // std::cout << "\t Doing nonHardeningReturn\n";
  Matrix3 sig_fixed(0.0); // final stress state for non-hardening return
  Matrix3 deltaEps_e_fixed(
    0.0); // increment in elastic strain for non-hardening return
  Matrix3 deltaEps_p_fixed(
    0.0); // increment in plastic strain for non-hardening return
  bool isSuccess =
    nonHardeningReturn(deltaEps, state_k_old, state_k_trial, sig_fixed,
                       deltaEps_e_fixed, deltaEps_p_fixed);
  if (!isSuccess) {
    proc0cout << "**WARNING** nonHardeningReturn has failed." << std::endl;
    return isSuccess;
  }

  // Do "consistency bisection"
  // std::cout << "\t Doing consistencyBisection\n";
  state_k_new = state_k_old;
  isSuccess = consistencyBisectionSimplified(
    deltaEps, state_k_old, state_k_trial, deltaEps_e_fixed, deltaEps_p_fixed,
    sig_fixed, state_k_new);

#ifdef DEBUG_INTERNAL_VAR_EVOLUTION
  std::cout << "computeSubstep: "
            << " pbar_w_old = " << state_k_old.pbar_w
            << " pbar_w_new = " << state_k_new.pbar_w
            << " Xbar_old = " << -state_k_old.capX
            << " Xbar_new = " << -state_k_new.capX
            << " Xeff_old = " << -state_k_old.capX - state_k_old.pbar_w
            << " Xeff_new = " << -state_k_new.capX - state_k_new.pbar_w
            << " ep_v_old = " << state_k_old.ep_v
            << " ep_v_new = " << state_k_new.ep_v << std::endl;
#endif

#ifdef DEBUG_YIELD_BISECTION_R
  std::cout << "after_consistency_bisection  = 1" << std::endl;
  std::cout << "I1_eff = " << state_k_new.I1_eff << std::endl;
  std::cout << "sqrt_J2 = " << state_k_new.sqrt_J2 << std::endl;
#endif

  // Update damage parameters
  if (isSuccess) {
    updateDamageParameters(D, dt, state_k_old, state_k_new);
  } else {
    proc0cout << "consistency bisection has failed in " << __FILE__ << ":"
              << __LINE__ << std::endl;
  }

  return isSuccess;

} //===================================================================

/**
 * Method: nonHardeningReturn
 * Purpose:
 *   Computes a non-hardening return to the yield surface in the meridional
 * profile
 *   (constant Lode angle) based on the current values of the internal state
 * variables
 *   and elastic properties.  Returns the updated stress and  the increment in
 * plastic
 *   strain corresponding to this return.
 *
 *   NOTE: all values of r and z in this function are transformed!
 */
bool
TabularPlasticity::nonHardeningReturn(const Uintah::Matrix3& strain_inc,
                          const ModelState_Tabular& state_k_old,
                          const ModelState_Tabular& state_k_trial,
                          Uintah::Matrix3& sig_fixed,
                          Uintah::Matrix3& elasticStrain_inc_fixed,
                          Uintah::Matrix3& plasticStrain_inc_fixed)
{
  // Get the yield parameters
  double BETA;
  double PEAKI1;
  try {
    BETA = state_k_old.yieldParams.at("BETA");
    PEAKI1 = state_k_old.yieldParams.at("PEAKI1");
  } catch (std::out_of_range) {
    std::ostringstream err;
    err << "**ERROR** Could not find yield parameters BETA and PEAKI1"
        << std::endl;
    for (auto param : state_k_old.yieldParams) {
      err << param.first << " " << param.second << std::endl;
    }
    throw InternalError(err.str(), __FILE__, __LINE__);
  }

  // Compute ratio of bulk and shear moduli
  double K_old = state_k_old.bulkModulus;
  double G_old = state_k_old.shearModulus;
  const double sqrt_K_over_G_old = std::sqrt(1.5 * K_old / G_old);
#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << " K_old = " << K_old << " G_old = " << G_old << std::endl;
#endif

  // Save the r and z Lode coordinates for the trial stress state
  double r_trial = BETA * state_k_trial.rr;
  double z_eff_trial = state_k_trial.zz_eff;
#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << " state_k_trial " << state_k_trial << std::endl;
#endif

  // Compute transformed r coordinates
  double rprime_trial = r_trial * sqrt_K_over_G_old;
#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << " z_trial = " << z_eff_trial
            << " r_trial = " << rprime_trial / sqrt_K_over_G_old << std::endl;
#endif

  // Find closest point
  double z_eff_closest = 0.0, rprime_closest = 0.0;
  d_yield->getClosestPoint(&state_k_old, z_eff_trial, rprime_trial,
                           z_eff_closest, rprime_closest);
#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << " z_eff_closest = " << z_eff_closest
            << " r_closest = " << rprime_closest / sqrt_K_over_G_old
            << std::endl;
#endif

  // Compute updated invariants of total stress
  double I1_closest = std::sqrt(3.0) * z_eff_closest - 3.0 * state_k_old.pbar_w;
  double sqrtJ2_closest =
    1.0 / (sqrt_K_over_G_old * BETA * sqrt_two) * rprime_closest;

#ifdef CHECK_FOR_NAN_EXTRA
  std::cout << "I1_eff_closest = " << I1_closest + 3.0 * state_k_old.pbar_w
            << " sqrtJ2_closest = " << sqrtJ2_closest << std::endl;
  std::cout << "Trial state = " << state_k_trial << std::endl;
#endif
#ifdef CHECK_HYDROSTATIC_TENSION
  if (I1_closest < 0) {
    std::cout << "I1_eff_closest = " << I1_closest + 3.0 * state_k_old.pbar_w
              << " sqrtJ2_closest = " << sqrtJ2_closest << std::endl;
    std::cout << "Trial state = " << state_k_trial << std::endl;
  }
#endif

  // Compute new stress
  Matrix3 sig_dev = state_k_trial.deviatoricStressTensor;
  if (state_k_trial.sqrt_J2 > 0.0) {
    // double z_close = I1_closest*one_sqrt_three;
    // double r_close = sqrtJ2_closest*sqrt_two;
    // double norm_Identity = sqrt_three;
    // double norm_s_trial = sig_dev.Norm();
    // sig_fixed = Identity*(z_close/norm_Identity) +
    // sig_dev*(r_close/norm_s_trial);
    sig_fixed = one_third * I1_closest * Identity +
                (sqrtJ2_closest / state_k_trial.sqrt_J2) * sig_dev;
  } else {
    // double z_close = I1_closest*one_sqrt_three;
    // double norm_Identity = sqrt_three;
    // sig_fixed = Identity*(z_close/norm_Identity) + sig_dev;
    sig_fixed = one_third * I1_closest * Identity + sig_dev;
  }

  // Compute new plastic strain increment
  //  d_ep = d_e - [C]^-1:(sigma_new-sigma_old)
  Matrix3 sig_inc = sig_fixed - state_k_old.stressTensor;
  Matrix3 sig_inc_iso = one_third * sig_inc.Trace() * Identity;
  Matrix3 sig_inc_dev = sig_inc - sig_inc_iso;
  elasticStrain_inc_fixed =
    sig_inc_iso * (one_third / K_old) + sig_inc_dev * (0.5 / G_old);
  plasticStrain_inc_fixed = strain_inc - elasticStrain_inc_fixed;

#ifdef CHECK_ELASTIC_STRAIN
  // std::cout << "Non-hardening:\n"
  //          << "\t Delta sig = " << sig_inc << std::endl
  //          << "\t Delta Eps_e = " << elasticStrain_inc_fixed << std::endl;
  std::cout
    << "press = " << sig_fixed.Trace() / 3.0 << " K = " << K_old << " ev_e = "
    << (state_k_old.elasticStrainTensor + elasticStrain_inc_fixed).Trace()
    << std::endl;
#endif

  // Compute volumetric plastic strain and compare with p3
  Matrix3 eps_p = state_k_old.plasticStrainTensor + plasticStrain_inc_fixed;
  double ep_v = eps_p.Trace();
  if (ep_v < 0.0) {
    if (-ep_v > state_k_old.p3) {
      proc0cout << "**WARNING** Nonhardening return has failed because "
                << " epsbar_p_v > p3 : " << -ep_v << " > " << state_k_old.p3
                << std::endl;
      proc0cout << " K_old = " << K_old << " G_old = " << G_old << std::endl;
      proc0cout << " state_k_trial " << state_k_trial << std::endl;
      proc0cout << " z_trial = " << z_eff_trial
                << " r_trial = " << rprime_trial / sqrt_K_over_G_old
                << std::endl;
      proc0cout << " z_eff_closest = " << z_eff_closest
                << " r_closest = " << rprime_closest / sqrt_K_over_G_old
                << std::endl;
      proc0cout << "Delta eps = " << strain_inc << std::endl;
      proc0cout << "sig_n = " << state_k_old.stressTensor << std::endl;
      proc0cout << "sig_n+1 = " << sig_fixed << std::endl;
      proc0cout << "Delta sig = " << sig_inc << std::endl;
      proc0cout << "Delta sig_iso = " << sig_inc_iso << std::endl;
      proc0cout << "Delta sig_dev = " << sig_inc_dev << std::endl;
      proc0cout << "Delta eps_e = " << elasticStrain_inc_fixed << std::endl;
      proc0cout << "Delta eps_p = " << plasticStrain_inc_fixed << std::endl;
      proc0cout << "I1_J2_trial = [" << state_k_trial.I1_eff << " "
                << state_k_trial.sqrt_J2 << "];" << std::endl;
      proc0cout << "I1_J2_closest = [" << I1_closest + 3.0 * state_k_old.pbar_w
                << " " << sqrtJ2_closest << "];" << std::endl;
      proc0cout << "plot([I1 I1_J2_closest(1)],[sqrtJ2 I1_J2_closest(2)],'gx')"
                << ";" << std::endl;
      proc0cout << "plot([I1_J2_trial(1) I1_J2_closest(1)],[I1_J2_trial(2) "
                   "I1_J2_closest(2)],'r-')"
                << ";" << std::endl;

      return false; // The plastic volume strain is too large, try again
    }
  }

#ifdef CHECK_YIELD_SURFACE_NORMAL
  // if (state_k_old.particleID == 3377699720593411) {
  std::cout << "Delta eps = " << strain_inc << std::endl;
  std::cout << "Trial state = " << state_k_trial << std::endl;
  std::cout << "Delta sig = " << sig_inc << std::endl;
  std::cout << "Delta sig_iso = " << sig_inc_iso << std::endl;
  std::cout << "Delta sig_dev = " << sig_inc_dev << std::endl;
  std::cout << "Delta eps_e = " << elasticStrain_inc_fixed << std::endl;
  std::cout << "Delta eps_p = " << plasticStrain_inc_fixed << std::endl;

  // Test normal to yield surface
  ModelState_Tabular state_test(state_k_old);
  state_test.stressTensor = sig_fixed;
  state_test.updateStressInvariants();

  Matrix3 df_dsigma;
  d_yield->eval_df_dsigma(Identity, &state_test, df_dsigma);
  std::cout << "df_dsigma = " << df_dsigma << std::endl;
  std::cout << "ratio = [" << plasticStrain_inc_fixed(0, 0) / df_dsigma(0, 0)
            << "," << plasticStrain_inc_fixed(1, 1) / df_dsigma(1, 1) << ","
            << plasticStrain_inc_fixed(2, 2) / df_dsigma(2, 2) << std::endl;

  // Compute CN = C:df_dsigma
  double lambda = state_test.bulkModulus - 2.0 / 3.0 * state_test.shearModulus;
  double mu = state_test.shearModulus;
  Matrix3 CN = Identity * (lambda * df_dsigma.Trace()) + df_dsigma * (2.0 * mu);
  Matrix3 sig_diff = state_k_trial.stressTensor - sig_fixed;
  std::cout << "sig_trial = [" << state_k_trial.stressTensor << "];"
            << std::endl;
  std::cout << "sig_n+1 = [" << sig_fixed << "];" << std::endl;
  std::cout << "sig_trial - sig_n+1 = [" << sig_diff << "];" << std::endl;
  std::cout << "C_df_dsigma = [" << CN << "];" << std::endl;
  std::cout << "sig ratio = [" << sig_diff(0, 0) / CN(0, 0) << " "
            << sig_diff(1, 1) / CN(1, 1) << " " << sig_diff(2, 2) / CN(2, 2)
            << "];" << std::endl;

  // Compute a test stress to check normal
  Matrix3 sig_test = sig_fixed + df_dsigma * sig_diff(0, 0);
  ModelState_Tabular state_sig_test(state_k_old);
  state_sig_test.stressTensor = sig_test;
  state_sig_test.updateStressInvariants();
  std::cout << "I1 = " << state_sig_test.I1_eff << ";" << std::endl;
  std::cout << "sqrtJ2 = " << state_sig_test.sqrt_J2 << ";" << std::endl;
  std::cout << "I1_J2_trial = [" << state_k_trial.I1_eff << " "
            << state_k_trial.sqrt_J2 << "];" << std::endl;
  std::cout << "I1_J2_closest = [" << I1_closest + 3.0 * state_k_old.pbar_w
            << " " << sqrtJ2_closest << "];" << std::endl;
  std::cout << "plot([I1 I1_J2_closest(1)],[sqrtJ2 I1_J2_closest(2)],'gx-')"
            << ";" << std::endl;
  std::cout << "plot([I1_J2_trial(1) I1_J2_closest(1)],[I1_J2_trial(2) "
               "I1_J2_closest(2)],'r-')"
            << ";" << std::endl;

  // Check actual location of projected point
  Matrix3 sig_test_actual =
    state_k_trial.stressTensor - CN * (std::abs(sig_diff(0, 0) / CN(0, 0)));
  state_sig_test.stressTensor = sig_test_actual;
  state_sig_test.updateStressInvariants();
  std::cout << "I1 = " << state_sig_test.I1_eff << ";" << std::endl;
  std::cout << "sqrtJ2 = " << state_sig_test.sqrt_J2 << ";" << std::endl;
  std::cout << "plot([I1 I1_J2_trial(1)],[sqrtJ2 I1_J2_trial(2)],'rx')"
            << ";" << std::endl;
//}
#endif

#ifdef CHECK_FOR_NAN
  if (std::isnan(sig_fixed(0, 0))) {
    std::cout << " K_old = " << K_old << " G_old = " << G_old << std::endl;
    std::cout << " z_trial = " << z_eff_trial
              << " r_trial = " << rprime_trial / sqrt_K_over_G_old << std::endl;
    std::cout << " z_eff_closest = " << z_eff_closest
              << " r_closest = " << rprime_closest / sqrt_K_over_G_old
              << std::endl;
    std::cout << "I1_eff_closest = " << I1_closest + 3.0 * state_k_old.pbar_w
              << " sqrtJ2_closest = " << sqrtJ2_closest << std::endl;
    std::cout << "Trial state = " << state_k_trial << std::endl;
    std::cout << "\t\t\t sig_fixed = " << sig_fixed << std::endl;
    std::cout << "\t\t\t I1_closest = " << I1_closest << std::endl;
    std::cout << "\t\t\t sqrtJ2_closest = " << sqrtJ2_closest << std::endl;
    std::cout << "\t\t\t state_k_trial.sqrt_J2 = " << state_k_trial.sqrt_J2
              << std::endl;
    std::cout << "\t\t\t sig_dev = " << sig_dev << std::endl;
    std::cout << "\t\t\t sig_inc = " << sig_inc << std::endl;
    std::cout << "\t\t\t strain_inc = " << strain_inc << std::endl;
    std::cout << "\t\t\t sig_inc_iso = " << sig_inc_iso << std::endl;
    std::cout << "\t\t\t sig_inc_dev = " << sig_inc_dev << std::endl;
    std::cout << "\t\t\t plasticStrain_inc_fixed = " << plasticStrain_inc_fixed
              << std::endl;
  }
#endif

#ifdef CHECK_HYDROSTATIC_TENSION
  if (I1_closest < 0) {
    std::cout << "\t\t\t sig_inc = " << sig_inc << std::endl;
    std::cout << "\t\t\t strain_inc = " << strain_inc << std::endl;
    std::cout << "\t\t\t sig_inc_iso = " << sig_inc_iso << std::endl;
    std::cout << "\t\t\t sig_inc_dev = " << sig_inc_dev << std::endl;
    std::cout << "\t\t\t plasticStrain_inc_fixed = " << plasticStrain_inc_fixed
              << std::endl;
  }
#endif

  return true; // isSuccess = true

} //===================================================================

/**
 * Method: consistencyBisectionSimplified
 * Purpose:
 *   Find the updated stress for hardening plasticity using the consistency
 * bisection
 *   algorithm
 *   Returns whether the procedure is sucessful or has failed
 */
bool
TabularPlasticity::consistencyBisectionSimplified(const Matrix3& deltaEps_new,
                                      const ModelState_Tabular& state_k_old,
                                      const ModelState_Tabular& state_k_trial,
                                      const Matrix3& deltaEps_e_fixed,
                                      const Matrix3& deltaEps_p_fixed,
                                      const Matrix3& sig_fixed,
                                      ModelState_Tabular& state_k_new)
{
  // bisection convergence tolerance on eta (if changed, change imax)
  const double TOLERANCE = d_cm.consistency_bisection_tolerance;
  // imax = ceil(-10.0*log(TOL)); // Update this if TOL changes
  const int IMAX = d_cm.max_bisection_iterations;

  // Get the old state
  Matrix3 sig_old = state_k_old.stressTensor;
  Matrix3 eps_e_old = state_k_old.elasticStrainTensor;
  Matrix3 eps_p_old = state_k_old.plasticStrainTensor;

  // Get the fixed non-hardening return state and compute invariants
  double deltaEps_p_v_fixed = deltaEps_p_fixed.Trace();
  double norm_deltaEps_p_fixed = deltaEps_p_fixed.Norm();

  // Create a state for the fixed non-hardening yield surface state
  // and update only the stress and plastic strain
  ModelState_Tabular state_k_fixed(state_k_old);
  state_k_fixed.stressTensor = sig_fixed;
  state_k_fixed.elasticStrainTensor = eps_e_old + deltaEps_e_fixed;
  state_k_fixed.plasticStrainTensor = eps_p_old + deltaEps_p_fixed;

  // Initialize the new consistently updated state
  Matrix3 sig_fixed_new = sig_fixed;
  Matrix3 deltaEps_e_fixed_new = deltaEps_e_fixed;
  Matrix3 deltaEps_p_fixed_new = deltaEps_p_fixed;
  double deltaEps_p_v_fixed_new = deltaEps_p_v_fixed;
  double norm_deltaEps_p_fixed_new = norm_deltaEps_p_fixed;

  // Set up a local trial state
  ModelState_Tabular state_trial_local(state_k_trial);

  // Start loop
  int ii = 1;
  double eta_lo = 0.0, eta_hi = 1.0, eta_mid = 0.5;
  bool isSuccess = false;

  while (std::abs(eta_hi - eta_lo) > TOLERANCE) {

#ifdef DEBUG_YIELD_BISECTION_R
    std::cout << "consistency_iter = " << ii << std::endl;
    std::cout << "eta_hi = " << eta_hi << std::endl;
    std::cout << "eta_lo = " << eta_lo << std::endl;
#endif

    // Reset the local trial state
    state_trial_local = state_k_trial;

    // Compute the volumetric plastic strain at eta = eta_mid
    eta_mid = 0.5 * (eta_lo + eta_hi);
    double deltaEps_p_v_mid = eta_mid * deltaEps_p_v_fixed;

    // Update the internal variables at eta = eta_mid in the local trial state
    isSuccess = computeInternalVariables(state_trial_local, deltaEps_p_v_mid);
    if (!isSuccess) {
      state_k_new = state_k_old;
      proc0cout << "computeInternalVariables has failed." << std::endl;
      return false;
    }

    // Test the yield condition to check whether the yield surface moves beyond
    // the
    // trial stress state when the internal variables are changed.
    // If the yield surface is too big, the plastic strain is reduced
    // by bisecting <eta> and the loop is repeated.
    int yield = (int)d_yield->evalYieldCondition(&state_trial_local);

    // If the local trial state is inside the updated yield surface the yield
    // condition evaluates to "elastic".  We need to reduce the size of the
    // yield surface by decreasing the plastic strain increment.
    if (yield != 1) { // Elastic or on yield surface
      eta_hi = eta_mid;
      ii++;
      continue;
    }

    // At this point, state_trial_local contains the trial stress, the plastic
    // strain at
    // the beginning of the timestep, and the updated values of the internal
    // variables
    // The yield surface depends only on X and p_w.  We will compute the updated
    // location
    // of the yield surface based on the updated internal variables (keeping the
    // elastic moduli at the values at the beginning of the step) and do
    // a non-hardening return to that yield surface.
    ModelState_Tabular state_k_updated(state_k_old);
    state_k_updated.pbar_w = state_trial_local.pbar_w;
    state_k_updated.porosity = state_trial_local.porosity;
    state_k_updated.saturation = state_trial_local.saturation;
    state_k_updated.capX = state_trial_local.capX;
    state_k_updated.updateStressInvariants();
    isSuccess = nonHardeningReturn(deltaEps_new, state_k_updated,
                                   state_trial_local, sig_fixed_new,
                                   deltaEps_e_fixed_new, deltaEps_p_fixed_new);
    if (!isSuccess) {
      proc0cout << "nonHardeningReturn inside consistencyBisection failed."
                << std::endl;
      return isSuccess;
    }

    // Check whether the isotropic component of the return has changed sign, as
    // this
    // would indicate that the cap apex has moved past the trial stress,
    // indicating
    // too much plastic strain in the return.
    Matrix3 sig_trial = state_trial_local.stressTensor;
    double diff_trial_fixed_new = (sig_trial - sig_fixed_new).Trace();
    double diff_trial_fixed = (sig_trial - sig_fixed).Trace();
    if (std::signbit(diff_trial_fixed_new) != std::signbit(diff_trial_fixed)) {
      eta_hi = eta_mid;
      ii++;
      continue;
    }

    // Compare magnitude of plastic strain with prior update
    norm_deltaEps_p_fixed_new = deltaEps_p_fixed_new.Norm();
    norm_deltaEps_p_fixed = eta_mid * deltaEps_p_fixed.Norm();
    deltaEps_p_v_fixed_new = deltaEps_p_fixed_new.Trace();
    deltaEps_p_v_fixed = eta_mid * deltaEps_p_fixed.Trace();

#ifdef CHECK_CONSISTENCY_BISECTION_CONVERGENCE
    std::cout << "eta_mid = " << eta_mid << " eta_mid*||deltaEps_p_fixed|| = "
              << eta_mid * norm_deltaEps_p_fixed
              << " ||deltaEps_p_fixed_new|| = " << norm_deltaEps_p_fixed_new
              << " ratio = "
              << eta_mid * norm_deltaEps_p_fixed / norm_deltaEps_p_fixed_new
              << std::endl;
    std::cout << " delta_eps_p_v_mid = " << deltaEps_p_v_mid
              << " delta_eps_p_v_fixed_new = " << deltaEps_p_v_fixed_new
              << " ratio = " << deltaEps_p_v_mid / deltaEps_p_fixed_new.Trace()
              << std::endl;
#endif

    // if (norm_deltaEps_p_fixed_new > eta_mid*norm_deltaEps_p_fixed) {
    if (std::abs(deltaEps_p_v_fixed_new) >
        eta_mid * std::abs(deltaEps_p_v_fixed)) {
      eta_lo = eta_mid;
    } else {
      eta_hi = eta_mid;
    }

    // Increment i and check
    ii++;
    if (ii > IMAX) {
      state_k_new = state_k_old;
      proc0cout << "Consistency bisection has failed because ii > IMAX." << ii
                << " > " << IMAX << std::endl;
      return false; // bool isSuccess = false;
    }

  } // end  while (std::abs(eta_hi - eta_lo) > TOLERANCE);

  // Set the new state to the old trial state
  // The volumetric strain may not have converged so recompute internal
  // variables
  state_k_new = state_k_trial;
  isSuccess = computeInternalVariables(state_k_new, deltaEps_p_v_fixed_new);
  if (!isSuccess) {
    state_k_new = state_k_old;
    proc0cout << "computeInternalVariables has failed." << std::endl;
    return false;
  }

  // Update the stress and plastic strain of the new state +  the elastic moduli
  state_k_new.stressTensor = sig_fixed_new;
  state_k_new.elasticStrainTensor = eps_e_old + deltaEps_e_fixed_new;
  state_k_new.plasticStrainTensor = eps_p_old + deltaEps_p_fixed_new;
  computeElasticProperties(state_k_new);

#ifdef DEBUG_YIELD_BISECTION_R
  std::cout << "pbar_w_before_consistency_3 = " << 3.0 * state_k_old.pbar_w
            << std::endl;
  std::cout << "pbar_w_after_consistency_3 = " << 3.0 * state_k_new.pbar_w
            << std::endl;
  std::cout << "K_before_consistency = " << state_k_old.bulkModulus
            << std::endl;
  std::cout << "K_after_consistency = " << state_k_new.bulkModulus << std::endl;
  std::cout << "I1_before_consistency = " << state_k_old.stressTensor.Trace()
            << std::endl;
  std::cout << "I1_after_consistency = " << state_k_new.stressTensor.Trace()
            << std::endl;
  std::cout << "I1_eff_before_consistency = " << state_k_old.I1_eff
            << std::endl;
  std::cout << "I1_eff_after_consistency = " << state_k_new.I1_eff << std::endl;
#endif

  // Update the cumulative equivalent plastic strain
  double deltaEps_p_v = deltaEps_p_fixed_new.Trace();
  Uintah::Matrix3 deltaEps_p_dev =
    deltaEps_p_fixed_new - Identity * (deltaEps_p_v / 3.0);
  state_k_new.ep_cum_eq =
    state_k_old.ep_cum_eq +
    std::sqrt(2.0 / 3.0 * deltaEps_p_dev.Contract(deltaEps_p_dev));

#ifdef DEBUG_INTERNAL_VAR_EVOLUTION
  std::cout << "consistencyBisection: " << std::endl
            << "\t state_old = " << state_k_old << std::endl
            << "\t state_new = " << state_k_new << std::endl;
#endif

  // Return success = true
  return true; // bool isSuccess = true;
}

/**
 * Method: computeInternalVariables
 * Purpose:
 *   Update an old state with new values of internal variables given the old
 * state and an
 *   increment in volumetric plastic strain
 */
bool
TabularPlasticity::computeInternalVariables(ModelState_Tabular& state,
                                const double& delta_eps_p_v)
{
  // Internal variables are not allowed to evolve when the effective stress is
  // tensile
  // if (state.I1_eff > 0.0) {
  //  return;
  //}

  // Convert strain increment to barred quantity (positive in compression)
  double delta_epsbar_p_v = -delta_eps_p_v;

  // Get the initial fluid pressure
  double pbar_w0 = d_fluidParam.pbar_w0;

  // Get the initial porosity and saturation
  double phi0 = d_fluidParam.phi0;
  double Sw0 = d_fluidParam.Sw0;

  // Get the old values of the internal variables
  double epsbar_p_v_old = -state.ep_v;
  double pbar_w_old = state.pbar_w;
  double phi_old = state.porosity;
  double Sw_old = state.saturation;
  // double Xbar_old = -state.capX;
  double p3_old = state.p3;

  // If epsbar_p_v_old + Delta epsbar_p_v > p3 don't do anything
  if ((epsbar_p_v_old + delta_epsbar_p_v) > p3_old) {
    proc0cout << "**WARNING** eps_p_v > p3_old : " << epsbar_p_v_old << "+"
              << delta_epsbar_p_v << ">" << p3_old << std::endl;
    return false;
  }

  // Compute the bulk moduli of air and water at the old value of pbar_w
  double K_a = d_air.computeBulkModulus(pbar_w_old);
  double K_w = d_water.computeBulkModulus(pbar_w_old);
  double one_over_K_a = 1.0 / K_a;
  double one_over_K_w = 1.0 / K_w;

  // Compute the volumetric strain in the air and water at the old value of
  // pbar_w
  double ev_a0 = d_air.computeElasticVolumetricStrain(pbar_w0, 0.0);
  double ev_a = d_air.computeElasticVolumetricStrain(pbar_w_old, 0.0);
  double ev_w = d_water.computeElasticVolumetricStrain(pbar_w_old, pbar_w0);
  double epsbar_v_a = std::max(-(ev_a - ev_a0), 0.0);
  double epsbar_v_w = std::max(-ev_w, 0.0);

#ifdef CHECK_FLOATING_POINT_OVERFLOW
  errno = 0;
  std::feclearexcept(FE_ALL_EXCEPT);
#endif
  double exp_ev_a_minus_ev_w = std::exp(epsbar_v_a - epsbar_v_w);
#ifdef CHECK_FLOATING_POINT_OVERFLOW
  if (errno == ERANGE) {
    std::cout << " in exp(): errno == ERANGE: " << std::strerror(errno)
              << std::endl;
  }
  if (std::fetestexcept(FE_OVERFLOW)) {
    std::cout << "    FE_OVERFLOW raised\n";
  }
#endif

  // Compute C_p and 1/(1+Cp)^2
  double C_p = Sw0 * exp_ev_a_minus_ev_w;
// double one_over_one_p_C_p_Sq = (1.0 - Sw0)/((1.0 - Sw0 + C_p)*(1.0 - Sw0 +
// C_p));

// Compute dC_p/dp_w
// double dC_p_dpbar_w = C_p*(one_over_K_a - one_over_K_w);

// Compute B_p
#ifdef CHECK_FLOATING_POINT_OVERFLOW
  errno = 0;
  std::feclearexcept(FE_ALL_EXCEPT);
#endif
  double exp_ev_p_minus_ev_a = std::exp(epsbar_p_v_old - epsbar_v_a);
  double exp_ev_p_minus_ev_w = std::exp(epsbar_p_v_old - epsbar_v_w);
#ifdef CHECK_FLOATING_POINT_OVERFLOW
  if (errno == ERANGE) {
    std::cout << " in exp(): errno == ERANGE: " << std::strerror(errno)
              << std::endl;
  }
  if (std::fetestexcept(FE_OVERFLOW)) {
    std::cout << "    FE_OVERFLOW raised\n";
  }
#endif

  double B_p = 1.0 /
               ((1.0 - Sw0) * exp_ev_p_minus_ev_a + Sw0 * exp_ev_p_minus_ev_w) *
               (-(1.0 - phi_old) * (phi_old / phi0) *
                  (Sw_old * one_over_K_w + (1.0 - Sw_old) * one_over_K_a) +
                (1.0 - Sw0) * one_over_K_a * exp_ev_p_minus_ev_a +
                Sw0 * one_over_K_w * exp_ev_p_minus_ev_w);
  // double one_over_B_p = (std::abs(B_p) < 1.0e-30) ? 1.0e30 : 1.0/B_p;
  double one_over_B_p = 1.0 / B_p;

  // Update the pore pressure
  double pbar_w_new = pbar_w_old + one_over_B_p * delta_epsbar_p_v;

#ifdef DEBUG_INTERNAL_VAR_EVOLUTION_COMPUTATION
  std::cout << "computeInternalVar: epsbar_p_v = " << -state.ep_v
            << " delta epsbar_p_v = " << delta_epsbar_p_v
            << " pbar_w = " << state.pbar_w << " pbar_w_old = " << pbar_w_old
            << " pbar_w_new = " << pbar_w_new << " Sw0 = " << Sw0
            << " Sw = " << Sw_old << " phi0 = " << phi0 << " phi = " << phi_old
            << " epsbar_v_a = " << epsbar_v_a << " epsbar_v_w = " << epsbar_v_w
            << " Ka = " << K_a << " Kw = " << K_w << " B_p = " << B_p
            << std::endl;
#endif

  // Don't allow negative pressures during dilatative plastic deformations
  pbar_w_new = std::max(pbar_w_new, 0.0);
  // assert(!(pbar_w_new < 0.0));

  // Update the hydrostatic compressive strength
  // (using integration)
  /*
  // Compute the drained hydrostatic compressive strength
  double Xbar_d = 0.0;
  double derivXbar_d = 0.0;
  computeDrainedHydrostaticStrengthAndDeriv(epsbar_p_v_old, p3_old, Xbar_d,
  derivXbar_d);

  // Update the hydrostatic strength
  double p1_sat = d_crushParam.p1_sat;
  double Xbar_new = Xbar_old + ((1.0 - Sw_old + p1_sat*Sw_old)*derivXbar_d +
    Xbar_d*(p1_sat - 1.0)*one_over_B_p*one_over_one_p_C_p_Sq*dC_p_dpbar_w +
  3.0*one_over_B_p)*
    delta_epsbar_p_v;
  double Xbar_clamp = d_crushParam.p0 + 3.0*pbar_w_new;
  Xbar_new = (Xbar_new < Xbar_clamp) ? Xbar_clamp : Xbar_new;
  //assert(!(Xbar_new < 0.0));
  */

  // Get the new value of the volumetric plastic strain
  double epsbar_p_v_new = epsbar_p_v_old + delta_epsbar_p_v;

  // Compute the volumetric strain in the air and water at the new value of
  // pbar_w
  ev_a = d_air.computeElasticVolumetricStrain(pbar_w_new, 0.0);
  ev_w = d_water.computeElasticVolumetricStrain(pbar_w_new, pbar_w0);
  epsbar_v_a = std::max(-(ev_a - ev_a0), 0.0);
  epsbar_v_w = std::max(-ev_w, 0.0);
  exp_ev_a_minus_ev_w = std::exp(epsbar_v_a - epsbar_v_w);
  exp_ev_p_minus_ev_a = std::exp(epsbar_p_v_new - epsbar_v_a);
  exp_ev_p_minus_ev_w = std::exp(epsbar_p_v_new - epsbar_v_w);

  // Update the saturation using closed form expression
  C_p = Sw0 * exp_ev_a_minus_ev_w;
  double Sw_new = C_p / (1.0 - Sw0 + C_p);
  // assert(!(Sw_new < 0.0));

  // Update the porosity using closed form expression
  double phi_new =
    (1.0 - Sw0) * phi0 * exp_ev_p_minus_ev_a + Sw0 * phi0 * exp_ev_p_minus_ev_w;
  // assert(!(phi_new < 0.0));

  // Update the hydrostatic compressive strength
  // (using closed form solution)
  // Compute the drained hydrostatic compressive strength
  double Xbar_d = 0.0;
  double derivXbar_d = 0.0;
  computeDrainedHydrostaticStrengthAndDeriv(epsbar_p_v_new, p3_old, Xbar_d,
                                            derivXbar_d);

  // Update the hydrostatic strength
  double p0 = d_crushParam.p0;
  double p1_sat = d_crushParam.p1_sat;
  double Xbar_new =
    p0 + (1.0 - Sw0 + p1_sat * Sw0) * (Xbar_d - p0) + 3.0 * pbar_w_new;
/*
if (state.particleID == 3377699720593411) {
  std::cout << "pbar_w_old = " << pbar_w_old
            << "pbar_w_new = " << pbar_w_new
            << "Xbar_d = " << Xbar_d
            << "Xbar_new = " << Xbar_new << std::endl;
}
*/

#ifdef CHECK_FOR_NAN
  if (std::isnan(Xbar_new)) {
    std::cout << "State = " << state << std::endl;
    std::cout << "epsbar_p_v_new = " << epsbar_p_v_new << std::endl;
    std::cout << "epsbar_p_v_old " << epsbar_p_v_old << std::endl;
    std::cout << "delta_epsbar_p_v = " << delta_epsbar_p_v << std::endl;
    std::cout << "pbar_w_old = " << pbar_w_old << "pbar_w_new = " << pbar_w_new
              << "Xbar_d = " << Xbar_d << "Xbar_new = " << Xbar_new
              << std::endl;
  }
#endif

  if (phi_new > 1.0) {
    proc0cout << "**WARNING** Porosity > 1.0 in particle " << state.particleID
              << std::endl;
    proc0cout << "\t ev_a = " << epsbar_v_a << " ev_w = " << epsbar_v_w
              << " ev_p_old = " << epsbar_p_v_old
              << " ev_p = " << epsbar_p_v_new
              << " exp(ev_p - ev_a) = " << exp_ev_p_minus_ev_a
              << " exp(ev_p - ev_w) = " << exp_ev_p_minus_ev_w
              << " phi_new = " << phi_new << std::endl;
    // proc0cout << "** WARNING ** Bad step, deleting particle"
    //          << ":" << __FILE__ << ":" << __LINE__ << std::endl;

    return false; // May have to delete the particle
  }

  // Update the state with new values of the internal variables
  state.pbar_w = pbar_w_new;
  state.porosity = phi_new;
  state.saturation = Sw_new;
  state.capX = -Xbar_new;

  return true;
}

/**
 * Method: computeDrainedHydrostaticStrengthAndDeriv
 * Purpose:
 *   Compute the drained hydrostatic compressive strength and its derivative
 */
void
TabularPlasticity::computeDrainedHydrostaticStrengthAndDeriv(const double& epsbar_p_v,
                                                 const double& p3,
                                                 double& Xbar_d,
                                                 double& derivXbar_d) const
{
  // Get the initial porosity
  double phi0 = d_fluidParam.phi0;

  // Get the crush curve parameters
  double p0 = d_crushParam.p0;
  double p1 = d_crushParam.p1;
  double p2 = d_crushParam.p2;
  // double p3 = -std::log(1.0 - phi0); // For disaggregation: Use p3 from
  // particle instead

  Xbar_d = p0;
  derivXbar_d = 0.0;
  // std::cout << "\t\t eps_bar_p_v = " << eps_bar_p_v << std::endl;
  if (epsbar_p_v > 0.0) {
    double local_epsbar_p_v = std::min(epsbar_p_v, 0.99999999 * p3);
    double phi_temp = std::exp(-p3 + local_epsbar_p_v);
    double phi = 1.0 - phi_temp;
    double phi0_phi = phi0 / phi;
    double phi0_phi_minus_one = std::max((phi0_phi - 1.0), 0.0);
    double xi_bar = p1 * std::pow(phi0_phi_minus_one, 1.0 / p2);
    Xbar_d += xi_bar;
    derivXbar_d =
      1.0 / p2 * phi0_phi * phi_temp * xi_bar / (phi * phi0_phi_minus_one);
#ifdef CHECK_FOR_NAN
    if (std::isnan(Xbar_d)) {
      proc0cout << "**ERROR** NaN in hydrostatic compressive strength."
                << std::endl;
      proc0cout << "\t Local values : epsbar_p_v = " << local_epsbar_p_v
                << std::endl;
      proc0cout << "\t\t phi_temp = " << phi_temp << " phi = " << phi
                << " xi_bar = " << xi_bar << " Xbar_d = " << Xbar_d
                << " dXbar_d = " << derivXbar_d << std::endl;
    }
#endif
  }

  return;
}

/**
 * Function: updateDamageParameters
 *
 * Purpose: Update the damage parameters local to this model
 */
void
TabularPlasticity::updateDamageParameters(const Matrix3& D, const double& delta_t,
                              const ModelState_Tabular& state_k_old,
                              ModelState_Tabular& state_k_new) const
{
#ifndef TEST_FRACTURE_STRAIN_CRITERION
  // Compute total strain increment
  Matrix3 deltaEps = D * delta_t;
  double deltaEpsNorm = deltaEps.Norm();

  // Compute plastic strain increment
  Matrix3 deltaEps_p =
    state_k_new.plasticStrainTensor - state_k_old.plasticStrainTensor;
  double deltaEps_p_Norm = deltaEps_p.Norm();

  // Compute fraction of time increment spent on yield surface
  double t_grow_inc =
    std::min(delta_t, (deltaEps_p_Norm / deltaEpsNorm) * delta_t);

  // Update t_grow
  double t_grow = state_k_old.t_grow + t_grow_inc;

  // Compute coherence
  double fspeed = d_damageParam.fSpeed;
  double xvar = std::exp(-fspeed * (t_grow / d_damageParam.tFail - 1.0));
  double coher = xvar / (1.0 + xvar);

#ifdef CHECK_DAMAGE_ALGORITHM
  std::cout << "||Delta Eps|| = " << deltaEpsNorm
            << " ||Delta Eps_p|| = " << deltaEps_p_Norm
            << " Delta t_grow = " << t_grow_inc << " t_grow = " << t_grow
            << " coher = " << coher << std::endl;
#endif

  // Update the state
  state_k_new.t_grow = t_grow;
  state_k_new.coherence = std::min(coher, state_k_old.coherence);

#ifdef CHECK_DAMAGE_ALGORITHM
  std::cout << "t_grow = " << t_grow << " t_fail = " << d_damageParam.tFail
            << "\t coherence = " << state_k_new.coherence << std::endl;
#endif

#else

  // Compute time rate of plastic strain
  double eps_p_f_eq = d_damageParam.ep_f_eq;
  double dot_eps_p_eq = D.Norm();
  // double dot_eps_p_eq = (state_k_new.ep_cum_eq -
  // state_k_old.ep_cum_eq)/delta_t;

  // If the plastic strain hasn't changed, return the old values
  if (dot_eps_p_eq < delta_t * 1.0e-6) {
    state_k_new.t_grow = state_k_old.t_grow;
    state_k_new.coherence = state_k_old.coherence;
    return;
  }

  // Update t_grow
  double t_grow = state_k_new.t_grow + delta_t;

  // Compute t_fail
  // double eps_p_eq = state_k_new.ep_cum_eq;
  // double t_fail = t_grow + (eps_p_f_eq - eps_p_eq)/dot_eps_p_eq;
  double t_fail = eps_p_f_eq / dot_eps_p_eq;

  // Compute coherence
  double fspeed = d_damageParam.fSpeed;
  double xvar = std::exp(-fspeed * (t_grow / t_fail - 1.0));
  double coher = xvar / (1.0 + xvar);

  // Update the state
  state_k_new.t_grow = t_grow;
  state_k_new.coherence = std::min(coher, state_k_old.coherence);

#ifdef CHECK_DAMAGE_ALGORITHM
  std::cout << "t_grow = " << t_grow << " t_fail = " << t_fail
            << " eps_p_f_eq = " << eps_p_f_eq
            << " dot_eps_p_eq = " << dot_eps_p_eq << " coher = " << coher
            << std::endl
            << "\t coherence = " << state_k_new.coherence << std::endl;
  std::cout << "\t ep_cum_eq(old) = " << state_k_old.ep_cum_eq
            << " ep_cum_eq(new) = " << state_k_new.ep_cum_eq
            << " ep_eq(old) = " << state_k_old.ep_eq
            << " ep_eq(new) = " << state_k_new.ep_eq << std::endl;
#endif
#endif

  return;
}

//===================================================================
/**
 * Function: rateDependentPlasticUpdate
 *
 * Purpose:
 *   Rate-dependent plastic step
 *   Compute the new dynamic stress from the old dynamic stress and the new and
 * old QS stress
 *   using Duvaut-Lions rate dependence, as described in "Elements of
 * Phenomenological Plasticity",
 *   by RM Brannon.
 */
bool
TabularPlasticity::rateDependentPlasticUpdate(const Matrix3& D, const double& delT,
                                  const ModelState_Tabular& stateStatic_old,
                                  const ModelState_Tabular& stateStatic_new,
                                  const ModelState_Tabular& stateDynamic_old,
                                  Matrix3& pStress_new)
{
  // Get the T1 & T2 parameters
  double T1 = 0.0, T2 = 0.0;
  try {
    T1 = stateStatic_old.yieldParams.at("T1");
    T2 = stateStatic_old.yieldParams.at("T2");
  } catch (std::out_of_range) {
    std::ostringstream err;
    err << "**ERROR** Could not find yield parameters T1 and T2" << std::endl;
    for (auto param : stateStatic_old.yieldParams) {
      err << param.first << " " << param.second << std::endl;
    }
    throw InternalError(err.str(), __FILE__, __LINE__);
  }

  // Check if rate-dependent plasticity has been turned on
  if (T1 == 0.0 || T2 == 0.0) {

    // No rate dependence, the dynamic stress equals the static stress.
    pStress_new = stateStatic_new.stressTensor;
    // bool isRateDependent = false;
    return false;
  }

  // This is not straightforward, due to nonlinear elasticity.  The equation
  // requires that we
  // compute the trial stress for the step, but this is not known, since the
  // bulk modulus is
  // evolving through the substeps.  It would be necessary to to loop through
  // the substeps to
  // compute the trial stress assuming nonlinear elasticity, but instead we will
  // approximate
  // the trial stress the average of the elastic moduli at the start and end of
  // the step.

  // Compute midstep bulk and shear modulus
  ModelState_Tabular stateDynamic(stateDynamic_old);
  stateDynamic.bulkModulus =
    0.5 * (stateStatic_old.bulkModulus + stateStatic_new.bulkModulus);
  stateDynamic.shearModulus =
    0.5 * (stateStatic_old.shearModulus + stateStatic_new.shearModulus);

  Matrix3 strain_inc = D * delT;
  if (strain_inc.Norm() < 1.0e-30) {
    pStress_new = stateStatic_new.stressTensor;
    return true;
  }

  Matrix3 sigma_trial = computeTrialStress(stateDynamic, strain_inc);

  // The characteristic time is defined from the rate dependence input
  // parameters and the
  // magnitude of the strain rate.
  // tau = T1*(epsdot)^(-T2) = T1*(1/epsdot)^T2, modified to avoid division by
  // zero.
  double tau = T1 * std::pow(1.0 / std::max(D.Norm(), 1.0e-15), T2);

  // RH and rh are defined by eq. 6.93 in the RMB book chapter, but there seems
  // to be a sign error
  // in the text, and I've rewritten it to avoid computing the exponential
  // twice.
  double dtbytau = delT / tau;
  double rh = std::exp(-dtbytau);
  double RH = (1.0 - rh) / dtbytau;

  // sigma_new = sigmaQS_new + sigma_over_new, as defined by eq. 6.92
  // sigma_over_new = [(sigma_trial_new - sigma_old) -
  // (sigmaQS_new-sigmaQS_old)]*RH + sigma_over_old*rh
  Matrix3 sigmaQS_old = stateStatic_old.stressTensor;
  Matrix3 sigmaQS_new = stateStatic_new.stressTensor;
  Matrix3 sigma_old = stateDynamic_old.stressTensor;
  pStress_new = sigmaQS_new +
                ((sigma_trial - sigma_old) - (sigmaQS_new - sigmaQS_old)) * RH +
                (sigma_old - sigmaQS_old) * rh;

  // bool isRateDependent = true;
  return true;
}

// ****************************************************************************************************
// ****************************************************************************************************
// ************** PUBLIC Uintah MPM constitutive model specific functions
// *****************************
// ****************************************************************************************************
// ****************************************************************************************************

void
TabularPlasticity::addRequiresDamageParameter(Task* task, const MPMMaterial* matl,
                                  const PatchSet*) const
{
  // Require the damage parameter
  const MaterialSubset* matlset = matl->thisMaterial();
#ifdef USE_LOCAL_LOCALIZED_PVAR
  task->requires(Task::NewDW, pLocalizedLabel_preReloc, matlset, Ghost::None);
#else
  task->requires(Task::NewDW, lb->pLocalizedMPMLabel_preReloc, matlset,
                 Ghost::None);
#endif
}

void
TabularPlasticity::getDamageParameter(const Patch* patch, ParticleVariable<int>& damage,
                          int matID, DataWarehouse* old_dw,
                          DataWarehouse* new_dw)
{
  // Get the damage parameter
  ParticleSubset* pset = old_dw->getParticleSubset(matID, patch);
  constParticleVariable<int> pLocalized;
#ifdef USE_LOCAL_LOCALIZED_PVAR
  new_dw->get(pLocalized, pLocalizedLabel_preReloc, pset);
#else
  new_dw->get(pLocalized, lb->pLocalizedMPMLabel_preReloc, pset);
#endif

  // Loop over the particle in the current patch.
  for (int& iter : *pset) {
    damage[iter] = pLocalized[iter];
  }
}

void
TabularPlasticity::carryForward(const PatchSubset* patches, const MPMMaterial* matl,
                    DataWarehouse* old_dw, DataWarehouse* new_dw)
{
  // Carry forward the data.
  for (int p = 0; p < patches->size(); p++) {
    const Patch* patch = patches->get(p);
    int matID = matl->getDWIndex();
    ParticleSubset* pset = old_dw->getParticleSubset(matID, patch);

    // Carry forward the data common to all constitutive models
    // when using RigidMPM.
    // This method is defined in the ConstitutiveModel base class.
    carryForwardSharedData(pset, old_dw, new_dw, matl);

    // Carry forward the data local to this constitutive model
    new_dw->put(delt_vartype(1.e10), lb->delTLabel, patch->getLevel());

    if (flag->d_reductionVars->accStrainEnergy ||
        flag->d_reductionVars->strainEnergy) {
      new_dw->put(sum_vartype(0.0), lb->StrainEnergyLabel);
    }
  }
}

// T2D: Throw exception that this is not supported
void
TabularPlasticity::addComputesAndRequires(Task*, const MPMMaterial*, const PatchSet*,
                              const bool, const bool) const
{
  std::cout
    << "NO Implicit VERSION OF addComputesAndRequires EXISTS YET FOR TabularPlasticity"
    << endl;
}

/*!
 * ---------------------------------------------------------------------------------------
 *  This is needed for converting from one material type to another.  The
 * functionality
 *  has been removed from the main Uintah branch.
 *  ---------------------------------------------------------------------------------------
 */
void
TabularPlasticity::allocateCMDataAdd(DataWarehouse* new_dw, ParticleSubset* addset,
                         ParticleLabelVariableMap* newState,
                         ParticleSubset* delset, DataWarehouse* old_dw)
{
  std::ostringstream out;
  out << "Material conversion after failure not implemented for Arenisca.";
  throw ProblemSetupException(out.str(), __FILE__, __LINE__);
  // task->requires(Task::NewDW, pPorosityLabel_preReloc,         matlset,
  // Ghost::None);
  // task->requires(Task::NewDW, pSaturationLabel_preReloc,       matlset,
  // Ghost::None);
}

/*---------------------------------------------------------------------------------------
 * MPMICE Hooks
 *---------------------------------------------------------------------------------------*/
double
TabularPlasticity::computeRhoMicroCM(double pressure, const double p_ref,
                         const MPMMaterial* matl, double temperature,
                         double rho_guess)
{
  double rho_0 = matl->getInitialDensity();
  double K0 = d_cm.K0_Murnaghan_EOS;
  double n = d_cm.n_Murnaghan_EOS;

  double p_gauge = pressure - p_ref;
  double rho_cur = rho_0 * std::pow(((n * p_gauge) / K0 + 1), (1.0 / n));

  return rho_cur;
}

void
TabularPlasticity::computePressEOSCM(double rho_cur, double& pressure, double p_ref,
                         double& dp_drho, double& soundSpeedSq,
                         const MPMMaterial* matl, double temperature)
{
  double rho_0 = matl->getInitialDensity();
  double K0 = d_cm.K0_Murnaghan_EOS;
  double n = d_cm.n_Murnaghan_EOS;

  double eta = rho_cur / rho_0;
  double p_gauge = K0 / n * (std::pow(eta, n) - 1.0);

  double bulk = K0 + n * p_gauge;
  // double nu = 0.0;
  double shear = 1.5 * bulk;

  pressure = p_ref + p_gauge;
  dp_drho = K0 * std::pow(eta, n - 1);
  soundSpeedSq = (bulk + 4.0 * shear / 3.0) / rho_cur; // speed of sound squared
}

double
TabularPlasticity::getCompressibility()
{
  std::cout << "NO VERSION OF getCompressibility EXISTS YET FOR TabularPlasticity" << endl;
  return 1.0 / d_cm.K0_Murnaghan_EOS;
}
