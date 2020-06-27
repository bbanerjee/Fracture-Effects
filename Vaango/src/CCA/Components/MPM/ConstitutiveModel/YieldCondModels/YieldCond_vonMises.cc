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
#include <CCA/Components/MPM/ConstitutiveModel/YieldCondModels/YieldCond_vonMises.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <cmath>

using namespace Uintah;
using namespace Vaango;

YieldCond_vonMises::YieldCond_vonMises(Uintah::ProblemSpecP& ps,
                                       InternalVariableModel* intvar)
{
  d_intvar = intvar;
}

YieldCond_vonMises::YieldCond_vonMises(const YieldCond_vonMises* ym)
{
  d_intvar = ym->d_intvar;
}

YieldCond_vonMises::~YieldCond_vonMises() = default;

void
YieldCond_vonMises::outputProblemSpec(Uintah::ProblemSpecP& ps)
{
  ProblemSpecP yield_ps = ps->appendChild("yield_condition");
  yield_ps->setAttribute("type", "von_mises");
}

//--------------------------------------------------------------
// Compute value of yield function
//--------------------------------------------------------------
std::pair<double, Util::YieldStatus>
YieldCond_vonMises::evalYieldCondition(const ModelStateBase* state)
{
  auto xi  = state->devStress - state->backStress;
  double f = evalYieldCondition(xi, state);
  if (f <= 0.0) {
    return std::make_pair(f, Util::YieldStatus::IS_ELASTIC);
  }
  return std::make_pair(f, Util::YieldStatus::HAS_YIELDED);
}

double
YieldCond_vonMises::evalYieldCondition(const Matrix3& xi,
                                       const ModelStateBase* state)
{
  double sigy   = state->yieldStress;
  double xiNorm = xi.Norm();
  double f      = Vaango::Util::sqrt_three_half * xiNorm - sigy;
  return f;
}

double
YieldCond_vonMises::evalYieldCondition(const double sigEqv,
                                       const double sigFlow,
                                       const double,
                                       const double,
                                       double& sig)
{
  sig = sigFlow;
  return (sigEqv - sigFlow);
}

double
YieldCond_vonMises::evalYieldConditionMax(const ModelStateBase* state)
{
  return state->yieldStress;
}

//--------------------------------------------------------------
// Compute derivatives of yield function
// df/dsigma = df/ds = sqrt(3/2) s / ||s||
// ||df/dsigma|| = ||df/ds|| = sqrt(3/2)
// df/dsigma / ||df/dsigma|| = s // ||s||
//--------------------------------------------------------------
Matrix3
YieldCond_vonMises::df_dsigma(const Matrix3& sig,
                              const double var1,
                              const double var2)
{
  return df_dsigmaDev(sig, var1, var2);
}

Matrix3
YieldCond_vonMises::df_dsigmaDev(const Matrix3& sig, const double, const double)
{
  Matrix3 s                = sig - Vaango::Util::Identity * (sig.Trace() / 3.0);
  Matrix3 df_ds_normalized = s / s.Norm();
  return df_ds_normalized;
}

/*! Derivative with respect to the Cauchy stress (\f$\sigma \f$)
    Assume f = sqrt{3/2} ||xi|| - sigma_y , xi = s - beta
    df/dsigma = sqrt(3/2) (xi / ||xi|| + 1/3 tr(beta) / ||xi|| I) */
Matrix3
YieldCond_vonMises::df_dsigma(const Matrix3& xi, const ModelStateBase* state)
{
  Matrix3 df_dsigma_normalized =
    (xi + Vaango::Util::Identity * (state->backStress.Trace() / 3.0)) *
    (Vaango::Util::sqrt_three_half / xi.Norm());
  df_dsigma_normalized /= df_dsigma_normalized.Norm();
  return df_dsigma_normalized;
}

/*! Derivative with respect to the \f$xi\f$ where \f$\xi = s - \beta \f$
    where \f$s\f$ is deviatoric part of Cauchy stress and
    \f$\beta\f$ is the backstress
    Assume f = sqrt{3/2} ||xi|| - sigma_y
    df/dxi = sqrt(3/2) xi / ||xi|| */
Matrix3
YieldCond_vonMises::df_dxi(const Matrix3& xi, const ModelStateBase*)
{
  Matrix3 df_dxi = xi * (Vaango::Util::sqrt_three_half / xi.Norm());
  return df_dxi;
}

/* Derivative with respect to \f$ s \f$ and \f$ \beta \f$ */
std::pair<Matrix3, Matrix3>
YieldCond_vonMises::df_dsigmaDev_dbeta(const Matrix3& xi,
                                       const ModelStateBase* state)
{
  Matrix3 df_ds    = df_dxi(xi, state);
  Matrix3 df_dbeta = df_ds * (-1.0);
  return std::make_pair(df_ds, df_dbeta);
}

/*! Derivative with respect to the plastic strain (\f$\epsilon^p \f$)
    Assume f = sqrt{3/2} ||xi|| - sigma_y */
double
YieldCond_vonMises::df_dplasticStrain(const Matrix3&,
                                      const double& dsigy_dep,
                                      const ModelStateBase*)
{
  return -dsigy_dep;
}

/*! Derivative with respect to the porosity (\f$\epsilon^p \f$)
    Assume f = sqrt{3/2} ||xi|| - sigma_y */
double
YieldCond_vonMises::df_dporosity(const Matrix3&, const ModelStateBase*)
{
  return 0.0;
}

/*! Compute h_alpha  where \f$d/dt(ep) = d/dt(gamma)~h_{\alpha}\f$ */
double
YieldCond_vonMises::eval_h_alpha(const Matrix3&, const ModelStateBase*)
{
  return 1.0;
}

/*! Compute h_phi  where \f$d/dt(phi) = d/dt(gamma)~h_{\phi}\f$ */
double
YieldCond_vonMises::eval_h_phi(const Matrix3&,
                               const double&,
                               const ModelStateBase*)
{
  return 0.0;
}

void
YieldCond_vonMises::computeElasPlasTangentModulus(
  const TangentModulusTensor& Ce,
  const Matrix3& sigma,
  double sigY,
  double dsigYdep,
  double porosity,
  double,
  TangentModulusTensor& Cep)
{
  // Calculate the derivative of the yield function wrt sigma
  Matrix3 f_sigma = df_dsigma(sigma, sigY, porosity);

  // Calculate derivative wrt plastic strain
  double f_q1 = dsigYdep;

  // Compute h_q1
  double sigma_f_sigma = sigma.Contract(f_sigma);
  double h_q1          = computePlasticStrainFactor(sigma_f_sigma, sigY);

  // Calculate elastic-plastic tangent modulus
  computeTangentModulus(Ce, f_sigma, f_q1, h_q1, Cep);
}

double
YieldCond_vonMises::computePlasticStrainFactor(double sigma_f_sigma,
                                               double sigma_Y)
{
  return sigma_f_sigma / sigma_Y;
}

void
YieldCond_vonMises::computeTangentModulus(const TangentModulusTensor& Ce,
                                          const Matrix3& f_sigma,
                                          double f_q1,
                                          double h_q1,
                                          TangentModulusTensor& Cep)
{
  double fqhq = f_q1 * h_q1;
  Matrix3 Cr(0.0), rC(0.0);
  double rCr = 0.0;
  for (int ii = 0; ii < 3; ++ii) {
    for (int jj = 0; jj < 3; ++jj) {
      Cr(ii, jj) = 0.0;
      rC(ii, jj) = 0.0;
      for (int kk = 0; kk < 3; ++kk) {
        for (int ll = 0; ll < 3; ++ll) {
          Cr(ii, jj) += Ce(ii, jj, kk, ll) * f_sigma(kk, ll);
          rC(ii, jj) += f_sigma(kk, ll) * Ce(kk, ll, ii, jj);
        }
      }
      rCr += rC(ii, jj) * f_sigma(ii, jj);
    }
  }
  for (int ii = 0; ii < 3; ++ii) {
    for (int jj = 0; jj < 3; ++jj) {
      for (int kk = 0; kk < 3; ++kk) {
        for (int ll = 0; ll < 3; ++ll) {
          Cep(ii, jj, kk, ll) =
            Ce(ii, jj, kk, ll) - Cr(ii, jj) * rC(kk, ll) / (-fqhq + rCr);
        }
      }
    }
  }
}
