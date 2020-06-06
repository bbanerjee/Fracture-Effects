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

#ifndef __MODEL_STATE_DEFAULT_H__
#define __MODEL_STATE_DEFAULT_H__

#include <CCA/Components/MPM/ConstitutiveModel/Models/ModelStateBase.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ModelStateVisitor.h>
#include <Core/Math/Matrix3.h>

namespace Vaango {

/////////////////////////////////////////////////////////////////////////////
/*!
  \class ModelState_Default
  \brief A structure that stores the plasticity state data
  \author Biswajit Banerjee \n
*/
/////////////////////////////////////////////////////////////////////////////
using Uintah::Matrix3;

class ModelState_Default : public ModelStateBase
{
public:
  double yieldStress;
  double strainRate;
  double plasticStrainRate;
  double plasticStrain;
  double pressure;
  double temperature;
  double initialTemperature;
  double density;
  double initialDensity;
  double volume;
  double initialVolume;
  double bulkModulus;
  double initialBulkModulus;
  double shearModulus;
  double initialShearModulus;
  double meltingTemp;
  double initialMeltTemp;
  double specificHeat;
  double porosity;
  double energy;
  const Matrix3* backStress;

  ModelState_Default();

  ModelState_Default(const ModelState_Default& state);
  ModelState_Default(const ModelState_Default* state);

  ~ModelState_Default() override;

  ModelState_Default& operator=(const ModelState_Default& state);
  ModelState_Default* operator=(const ModelState_Default* state);

  void accept(ModelStateVisitor& visitor) override
  {
    visitor.visit(*this);
  }
};

} // End namespace Uintah

#endif // __MODEL_STATE_DEFAULT_H__
