/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
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

#include <CCA/Components/MPM/ConstitutiveModel/ModelState/ModelStateBase.h>
#include <CCA/Components/MPM/ConstitutiveModel/Utilities/Constants.h>

using namespace Vaango;

ModelStateBase::ModelStateBase()
{
  yieldStress = 0.0;
  strainRate = 0.0;
  plasticStrainRate = 0.0;
  plasticStrain = 0.0;
  pressure = 0.0;
  temperature = 0.0;
  initialTemperature = 0.0;
  density = 0.0;
  initialDensity = 0.0;
  volume = 0.0;
  initialVolume = 0.0;
  bulkModulus = 0.0;
  initialBulkModulus = 0.0;
  shearModulus = 0.0;
  initialShearModulus = 0.0;
  meltingTemp = 0.0;
  initialMeltTemp = 0.0;
  specificHeat = 0.0;
  porosity = 0.0;
  energy = 0.0;
  backStress = nullptr;
  I1 = 0.0;
  J2 = 0.0;
}

ModelStateBase::ModelStateBase(const ModelStateBase& state)
{
  *this = &state;
}

ModelStateBase::ModelStateBase(const ModelStateBase* state)
{
  *this = state;
}

ModelStateBase::~ModelStateBase() = default;

ModelStateBase&
ModelStateBase::operator=(const ModelStateBase& state_in)
{
  const ModelStateBase* state = &state_in;
  if (this == state)
    return *this;

  *this = state;
  return *this;
}

ModelStateBase*
ModelStateBase::operator=(const ModelStateBase* state)
{
  if (this == state)
    return this;
  yieldStress = state->yieldStress;
  strainRate = state->strainRate;
  plasticStrainRate = state->plasticStrainRate;
  plasticStrain = state->plasticStrain;
  pressure = state->pressure;
  temperature = state->temperature;
  initialTemperature = state->initialTemperature;
  density = state->density;
  initialDensity = state->initialDensity;
  volume = state->volume;
  initialVolume = state->initialVolume;
  bulkModulus = state->bulkModulus;
  initialBulkModulus = state->initialBulkModulus;
  shearModulus = state->shearModulus;
  initialShearModulus = state->initialShearModulus;
  meltingTemp = state->meltingTemp;
  initialMeltTemp = state->initialMeltTemp;
  specificHeat = state->specificHeat;
  porosity = state->porosity;
  energy = state->energy;
  backStress = state->backStress;
  I1 = state->I1;
  J2 = state->J2;
  return this;
}

void 
ModelStateBase::updateStressInvariants(const Uintah::Matrix3& stress)
{
  Uintah::Matrix3 sdev = stress - Vaango::Util::Identity * (I1 / 3.0); 
  I1 = stress.Trace(); 
  J2 = 0.5 * sdev.Contract(sdev);
}
