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

#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCond_Tabular.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCondUtils.h>
#include <Core/Exceptions/InternalError.h>
#include <Core/Exceptions/InvalidValue.h>
#include <Core/Exceptions/ProblemSetupException.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <chrono>
#include <cmath>

#define USE_GEOMETRIC_BISECTION
//#define DEBUG_YIELD_BISECTION
//#define DEBUG_YIELD_BISECTION_I1_J2
//#define DEBUG_YIELD_BISECTION_R
//#define CHECK_FOR_NANS

using namespace Vaango;

const double YieldCond_Tabular::sqrt_two = std::sqrt(2.0);
const double YieldCond_Tabular::sqrt_three = std::sqrt(3.0);
const double YieldCond_Tabular::one_sqrt_three = 1.0 / sqrt_three;

YieldCond_Tabular::YieldCond_Tabular(Uintah::ProblemSpecP& ps)
  : d_yield(ps)
{
  // Check the input parameters
  checkInputParameters();
  setYieldConditionRange();
}

void
YieldCond_Tabular::checkInputParameters()
{
  // **TODO** Check convexity 
}

YieldCond_Tabular::YieldCond_Tabular(const YieldCond_Tabular* yc)
{
  d_yield = yc->d_yield;
  d_I1bar_min = yc->d_I1bar_min;
  d_I1bar_max = yc->d_I1bar_max;
  d_sqrtJ2_max = yc->d_sqrtJ2_max;
}

void
YieldCond_Tabular::outputProblemSpec(Uintah::ProblemSpecP& ps)
{
  ProblemSpecP yield_ps = ps->appendChild("plastic_yield_condition");
  yield_ps->setAttribute("type", "tabular");

  d_yield.table.outputProblemSpec(yield_ps);
}

//--------------------------------------------------------------
// Evaluate yield condition
//
// f := sqrt(J2) - g(p) = 0
// where
//     J2 = 1/2 s:s,  s = sigma - p I,  p = 1/3 Tr(sigma)
//     g(pbar) = table,  pbar = -p
//
// Returns:
//   hasYielded = -1.0 (if elastic)
//              =  1.0 (otherwise)
//--------------------------------------------------------------
double
YieldCond_Tabular::evalYieldCondition(const ModelStateBase* state_input)
{
  const ModelState_Tabular* state =
    dynamic_cast<const ModelState_Tabular*>(state_input);
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_Tabular.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }

  double p_bar = -state->I1/3;
  if (p_bar < d_I1bar_min) {
    return 1.0;
  }

  DoubleVec1D gg = d_yield.table.interpolate<1>({{p_bar}}); 
  //std::cout << "p_bar = " << p_bar << " gg = " << gg[0] 
  //          << " sqrtJ2 = " << state->sqrt_J2 << std::endl;
  if (state->sqrt_J2 > gg[0]) {
    return 1.0;
  }

  return -1.0;
}

//--------------------------------------------------------------
// Derivatives needed by return algorithms and Newton iterations

//--------------------------------------------------------------
// Evaluate yield condition max value of sqrtJ2
//--------------------------------------------------------------
double
YieldCond_Tabular::evalYieldConditionMax(const ModelStateBase* )
{
  setYieldConditionRange();
  return d_sqrtJ2_max;
}

/* Yield surface does not change over time.  So we can caculate the
   max sqrt(J2) at the beginning */
void 
YieldCond_Tabular::setYieldConditionRange() {

  d_I1bar_min = std::numeric_limits<double>::max();
  d_I1bar_max = std::numeric_limits<double>::min();
  d_sqrtJ2_max = std::numeric_limits<double>::min();

  DoubleVec1D pvals = 
    d_yield.table.getIndependentVarData("Pressure", IndexKey(0,0,0,0));
  for (auto p_bar : pvals) {
    DoubleVec1D gg = d_yield.table.interpolate<1>({{p_bar}}); 
    d_I1bar_min = std::min(d_I1bar_min, 3.0*p_bar);
    d_I1bar_max = std::max(d_I1bar_max, 3.0*p_bar);
    d_sqrtJ2_max = std::max(d_sqrtJ2_max, gg[0]);
  }
}

//--------------------------------------------------------------
/*! Compute Derivative with respect to the Cauchy stress (\f$\sigma \f$)
 *  Compute df/dsigma
 *
 *  for the yield function
 *      f := sqrt(J2(s)) - g(p) = 0
 *  where
 *      J2 = 1/2 s:s,  s = sigma - p I,  p = 1/3 Tr(sigma)
 *      g(pbar) = table, pbar = -p
 *
 *  The derivative is
 *      df/dsigma = df/dp dp/dsigma + df/ds : ds/dsigma
 *  where
 *      df/dp = computeVolStressDerivOfYieldFunction
 *      dp/dsigma = 1/3 I
 *  and
 *      df/ds = df/dJ2 dJ2/ds
 *      df/dJ2 = computeDevStressDerivOfYieldFunction
 *      dJ2/ds = s
 *      ds/dsigma = I(4s) - 1/3 II
 *  which means
 *      df/dp dp/dsigma = 1/3 df/dp I
 *      df/ds : ds/dsigma = df/dJ2 s : [I(4s) - 1/3 II]
 *                        = df/dJ2 s
*/
void
YieldCond_Tabular::eval_df_dsigma(const Matrix3&,
                                  const ModelStateBase* state_input,
                                  Matrix3& df_dsigma)
{
  const ModelState_Tabular* state =
    dynamic_cast<const ModelState_Tabular*>(state_input);
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_Tabular.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }

  double df_dp = computeVolStressDerivOfYieldFunction(state_input);
  double df_dJ2 = computeDevStressDerivOfYieldFunction(state_input);

  Matrix3 One;
  One.Identity();
  Matrix3 p_term = One * (df_dp / 3.0);
  Matrix3 s_term = state->deviatoricStressTensor * (df_dJ2);

  df_dsigma = p_term + s_term;

  return;
}

//--------------------------------------------------------------
// Compute df/dp  where pI = volumetric stress = 1/3 Tr(sigma) I
//   df/dp = derivative of the yield function wrt p
//
// for the yield function
//     f := sqrt(J2) - g(p) = 0
// where
//     J2 = 1/2 s:s,  s = sigma - p I,  p = 1/3 Tr(sigma)
//     g(pbar) := table,  pbar = -p
//
// the derivative is
//     df/dp = -dg/dp = -dg/dpbar dpbar/dp = dg/dpbar
//--------------------------------------------------------------
double
YieldCond_Tabular::computeVolStressDerivOfYieldFunction(
  const ModelStateBase* state_input)
{
  const ModelState_Tabular* state =
    dynamic_cast<const ModelState_Tabular*>(state_input);
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_Tabular.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }

  double epsilon = 1.0e-6;
  double p_bar = -state->I1/3;
  double dg_dpbar = 0.0;
  if (p_bar < d_I1bar_min/3.0) {
    TabularData shifted(d_yield.table);
    shifted.translateIndepVar0<1>(p_bar+epsilon);
    DoubleVec1D g_lo = shifted.interpolate<1>({{p_bar-epsilon}});
    DoubleVec1D g_hi = shifted.interpolate<1>({{p_bar+epsilon}});
    dg_dpbar = (g_hi[0] - g_lo[0])/(2*epsilon);
  } else {
    DoubleVec1D g_lo = d_yield.table.interpolate<1>({{p_bar-epsilon}});
    DoubleVec1D g_hi = d_yield.table.interpolate<1>({{p_bar+epsilon}});
    dg_dpbar = (g_hi[0] - g_lo[0])/(2*epsilon);
  }

  return dg_dpbar;
}

//--------------------------------------------------------------
// Compute df/dJ2  where J2 = 1/2 s:s ,  s = sigma - p I,  p = 1/3 Tr(sigma)
//   s = derivatoric stress
//   df/dJ2 = derivative of the yield function wrt J2
//
// for the yield function
//     f := sqrt(J2) - g(p) = 0
// where
//     J2 = 1/2 s:s,  s = sigma - p I,  p = 1/3 Tr(sigma)
//     g(pbar) := table, pbar = -p
//
// the derivative is
//     df/dJ2 = 1/(2 sqrt(J2))
//--------------------------------------------------------------
double
YieldCond_Tabular::computeDevStressDerivOfYieldFunction(
  const ModelStateBase* state_input)
{
  const ModelState_Tabular* state =
    dynamic_cast<const ModelState_Tabular*>(state_input);
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_Tabular.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }

  double df_dJ2 = (state->sqrt_J2 == 0) ? 0 : 1/(2*state->sqrt_J2);

  return df_dJ2;
}

/**
 * Function: getClosestPoint
 * Purpose: Get the point on the yield surface that is closest to a given point
 * (2D)
 * Inputs:
 *  state = current state
 *  px = x-coordinate of point
 *  py = y-coordinate of point
 * Outputs:
 *  cpx = x-coordinate of closest point on yield surface
 *  cpy = y-coordinate of closest point
 */
bool
YieldCond_Tabular::getClosestPoint(const ModelStateBase* state_input,
                                   const double& px, const double& py,
                                   double& cpx, double& cpy)
{
  const ModelState_Tabular* state =
    dynamic_cast<const ModelState_Tabular*>(state_input);
  if (!state) {
    std::ostringstream out;
    out << "**ERROR** The correct ModelState object has not been passed."
        << " Need ModelState_Tabular.";
    throw Uintah::InternalError(out.str(), __FILE__, __LINE__);
  }

  //std::chrono::time_point<std::chrono::system_clock> start, end;
  //start = std::chrono::system_clock::now();
  Point pt(px, py, 0.0);
  Point closest(0.0, 0.0, 0.0);
  getClosestPointGeometricBisect(state, pt, closest);
  cpx = closest.x();
  cpy = closest.y();
  //end = std::chrono::system_clock::now();
  //std::cout << "Geomeric Bisection : Time taken = " <<
  //std::chrono::duration<double>(end-start).count() << std::endl;

  return true;
}

void
YieldCond_Tabular::getClosestPointGeometricBisect(
  const ModelState_Tabular* state, const Uintah::Point& z_r_pt,
  Uintah::Point& z_r_closest)
{
  // Get the bulk and shear moduli and compute sqrt(3/2 K/G)
  double sqrtKG = std::sqrt(1.5 * state->bulkModulus / state->shearModulus);

  // Compute diameter of yield surface in z-r space
  double I1_diff = d_I1bar_max - d_I1bar_min;
  double sqrtJ2_diff = 2*d_sqrtJ2_max;
  double yield_surf_dia_zrprime =
    std::max(I1_diff * one_sqrt_three, sqrtJ2_diff * sqrt_two * sqrtKG);
  double dist_to_trial_zr =
    std::sqrt(z_r_pt.x() * z_r_pt.x() + z_r_pt.y() * z_r_pt.y());
  double dist_dia_ratio = dist_to_trial_zr / yield_surf_dia_zrprime;
  int num_points = std::max(5, (int)std::ceil(std::log(dist_dia_ratio)));

  // Set up I1 limits
  double I1_min = -d_I1bar_max;
  double I1_max = -d_I1bar_min;

  // Set up bisection
  double eta_lo = 0.0, eta_hi = 1.0;

  // Set up mid point
  double I1_mid = 0.5 * (I1_min + I1_max);
  double eta_mid = 0.5 * (eta_lo + eta_hi);

  // Do bisection
  int iters = 1;
  double TOLERANCE = 1.0e-10;
  std::vector<Uintah::Point> z_r_points;
  std::vector<Uintah::Point> z_r_segments;
  std::vector<Uintah::Point> z_r_segment_points;
  Uintah::Point z_r_closest_old;
  z_r_closest_old.x(std::numeric_limits<double>::max());
  z_r_closest_old.y(std::numeric_limits<double>::max());
  z_r_closest_old.z(0.0);
  while (std::abs(eta_hi - eta_lo) > TOLERANCE) {

    // Get the yield surface points
    z_r_points.clear();
    getYieldSurfacePointsAll_RprimeZ(sqrtKG, I1_min, I1_max,
                                     num_points, z_r_points);

    // Find the closest point
    Vaango::Util::findClosestPoint(z_r_pt, z_r_points, z_r_closest);

#ifdef DEBUG_YIELD_BISECTION_R
    std::cout << "iteration = " << iters << std::endl;
    std::cout << "K = " << state->bulkModulus << std::endl;
    std::cout << "G = " << state->shearModulus << std::endl;
    std::cout << "z_r_pt = c(" << z_r_pt.x() << "," << z_r_pt.y() << ")"
              << std::endl;
    std::cout << "z_r_closest = c(" << z_r_closest.x() << "," << z_r_closest.y()
              << ")" << std::endl;
    std::cout << "z_r_yield_z = c(";
    for (auto& pt : z_r_points) {
      if (pt == z_r_points.back()) {
        std::cout << pt.x();
      } else {
        std::cout << pt.x() << ",";
      }
    }
    std::cout << ")" << std::endl;
    std::cout << "z_r_yield_r = c(";
    for (auto& pt : z_r_points) {
      if (pt == z_r_points.back()) {
        std::cout << pt.y();
      } else {
        std::cout << pt.y() << ",";
      }
    }
    std::cout << ")" << std::endl;
    if (iters == 1) {
      std::cout << "zr_df = \n"
                << "  ComputeFullYieldSurface(yieldParams, X, pbar_w, K, G, "
                   "num_points,\n"
                << "                          z_r_pt, z_r_closest, "
                   "z_r_yield_z, z_r_yield_r,\n"
                << "                          iteration, consistency_iter)"
                << std::endl;
    } else {
      std::cout << "zr_df = rbind(zr_df,\n"
                << "  ComputeFullYieldSurface(yieldParams, X, pbar_w, K, G, "
                   "num_points,\n"
                << "                          z_r_pt, z_r_closest, "
                   "z_r_yield_z, z_r_yield_r,\n"
                << "                          iteration, consistency_iter))"
                << std::endl;
    }
#endif

#ifdef DEBUG_YIELD_BISECTION
    std::cout << "Iteration = " << iters << std::endl;
    std::cout << "State = " << *state << std::endl;
    std::cout << "z_r_pt = " << z_r_pt << ";" << std::endl;
    std::cout << "z_r_closest = " << z_r_closest << ";" << std::endl;
    std::cout << "z_r_yield_z = [";
    for (auto& pt : z_r_points) {
      std::cout << pt.x() << " ";
    }
    std::cout << "];" << std::endl;
    std::cout << "z_r_yield_r = [";
    for (auto& pt : z_r_points) {
      std::cout << pt.y() << " ";
    }
    std::cout << "];" << std::endl;
    std::cout << "plot(z_r_yield_z, z_r_yield_r); hold on;" << std::endl;
    std::cout << "plot(z_r_pt(1), z_r_pt(2));" << std::endl;
    std::cout << "plot(z_r_closest(1), z_r_closest(2));" << std::endl;
    std::cout
      << "plot([z_r_pt(1) z_r_closest(1)],[z_r_pt(2) z_r_closest(2)], '--');"
      << std::endl;
#endif
#ifdef DEBUG_YIELD_BISECTION_I1_J2
    double fac_z = sqrt_three;
    double fac_r = d_local.BETA * sqrtKG * sqrt_two;
    std::cout << "Iteration = " << iters << std::endl;
    std::cout << "I1_J2_trial = [" << z_r_pt.x() * fac_z << " "
              << z_r_pt.y() / fac_r << "];" << std::endl;
    std::cout << "I1_J2_closest = [" << z_r_closest.x() * fac_z << " "
              << z_r_closest.y() / fac_r << "];" << std::endl;
    std::cout << "I1_J2_yield_I1 = [";
    for (auto& pt : z_r_points) {
      std::cout << pt.x() * fac_z << " ";
    }
    std::cout << "];" << std::endl;
    std::cout << "I1_J2_yield_J2 = [";
    for (auto& pt : z_r_points) {
      std::cout << pt.y() / fac_r << " ";
    }
    std::cout << "];" << std::endl;
    std::cout << "plot(I1_J2_yield_I1, I1_J2_yield_J2); hold on;" << std::endl;
    std::cout << "plot(I1_J2_trial(1), I1_J2_trial(2), 'ko');" << std::endl;
    std::cout << "plot(I1_J2_closest(1), I1_J2_closest(2));" << std::endl;
    std::cout << "plot([I1_J2_trial(1) I1_J2_closest(1)],[I1_J2_trial(2) "
                 "I1_J2_closest(2)], '--');"
              << std::endl;
#endif

    // Compute I1 for the closest point
    double I1_closest = sqrt_three * z_r_closest.x();

    // If (I1_closest < I1_mid)
    if (I1_closest < I1_mid) {
      I1_max = I1_mid;
      eta_hi = eta_mid;
    } else {
      I1_min = I1_mid;
      eta_lo = eta_mid;
    }

    I1_mid = 0.5 * (I1_min + I1_max);
    eta_mid = 0.5 * (eta_lo + eta_hi);

    // Distance to old closest point
    if (iters > 10 && (z_r_closest - z_r_closest_old).length2() < 1.0e-16) {
      break;
    }
    z_r_closest_old = z_r_closest;

    ++iters;
  }

  return;
}

/* Get the points on the yield surface */
void
YieldCond_Tabular::getYieldSurfacePointsAll_RprimeZ(const double& sqrtKG,
  const double& I1_min, const double& I1_max, const int& num_points,
  std::vector<Uintah::Point>& z_r_vec)
{
  // Compute z and r'
  computeZ_and_RPrime(sqrtKG, I1_min, I1_max, num_points, z_r_vec);

  return;
}

/*! Compute a vector of z, r' values given a range of I1 values */
void
YieldCond_Tabular::computeZ_and_RPrime(const double& sqrtKG,
  const double& I1_min, const double& I1_max, const int& num_points,
  std::vector<Uintah::Point>& z_r_vec)
{
  // Set up points
  double rad = 0.5 * (I1_max - I1_min);
  double cen = 0.5 * (I1_max + I1_min);
  double theta_max = std::acos(std::max((I1_min - cen) / rad, -1.0));
  double theta_min = std::acos(std::min((I1_max - cen) / rad, 1.0));
  std::vector<double> theta_vec;
  Vaango::Util::linspace(theta_min, theta_max, num_points, theta_vec);

  for (auto theta : theta_vec) {
    double I1 = std::max(cen + rad * std::cos(theta), I1_max);
    double p_bar = -I1/3.0;
    DoubleVec1D gg = d_yield.table.interpolate<1>({{p_bar}}); 
    double sqrt_J2 = gg[0];

// Check for nans
#ifdef CHECK_FOR_NANS
    if (std::isnan(I1) || std::isnan(sqrt_J2)) {
      std::cout << "theta = " << theta 
                << " I1 = " << I1
                << " sqrtJ2 = " << sqrt_J2 << std::endl;
      std::cout << "rad = " << rad << " cen = " << cen
                << " theta_max = " << theta_max << " theta_min = " << theta_min
                << " I1_max = " << I1_max << " I1_min = " << I1_min
                << std::endl;
    }
#endif

    z_r_vec.push_back(Uintah::Point(
      I1 / sqrt_three, sqrt_two * sqrt_J2 * sqrtKG, 0.0));
  }

  return;
}

//--------------------------------------------------------------
// Other yield condition functions

//--------------------------------------------------------------
// Compute d/depse_v(df/dp)
//   df/dp = 6*Ff*(a2*a3*exp(3*a2*p) + a4)*Fc^2 -
//             6*Ff^2*(kappa - I1)/(kappa - X)^2
//   d/depse_v(df/dp) =
//
// Requires:  Equation of state and internal variable
//--------------------------------------------------------------
double
YieldCond_Tabular::computeVolStrainDerivOfDfDp(
  const ModelStateBase* state_input, const PressureModel* eos,
  const ShearModulusModel*, const InternalVariableModel*)
{
  std::ostringstream out;
  out << "**ERROR** computeVolStrainDerivOfDfDp should not be called by "
      << " models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return 0.0;
}

//--------------------------------------------------------------
// Compute d/depse_s(df/dp)
//   df/dp =
//   d/depse_s(df/dp) =
//
// Requires:  Equation of state
//--------------------------------------------------------------
double
YieldCond_Tabular::computeDevStrainDerivOfDfDp(
  const ModelStateBase* state_input, const PressureModel* eos,
  const ShearModulusModel*, const InternalVariableModel*)
{
  std::ostringstream out;
  out << "**ERROR** computeDevStrainDerivOfDfDp should not be called by "
      << " models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return 0.0;
}

//--------------------------------------------------------------
// Compute d/depse_v(df/dq)
//   df/dq =
//   d/depse_v(df/dq) =
//
// Requires:  Shear modulus model
//--------------------------------------------------------------
double
YieldCond_Tabular::computeVolStrainDerivOfDfDq(
  const ModelStateBase* state_input, const PressureModel*,
  const ShearModulusModel* shear, const InternalVariableModel*)
{
  std::ostringstream out;
  out << "**ERROR** computeVolStrainDerivOfDfDq should not be called by "
      << " models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return 0.0;
}

//--------------------------------------------------------------
// Compute d/depse_s(df/dq)
//   df/dq =
//   d/depse_s(df/dq) =
//
// Requires:  Shear modulus model
//--------------------------------------------------------------
double
YieldCond_Tabular::computeDevStrainDerivOfDfDq(
  const ModelStateBase* state_input, const PressureModel*,
  const ShearModulusModel* shear, const InternalVariableModel*)
{
  std::ostringstream out;
  out << "**ERROR** computeDevStrainDerivOfDfDq should not be called by "
      << " models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return 0.0;
}

//--------------------------------------------------------------
// Compute df/depse_v
//   df/depse_v =
//
// Requires:  Equation of state, shear modulus model, internal variable model
//--------------------------------------------------------------
double
YieldCond_Tabular::computeVolStrainDerivOfYieldFunction(
  const ModelStateBase* state_input, const PressureModel* eos,
  const ShearModulusModel* shear, const InternalVariableModel*)
{
  std::ostringstream out;
  out
    << "**ERROR** computeVolStrainDerivOfYieldFunction should not be called by "
    << " models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return 0.0;
}

//--------------------------------------------------------------
// Compute df/depse_s
//   df/depse_s =
//
// Requires:  Equation of state, shear modulus model
//--------------------------------------------------------------
double
YieldCond_Tabular::computeDevStrainDerivOfYieldFunction(
  const ModelStateBase* state_input, const PressureModel* eos,
  const ShearModulusModel* shear, const InternalVariableModel*)
{
  std::ostringstream out;
  out
    << "**ERROR** computeVolStrainDerivOfYieldFunction should not be called by "
    << " models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return 0.0;
}

// Evaluate the yield function.
double
YieldCond_Tabular::evalYieldCondition(const double p, const double q,
                                      const double dummy0, const double dummy1,
                                      double& dummy2)
{
  std::ostringstream out;
  out
    << "**ERROR** Deprecated evalYieldCondition with double arguments. "
    << " Should not be called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return 0.0;
}

// Evaluate yield condition (s = deviatoric stress
//                           p = state->p)
double
YieldCond_Tabular::evalYieldCondition(const Uintah::Matrix3&,
                                      const ModelStateBase* state_input)
{
  std::ostringstream out;
  out << "**ERROR** evalYieldCondition with a Matrix3 argument should not be "
         "called by "
      << " models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return 0.0;
}

//--------------------------------------------------------------
// Other derivatives

// Compute df/dsigma
//    df/dsigma =
// where
//    s = sigma - 1/3 tr(sigma) I
void
YieldCond_Tabular::evalDerivOfYieldFunction(const Uintah::Matrix3& sig,
                                            const double p_c, const double,
                                            Uintah::Matrix3& derivative)
{
  std::ostringstream out;
  out << "**ERROR** evalDerivOfYieldCondition with a Matrix3 argument should "
         "not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return;
}

// Compute df/ds  where s = deviatoric stress
//    df/ds =
void
YieldCond_Tabular::evalDevDerivOfYieldFunction(const Uintah::Matrix3& sigDev,
                                               const double, const double,
                                               Uintah::Matrix3& derivative)
{
  std::ostringstream out;
  out << "**ERROR** evalDerivOfYieldCondition with a Matrix3 argument should "
         "not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);

  return;
}

/*! Derivative with respect to the \f$xi\f$ where \f$\xi = s \f$
    where \f$s\f$ is deviatoric part of Cauchy stress */
void
YieldCond_Tabular::eval_df_dxi(const Matrix3& sigDev, const ModelStateBase*,
                               Matrix3& df_ds)

{
  std::ostringstream out;
  out << "**ERROR** eval_df_dxi with a Matrix3 argument should not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);
  return;
}

/* Derivative with respect to \f$ s \f$ and \f$ \beta \f$ */
void
YieldCond_Tabular::eval_df_ds_df_dbeta(const Matrix3& sigDev,
                                       const ModelStateBase*, Matrix3& df_ds,
                                       Matrix3& df_dbeta)
{
  std::ostringstream out;
  out << "**ERROR** eval_df_ds_df_dbeta with a Matrix3 argument should not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);
  return;
}

/*! Derivative with respect to the plastic strain (\f$\epsilon^p \f$) */
double
YieldCond_Tabular::eval_df_dep(const Matrix3&, const double& dsigy_dep,
                               const ModelStateBase*)
{
  std::ostringstream out;
  out << "**ERROR** eval_df_dep with a Matrix3 argument should not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);
  return 0.0;
}

/*! Derivative with respect to the porosity (\f$\epsilon^p \f$) */
double
YieldCond_Tabular::eval_df_dphi(const Matrix3&, const ModelStateBase*)
{
  std::ostringstream out;
  out << "**ERROR** eval_df_dphi with a Matrix3 argument should not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);
  return 0.0;
}

/*! Compute h_alpha  where \f$d/dt(ep) = d/dt(gamma)~h_{\alpha}\f$ */
double
YieldCond_Tabular::eval_h_alpha(const Matrix3&, const ModelStateBase*)
{
  std::ostringstream out;
  out << "**ERROR** eval_h_alpha with a Matrix3 argument should not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);
  return 1.0;
}

/*! Compute h_phi  where \f$d/dt(phi) = d/dt(gamma)~h_{\phi}\f$ */
double
YieldCond_Tabular::eval_h_phi(const Matrix3&, const double&,
                              const ModelStateBase*)
{
  std::ostringstream out;
  out << "**ERROR** eval_h_phi with a Matrix3 argument should not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);
  return 0.0;
}

//--------------------------------------------------------------
// Tangent moduli
void
YieldCond_Tabular::computeElasPlasTangentModulus(const TangentModulusTensor& Ce,
                                                 const Matrix3& sigma,
                                                 double sigY, double dsigYdep,
                                                 double porosity, double,
                                                 TangentModulusTensor& Cep)
{
  std::ostringstream out;
  out << "**ERROR** computeElasPlasTangentModulus with a Matrix3 argument "
         "should not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);
  return;
}

void
YieldCond_Tabular::computeTangentModulus(const TangentModulusTensor& Ce,
                                         const Matrix3& f_sigma, double f_q1,
                                         double h_q1, TangentModulusTensor& Cep)
{
  std::ostringstream out;
  out << "**ERROR** coputeTangentModulus with a Matrix3 argument should not be "
      << "called by models that use the Tabular yield criterion.";
  throw InternalError(out.str(), __FILE__, __LINE__);
  return;
}

namespace Vaango {

std::ostream&
operator<<(std::ostream& out, const YieldCond_Tabular& yc)
{
  DoubleVec1D pvals, qvals;
  try {
    pvals = yc.d_yield.table.getIndependentVarData("Pressure", 
                                                   IndexKey(0,0,0,0));
  } catch (Uintah::InvalidValue e) {
    std::cout << e.message() << std::endl;
  }

  try {
    qvals = yc.d_yield.table.getDependentVarData("SqrtJ2", 
                                                 IndexKey(0,0,0,0));
  } catch (Uintah::InvalidValue e) {
    std::cout << e.message() << std::endl;
  }

  out << "p:";
  std::copy(pvals.begin(), pvals.end(),
            std::ostream_iterator<double>(out, " "));
  out << std::endl;
  out << "sqrtJ2:";
  std::copy(qvals.begin(), qvals.end(),
            std::ostream_iterator<double>(out, " "));
  out << std::endl;

  out << "I1_bar_min = " << yc.d_I1bar_min
      << " I1_bar_max = " << yc.d_I1bar_max
      << " sqrtJ2_max = " << yc.d_sqrtJ2_max << std::endl;

  return out;
}

} // end namespace Vaango