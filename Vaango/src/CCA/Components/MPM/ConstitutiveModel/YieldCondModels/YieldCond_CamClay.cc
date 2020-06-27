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

#include <CCA/Components/MPM/ConstitutiveModel/YieldCondModels/YieldCond_CamClay.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <cmath>

using namespace Uintah;
using namespace Vaango;

YieldCond_CamClay::YieldCond_CamClay(Uintah::ProblemSpecP& ps,
                                     InternalVariableModel* intvar)
{
  d_intvar = intvar;

  ps->require("M", d_M);
}

YieldCond_CamClay::YieldCond_CamClay(const YieldCond_CamClay* yc)
{
  d_intvar = yc->d_intvar;

  d_M = yc->d_M;
}

YieldCond_CamClay::~YieldCond_CamClay() = default;

void
YieldCond_CamClay::outputProblemSpec(Uintah::ProblemSpecP& ps)
{
  ProblemSpecP yield_ps = ps->appendChild("yield_condition");
  yield_ps->setAttribute("type", "camclay");
  yield_ps->appendElement("M", d_M);
}

//--------------------------------------------------------------
// Evaluate yield condition (q = state->q
//                           p = state->p
//                           p_c = state->p_c)
//--------------------------------------------------------------
std::pair<double, Util::YieldStatus>
YieldCond_CamClay::evalYieldCondition(const ModelStateBase* state_input)
{
  const ModelState_CamClay* state =
    static_cast<const ModelState_CamClay*>(state_input);
  /*
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_CamClay.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }
  */

  double p   = state->p;
  double q   = state->q;
  double p_c = state->p_c;
  double f   = q * q / (d_M * d_M) + p * (p - p_c);
  if (f > 0.0) {
    return std::make_pair(f, Util::YieldStatus::HAS_YIELDED);
  }
  return std::make_pair(f, Util::YieldStatus::IS_ELASTIC);
}

//--------------------------------------------------------------
// Evaluate yield condition max (q = state->q
//                               p = state->p
//                               p_c = state->p_c)
//--------------------------------------------------------------
double
YieldCond_CamClay::evalYieldConditionMax(const ModelStateBase* state_input)
{
  const ModelState_CamClay* state =
    static_cast<const ModelState_CamClay*>(state_input);
  /*
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_CamClay.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }
  */

  double p_c  = state->p_c;
  double qmax = fabs(0.5 * d_M * p_c);
  return qmax * qmax / (d_M * d_M);
}

//--------------------------------------------------------------
// Derivatives needed by return algorithms and Newton iterations

//--------------------------------------------------------------
// Compute df/dp  where p = volumetric stress = 1/3 Tr(sigma)
//   df/dp = 2p - p_c
//--------------------------------------------------------------
double
YieldCond_CamClay::df_dp(const ModelStateBase* state_input)
{
  const ModelState_CamClay* state =
    static_cast<const ModelState_CamClay*>(state_input);
  /*
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_CamClay.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }
  */

  // std::std::cout << " p = " << state->p << " pc = " << state->p_c << " dfdp =
  // " <<
  // 2*state->p-state->p_c << "\n";
  return (2.0 * state->p - state->p_c);
}

//--------------------------------------------------------------
// Compute df/dq
//   df/dq = 2q/M^2
//--------------------------------------------------------------
double
YieldCond_CamClay::df_dq(const ModelStateBase* state_input)
{
  const ModelState_CamClay* state =
    static_cast<const ModelState_CamClay*>(state_input);
  /*
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_CamClay.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }
  */

  return 2.0 * state->q / (d_M * d_M);
}

//--------------------------------------------------------------
// Compute d/depse_v(df/dp)
//   df/dp = 2p(epse_v, epse_s) - p_c(epse_v)
//   d/depse_v(df/dp) = 2dp/depse_v - dp_c/depse_v
//
// Requires:  Equation of state and internal variable
//--------------------------------------------------------------
double
YieldCond_CamClay::d2f_dp_depsVol(const ModelStateBase* state_input,
                                  const PressureModel* eos,
                                  const ShearModulusModel*,
                                  const InternalVariableModel* intvar)
{
  double dpdepsev = eos->computeDpDepse_v(state_input);
  double dpcdepsev =
    intvar->computeVolStrainDerivOfInternalVariable(nullptr, state_input);
  return 2.0 * dpdepsev - dpcdepsev;
}

//--------------------------------------------------------------
// Compute d/depse_s(df/dp)
//   df/dp = 2p(epse_v, epse_s) - p_c(epse_v)
//   d/depse_s(df/dp) = 2dp/depse_s
//
// Requires:  Equation of state
//--------------------------------------------------------------
double
YieldCond_CamClay::d2f_dp_depsDev(const ModelStateBase* state_input,
                                  const PressureModel* eos,
                                  const ShearModulusModel*,
                                  const InternalVariableModel*)
{
  double dpdepses = eos->computeDpDepse_s(state_input);
  return 2.0 * dpdepses;
}

//--------------------------------------------------------------
// Compute d/depse_v(df/dq)
//   df/dq = 2q(epse_v, epse_s)/M^2
//   d/depse_v(df/dq) = 2/M^2 dq/depse_v
//
// Requires:  Shear modulus model
//--------------------------------------------------------------
double
YieldCond_CamClay::d2f_dq_depsVol(const ModelStateBase* state_input,
                                  const PressureModel*,
                                  const ShearModulusModel* shear,
                                  const InternalVariableModel*)
{
  double dqdepsev = shear->computeDqDepse_v(state_input);
  return (2.0 * dqdepsev) / (d_M * d_M);
}

//--------------------------------------------------------------
// Compute d/depse_s(df/dq)
//   df/dq = 2q(epse_v, epse_s)/M^2
//   d/depse_s(df/dq) = 2/M^2 dq/depse_s
//
// Requires:  Shear modulus model
//--------------------------------------------------------------
double
YieldCond_CamClay::d2f_dq_depsDev(const ModelStateBase* state_input,
                                  const PressureModel*,
                                  const ShearModulusModel* shear,
                                  const InternalVariableModel*)
{
  double dqdepses = shear->computeDqDepse_s(state_input);
  return (2.0 * dqdepses) / (d_M * d_M);
}

//--------------------------------------------------------------
// Compute df/depse_v
//   df/depse_v = df/dq dq/depse_v + df/dp dp/depse_v - p dp_c/depse_v
//
// Requires:  Equation of state, shear modulus model, internal variable model
//--------------------------------------------------------------
double
YieldCond_CamClay::df_depsVol(const ModelStateBase* state_input,
                              const PressureModel* eos,
                              const ShearModulusModel* shear,
                              const InternalVariableModel* intvar)
{
  const ModelState_CamClay* state =
    static_cast<const ModelState_CamClay*>(state_input);
  /*
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_CamClay.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }
  */

  double dfdq     = df_dq(state_input);
  double dfdp     = df_dp(state_input);
  double dqdepsev = shear->computeDqDepse_v(state_input);
  double dpdepsev = eos->computeDpDepse_v(state_input);
  double dpcdepsev =
    intvar->computeVolStrainDerivOfInternalVariable(nullptr, state_input);
  double dfdepsev = dfdq * dqdepsev + dfdp * dpdepsev - state->p * dpcdepsev;

  return dfdepsev;
}

//--------------------------------------------------------------
// Compute df/depse_s
//   df/depse_s = df/dq dq/depse_s + df/dp dp/depse_s
//
// Requires:  Equation of state, shear modulus model
//--------------------------------------------------------------
double
YieldCond_CamClay::df_depsDev(const ModelStateBase* state_input,
                              const PressureModel* eos,
                              const ShearModulusModel* shear,
                              const InternalVariableModel*)
{
  double dfdq     = df_dq(state_input);
  double dfdp     = df_dp(state_input);
  double dqdepses = shear->computeDqDepse_s(state_input);
  double dpdepses = eos->computeDpDepse_s(state_input);
  double dfdepses = dfdq * dqdepses + dfdp * dpdepses;

  return dfdepses;
}

//--------------------------------------------------------------
// Other yield condition functions

// Evaluate yield condition (s = deviatoric stress
//                           p = state->p
//                           p_c = state->p_c)
double
YieldCond_CamClay::evalYieldCondition(const Uintah::Matrix3&,
                                      const ModelStateBase* state_input)
{
  const ModelState_CamClay* state =
    static_cast<const ModelState_CamClay*>(state_input);
  /*
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_CamClay.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }
  */

  double p     = state->p;
  double q     = state->q;
  double pc    = state->p_c;
  double dummy = 0.0;
  return evalYieldCondition(p, q, pc, 0.0, dummy);
}

double
YieldCond_CamClay::evalYieldCondition(const double p,
                                      const double q,
                                      const double p_c,
                                      const double,
                                      double&)
{
  return q * q / (d_M * d_M) + p * (p - p_c);
}

//--------------------------------------------------------------
// Other derivatives

// Compute df/dsigma
//    df/dsigma = (2p - p_c)/3 I + sqrt(3/2) 2q/M^2 s/||s||
//              = 1/3 df/dp I + sqrt(3/2) df/dq s/||s||
//              = 1/3 df/dp I + df/ds
// where
//    s = sigma - 1/3 tr(sigma) I
Uintah::Matrix3
YieldCond_CamClay::df_dsigma(const Uintah::Matrix3& sig,
                             const double p_c,
                             const double)
{
  Matrix3 One;
  One.Identity();
  double p       = sig.Trace() / 3.0;
  Matrix3 sigDev = sig - One * p;
  double df_dp   = 2.0 * p - p_c;
  Matrix3 df_ds = df_dsigmaDev(sigDev, 0.0, 0.0);
  Matrix3 derivative = One * (df_dp / 3.0) + df_ds;
  return derivative/derivative.Norm();
}

// Compute df/ds  where s = deviatoric stress
//    df/ds = sqrt(3/2) df/dq s/||s|| = sqrt(3/2) 2q/M^2 n
Uintah::Matrix3
YieldCond_CamClay::df_dsigmaDev(const Uintah::Matrix3& sigDev,
                                const double,
                                const double)
{
  double sigDevNorm = sigDev.Norm();
  Matrix3 n         = sigDev / sigDevNorm;
  double q_scaled   = 3.0 * sigDevNorm;
  Matrix3 derivative        = n * (q_scaled / d_M * d_M);
  return derivative/derivative.Norm();
}

/*! Derivative with respect to the Cauchy stress (\f$\sigma \f$) */
//   p_c = state->p_c
Uintah::Matrix3
YieldCond_CamClay::df_dsigma(const Matrix3& sig,
                             const ModelStateBase* state_input)
{
  const ModelState_CamClay* state =
    static_cast<const ModelState_CamClay*>(state_input);
  /*
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_CamClay.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }
  */

  return df_dsigma(sig, state->p_c, 0.0);
}

/*! Derivative with respect to the \f$xi\f$ where \f$\xi = s \f$
    where \f$s\f$ is deviatoric part of Cauchy stress */
Uintah::Matrix3
YieldCond_CamClay::df_dxi(const Matrix3& sigDev,
                          const ModelStateBase*)
{
  return df_dsigmaDev(sigDev, 0.0, 0.0);
}

/* Derivative with respect to \f$ s \f$ and \f$ \beta \f$ */
std::pair<Uintah::Matrix3, Uintah::Matrix3>
YieldCond_CamClay::df_dsigmaDev_dbeta(const Matrix3& sigDev,
                                      const ModelStateBase*)
{
  Matrix3 df_ds = df_dsigmaDev(sigDev, 0.0, 0.0);
  Matrix3 zero(0.0);
  Matrix3 df_dbeta = zero;
  return std::make_pair(df_ds, df_dbeta);
}

/*! Derivative with respect to the plastic strain (\f$\epsilon^p \f$) */
double
YieldCond_CamClay::df_dplasticStrain(const Matrix3&,
                                     const double& dsigy_dep,
                                     const ModelStateBase*)
{
  std::cout << "YieldCond_CamClay: df_dplasticStrain not implemented yet "
            << "\n";
  return 0.0;
}

/*! Derivative with respect to the porosity (\f$\epsilon^p \f$) */
double
YieldCond_CamClay::df_dporosity(const Matrix3&, const ModelStateBase*)
{
  std::cout << "YieldCond_CamClay: df_dporosity not implemented yet "
            << "\n";
  return 0.0;
}

/*! Compute h_alpha  where \f$d/dt(ep) = d/dt(gamma)~h_{\alpha}\f$ */
double
YieldCond_CamClay::eval_h_alpha(const Matrix3&, const ModelStateBase*)
{
  std::cout << "YieldCond_CamClay: eval_h_alpha not implemented yet "
            << "\n";
  return 1.0;
}

/*! Compute h_phi  where \f$d/dt(phi) = d/dt(gamma)~h_{\phi}\f$ */
double
YieldCond_CamClay::eval_h_phi(const Matrix3&,
                              const double&,
                              const ModelStateBase*)
{
  std::cout << "YieldCond_CamClay: eval_h_phi not implemented yet "
            << "\n";
  return 0.0;
}

//--------------------------------------------------------------
// Tangent moduli
void
YieldCond_CamClay::computeElasPlasTangentModulus(const TangentModulusTensor& Ce,
                                                 const Matrix3& sigma,
                                                 double sigY,
                                                 double dsigYdep,
                                                 double porosity,
                                                 double,
                                                 TangentModulusTensor& Cep)
{
  std::cout
    << "YieldCond_CamClay: computeElasPlasTangentModulus not implemented yet "
    << "\n";
  return;
}

void
YieldCond_CamClay::computeTangentModulus(const TangentModulusTensor& Ce,
                                         const Matrix3& f_sigma,
                                         double f_q1,
                                         double h_q1,
                                         TangentModulusTensor& Cep)
{
  std::cout << "YieldCond_CamClay: computeTangentModulus not implemented yet "
            << "\n";
  return;
}
