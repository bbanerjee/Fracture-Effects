/*
 * The MIT License
 *
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

#include <CCA/Components/MPM/ConstitutiveModel/ElasticModuliModels/ElasticModuliModel.h>
#include <CCA/Components/MPM/ConstitutiveModel/InternalVarModels/IntVar_Metal.h>
#include <CCA/Components/MPM/ConstitutiveModel/ModelState/ModelStateBase.h>
#include <Core/Exceptions/InternalError.h>
#include <Core/Exceptions/InvalidValue.h>
#include <Core/Grid/Variables/MPMIntVarTypes.h>
#include <Core/Labels/MPMLabel.h>
#include <cmath>
#include <iomanip>
#include <iostream>

#include <errno.h>
#include <fenv.h>

using namespace Vaango;
using namespace Uintah;

using MetalIntVar = Uintah::MetalIntVar;

IntVar_Metal::IntVar_Metal(ProblemSpecP& ps)
{
  d_elastic = nullptr;
  d_shear = nullptr;
  d_eos = nullptr;
  initializeLocalMPMLabels();
}

IntVar_Metal::IntVar_Metal(const IntVar_Metal* cm)
{
  d_elastic = cm->d_elastic;
  d_shear = cm->d_shear;
  d_eos = cm->d_eos;
  initializeLocalMPMLabels();
}

void
IntVar_Metal::initializeLocalMPMLabels()
{
  auto type_d =
    Uintah::ParticleVariable<Uintah::MetalIntVar>::getTypeDescription();
  pIntVarLabel = Uintah::VarLabel::create("p.intVarMetal", type_d);
  pIntVarLabel_preReloc =
    Uintah::VarLabel::create("p.intVarMetal+", type_d);
}

IntVar_Metal::~IntVar_Metal()
{
  VarLabel::destroy(pIntVarLabel);
  VarLabel::destroy(pIntVarLabel_preReloc);
}

void
IntVar_Metal::outputProblemSpec(ProblemSpecP& ps)
{
  ProblemSpecP int_var_ps = ps->appendChild("internal_variable_model");
  int_var_ps->setAttribute("type", "metal_plasticity");
}

/* get internal variable labels */
std::vector<const Uintah::VarLabel*>
IntVar_Metal::getLabels() const
{
  std::vector<const Uintah::VarLabel*> labels;
  labels.push_back(pIntVarLabel);
  labels.push_back(pIntVarLabel_preReloc);
  return labels;
}

void
IntVar_Metal::addParticleState(std::vector<const VarLabel*>& from,
                               std::vector<const VarLabel*>& to)
{
  from.push_back(pIntVarLabel);
  to.push_back(pIntVarLabel_preReloc);
}

void
IntVar_Metal::addInitialComputesAndRequires(Task* task,
                                            const MPMMaterial* matl,
                                            const PatchSet*)
{
  const MaterialSubset* matlset = matl->thisMaterial();
  task->computes(pIntVarLabel, matlset);
}

void
IntVar_Metal::initializeInternalVariable(Uintah::ParticleSubset* pset,
                                         Uintah::DataWarehouse* new_dw)
{
  Uintah::ParticleVariable<Uintah::MetalIntVar> pIntVar;
  new_dw->allocateAndPut(pIntVar, pIntVarLabel, pset);

  for (auto pidx : *pset) {
    pIntVar[pidx] = { 0.0, 0.0 };
  }
}

void
IntVar_Metal::initializeInternalVariable(const Patch* /*patch*/,
                                         const MPMMaterial* /*matl*/,
                                         ParticleSubset* pset,
                                         DataWarehouse* new_dw,
                                         MPMLabel* /*lb*/,
                                         ParameterDict& /*params*/)
{
  initializeInternalVariable(pset, new_dw);
}

void
IntVar_Metal::addComputesAndRequires(Task* task,
                                     const MPMMaterial* matl,
                                     const PatchSet*)
{
  const MaterialSubset* matlset = matl->thisMaterial();
  task->requires(Task::OldDW, pIntVarLabel, matlset, Ghost::None);
  task->computes(pIntVarLabel_preReloc, matlset);
}

std::vector<Uintah::constParticleVariable<double>>
IntVar_Metal::getInternalVariables(Uintah::ParticleSubset* pset,
                                   Uintah::DataWarehouse* old_dw,
                                   const double& dummy)
{
  std::vector<Uintah::constParticleVariable<double>> pIntVars;
  return pIntVars;
}

std::vector<Uintah::constParticleVariable<Uintah::Matrix3>>
IntVar_Metal::getInternalVariables(Uintah::ParticleSubset* pset,
                                   Uintah::DataWarehouse* old_dw,
                                   const Uintah::Matrix3& dummy)
{
  std::vector<Uintah::constParticleVariable<Uintah::Matrix3>> pIntVars;
  return pIntVars;
}

// Allocate and put the local particle internal variables
void
IntVar_Metal::allocateAndPutInternalVariable(Uintah::ParticleSubset* pset,
                                             Uintah::DataWarehouse* new_dw,
                                             Uintah::ParticleVariableBase& var)
{
  new_dw->allocateAndPut(var, pIntVarLabel, pset);
}

void
IntVar_Metal::allocateAndPutInternalVariable(Uintah::ParticleSubset* pset,
                                             Uintah::DataWarehouse* new_dw,
                                             ParticleDoublePVec& pVars)
{
}

// Allocate and put the local <Matrix3> particle variables
void
IntVar_Metal::allocateAndPutInternalVariable(Uintah::ParticleSubset* pset,
                                             Uintah::DataWarehouse* new_dw,
                                             ParticleMatrix3PVec& pVars)
{
}

template <>
void
IntVar_Metal::evolveInternalVariable(
  Uintah::particleIndex idx,
  const ModelStateBase* state,
  Uintah::constParticleVariable<MetalIntVar>& var_old,
  Uintah::ParticleVariable<MetalIntVar>& var_new)
{
  var_new[idx].eqPlasticStrain =
    computeEqPlasticStrain(var_old[idx].eqPlasticStrain, state);
  var_new[idx].plasticPorosity =
    computePlasticPorosity(var_old[idx].plasticPorosity, state);
}

double
IntVar_Metal::computeInternalVariable(const std::string& label,
                                      const ModelStateBase* state) const
{
  if (label == "porosity") {
    return computePlasticPorosity(state->porosity, state);
  } else if (label == "eqPlasticStrain") {
    return computeEqPlasticStrain(state->eqPlasticStrain, state);
  }
  return 0.0;
}

double
IntVar_Metal::computeEqPlasticStrain(double eqPlasticStrain_old,
                                     const ModelStateBase* state) const
{
  return 0.0;
}

double
IntVar_Metal::computePlasticPorosity(double plasticPorosity_old,
                                     const ModelStateBase* state) const
{
  return 0.0;
}

double
IntVar_Metal::computeVolStrainDerivOfInternalVariable(
  const std::string& label,
  const ModelStateBase* state) const
{
  return 0.0;
}

/*!-----------------------------------------------------*/
void
IntVar_Metal::allocateCMDataAddRequires(Task* task,
                                        const MPMMaterial* matl,
                                        const PatchSet*,
                                        MPMLabel*)
{
  const MaterialSubset* matlset = matl->thisMaterial();
  task->requires(Task::NewDW, pIntVarLabel_preReloc, matlset, Ghost::None);
}

void
IntVar_Metal::allocateCMDataAdd(DataWarehouse* old_dw,
                                ParticleSubset* addset,
                                ParticleLabelVariableMap* newState,
                                ParticleSubset* delset,
                                DataWarehouse* new_dw)
{
  ParticleVariable<MetalIntVar> pIntVar;
  constParticleVariable<MetalIntVar> o_pIntVar;

  new_dw->allocateTemporary(pIntVar, addset);

  new_dw->get(o_pIntVar, pIntVarLabel_preReloc, delset);

  auto o = addset->begin();
  auto n = addset->begin();
  for (o = delset->begin(); o != delset->end(); o++, n++) {
    pIntVar[*n] = o_pIntVar[*o];
  }

  (*newState)[pIntVarLabel] = pIntVar.clone();
}

/*!-----------------------------------------------------*/
void
IntVar_Metal::allocateAndPutRigid(ParticleSubset* pset,
                                  DataWarehouse* new_dw,
                                  constParticleLabelVariableMap& var)
{
  ParticleVariable<MetalIntVar> pIntVar_new;
  new_dw->allocateAndPut(pIntVar_new, pIntVarLabel_preReloc, pset);
  for (auto pidx : *pset) {
    pIntVar_new[pidx] = dynamic_cast<constParticleVariable<MetalIntVar>&>(
      *var[pIntVarLabel])[pidx];
  }
}
