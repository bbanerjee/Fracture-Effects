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

#ifndef __TEMPLATED_BORJA_SHEAR_MODEL_H__
#define __TEMPLATED_BORJA_SHEAR_MODEL_H__

#include <CCA/Components/MPM/ConstitutiveModel/ModelState/ModelState_Borja.h>
#include <CCA/Components/MPM/ConstitutiveModel/ShearModulusModels/ShearModulusT.h>
#include <Core/ProblemSpec/ProblemSpecP.h>

namespace Vaango {

/*!
  \class ShearModulus_BorjaT

  \brief The Borja model for calculating shear stress

  Reference:Borja, R.I. and Tamagnini, C.(1998) Cam-Clay plasticity Part III:
  Extension of the infinitesimal model to include finite strains,
  Computer Methods in Applied Mechanics and Engineering, 155 (1-2),
  pp. 73-95.

  The sheear stress magnitude is given by

  q = 3 mu epse_s

  where

  mu = mu0 + alpha p0 exp[(epse_v - epse_v0)/kappatilde]
  mu0 = constant
  alpha = constant
  p0 = constant
  kappatilde = constant
  epse_s = sqrt(2/3) ||epse||
  epse_v = tr(epse)
  epse_v0 = constant
  epse = elastic strain tensor
 *
*/
class ShearModulus_BorjaT : public ShearModulusT<ShearModulus_BorjaT>
{

private:
  double d_mu0;        // Reference shear modulus
  double d_alpha;      // Coupling constant (shear-pressure)
  double d_p0;         // Reference pressure
  double d_kappatilde; // Reference compressibility
  double d_epse_v0;    // Reference volume strain


public:
  /*! Construct a constant shear modulus model. */
  ShearModulus_BorjaT(Uintah::ProblemSpecP& ps, Uintah::MPMEquationOfState* eos);

  /*! Construct a copy of constant shear modulus model. */
  ShearModulus_BorjaT(const ShearModulus_BorjaT* smm);
  ShearModulus_BorjaT& operator=(const ShearModulus_BorjaT& smm) = delete;

  /*! Destructor of constant shear modulus model.   */
  ~ShearModulus_BorjaT();

  void outputProblemSpec(Uintah::ProblemSpecP& ps);

  /*! Get parameters */
  ParameterDict getParameters() const
  {
    ParameterDict params;
    params["mu0"] = d_mu0;
    return params;
  }

  /*! Compute the shear modulus */
  double computeInitialShearModulus();

  double computeShearModulus(const ModelState_Borja* state);

  double computeShearModulus(const ModelState_Borja* state) const;

  /*! Compute the shear strain energy */
  double computeStrainEnergy(const ModelState_Borja* state);

  /////////////////////////////////////////////////////////////////////////
  /*
    Compute q = 3 mu epse_s
       where mu = shear modulus
             epse_s = sqrt{2/3} ||ee||
             ee = deviatoric part of elastic strain = epse - 1/3 epse_v I
             epse = total elastic strain
             epse_v = tr(epse)
  */
  /////////////////////////////////////////////////////////////////////////
  double computeQ(const ModelState_Borja* state) const;

  /////////////////////////////////////////////////////////////////////////
  /*
    Compute dq/depse_s
  */
  /////////////////////////////////////////////////////////////////////////
  double computeDqDepse_s(const ModelState_Borja* state) const;

  /////////////////////////////////////////////////////////////////////////
  /*
    Compute dq/depse_v
  */
  /////////////////////////////////////////////////////////////////////////
  double computeDqDepse_v(const ModelState_Borja* state) const;

private:
  //  Compute shear modulus (volume strain dependent)
  double evalShearModulus(const double& epse_v) const;

  //  Shear stress magnitude computation
  double evalQ(const double& epse_v, const double& epse_s) const;

  //  Shear stress volume strain derivative computation
  double evalDqDepse_v(const double& epse_v, const double& epse_s) const;

  //  Shear stress shear strain derivative computation
  double evalDqDepse_s(const double& epse_v, const double& epse_s) const;
};
} // End namespace Uintah

#endif // __BORJA_SHEAR_MODEL_H__
