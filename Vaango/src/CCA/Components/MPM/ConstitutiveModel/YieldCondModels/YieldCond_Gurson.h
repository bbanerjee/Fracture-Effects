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

#ifndef __BB_GURSON_YIELD_MODEL_H__
#define __BB_GURSON_YIELD_MODEL_H__

#include <CCA/Components/MPM/ConstitutiveModel/YieldCondModels/YieldCondition.h>
#include <CCA/Components/MPM/ConstitutiveModel/ModelState/ModelStateBase.h>
#include <CCA/Components/MPM/ConstitutiveModel/InternalVarModels/IntVar_Metal.h>
#include <CCA/Components/MPM/ConstitutiveModel/FlowStressModels/FlowStressModel.h>
#include <Core/ProblemSpec/ProblemSpecP.h>

namespace Vaango {

//////////////////////////////////////////////////////////////////////
/*!
  \class  YieldCond_Gurson
  \brief  Gurson-Tvergaard-Needleman Yield Condition.
  \author Biswajit Banerjee \n
  C-SAFE and Department of Mechanical Engineering
  University of Utah
  \warning The stress tensor is the Cauchy stress and not the
  Kirchhoff stress.

  References:

  1) Bernauer, G. and Brocks, W., 2002, Fatigue Fract. Engg. Mater. Struct.,
  25, 363-384.
  2) Ramaswamy, S. and Aravas, N., 1998, Comput. Methods Appl. Mech. Engrg.,
  163, 33-53.

  The yield condition is given by
  \f[
  f(\sigma,k,T) =
  \frac{\sigma_{eq}^2}{\sigma_f^2} +
  2 q_1 \phi_* \cosh \left(q_2 \frac{Tr(\sigma)}{2\sigma_f}\right) -
  (1+q_3 \phi_*^2) = 0
  \f]
  where \f$f(\sigma,k,T)\f$ is the yield condition,
  \f$\sigma\f$ is the Cauchy stress,
  \f$k\f$ is a set of internal variable that evolve with time,
  \f$T\f$ is the temperature,
  \f$\sigma_{eq}\f$ is the von Mises equivalent stress given by
  \f$ \sigma_{eq} = \sqrt{\frac{3}{2}\sigma^{d}:\sigma^{d}}\f$ where
  \f$\sigma^{d}\f$ is the deviatoric part of the Cauchy stress,
  \f$\sigma_{f}\f$ is the flow stress,
  \f$q_1,q_2,q_3\f$ are material constants, and
  \f$\phi_*\f$ is the porosity (damage) function.

  The damage function is given by
  \f$ \phi_* = \phi \f$ for \f$ \phi \le \phi_c \f$,
  \f$ \phi_* = \phi_c + k (\phi - \phi_c) \f$ for \f$ \phi > \phi_c \f$, where
  \f$ k \f$ is constant, and \f$ \phi \f$ is the porosity (void volume
  fraction).
*/
//////////////////////////////////////////////////////////////////////

class YieldCond_Gurson : public YieldCondition
{

public:
  /*! \struct CMData
    \brief Constants needed for GTN model */
  struct CMData
  {
    double q1;  /*< Constant q_1 */
    double q2;  /*< Constant q_2 */
    double q3;  /*< Constant q_3 */
    double k;   /*< Constant k */
    double f_c; /*< Critical void volume fraction */
  };

  /*! Constructor
    Creates a Gurson Yield Function object */
  YieldCond_Gurson(Uintah::ProblemSpecP& ps, 
                   IntVar_Metal* intvar,
                   const Uintah::FlowStressModel* flow);
  YieldCond_Gurson(const YieldCond_Gurson* cm);
  YieldCond_Gurson&
  operator=(const YieldCond_Gurson&) = delete;

  //! Destructor
  virtual ~YieldCond_Gurson() override;

  void
  outputProblemSpec(Uintah::ProblemSpecP& ps) override;

  /*! Get parameters */
  std::map<std::string, double>
  getParameters() const override
  {
    std::map<std::string, double> params;
    params["q1"]  = d_CM.q1;
    params["q2"]  = d_CM.q2;
    params["q3"]  = d_CM.q3;
    params["k"]   = d_CM.k;
    params["f_c"] = d_CM.f_c;
    return params;
  }

  //! Evaluate the yield function.
  double
  evalYieldCondition(const Uintah::Matrix3& xi,
                     const ModelStateBase* state) override;

  std::pair<double, Util::YieldStatus>
  evalYieldCondition(const ModelStateBase* state) override;

  double
  evalYieldCondition(const double equivStress,
                     const double flowStress,
                     const double traceOfCauchyStress,
                     const double porosity,
                     double& sig) override;

  double
  evalYieldConditionMax(const ModelStateBase* state) override;

  /////////////////////////////////////////////////////////////////////////
  /*!
    \brief Evaluate the derivative of the yield function \f$(f)\f$
    with respect to \f$\sigma_{ij}\f$.

    This is for the associated flow rule.
  */
  /////////////////////////////////////////////////////////////////////////
  Uintah::Matrix3
  df_dsigma(const Uintah::Matrix3& stress,
            const double flowStress,
            const double porosity) override;

  /////////////////////////////////////////////////////////////////////////
  /*!
    \brief Evaluate the derivative of the yield function \f$(f)\f$
    with respect to \f$s_{ij}\f$.

    This is for the associated flow rule with \f$s_{ij}\f$ being
    the deviatoric stress.
  */
  /////////////////////////////////////////////////////////////////////////
  Uintah::Matrix3
  df_dsigmaDev(const Uintah::Matrix3& stress,
               const double flowStress,
               const double porosity) override;

  /*! Derivative with respect to the Cauchy stress (\f$\sigma \f$)*/
  Uintah::Matrix3
  df_dsigma(const Uintah::Matrix3& xi,
            const ModelStateBase* state) override;

  /*! Derivative with respect to the \f$xi\f$ where \f$\xi = s - \beta \f$
      where \f$s\f$ is deviatoric part of Cauchy stress and
      \f$\beta\f$ is the backstress */
  Uintah::Matrix3
  df_dxi(const Uintah::Matrix3& xi,
         const ModelStateBase* state) override;

  /* Derivative with respect to \f$ s \f$ and \f$ \beta \f$ */
  std::pair<Uintah::Matrix3, Uintah::Matrix3>
  df_dsigmaDev_dbeta(const Uintah::Matrix3& xi,
                     const ModelStateBase* state) override;

  /////////////////////////////////////////////////////////////////////////
  /*!
    \brief Evaluate the derivative of the yield function \f$ f \f$
    with respect to a scalar variable.

    \f[
    f := \sigma^2_{eq} -
    (A \cosh(\frac{B}{\sigma_Y(v_i)}) - C) \sigma_Y^2(v_i)
    \f]
    Therefore,
    \f[
    \frac{df}{dv_i} := -A \sinh(\frac{B}{\sigma_Y}) B
    \frac{d\sigma_Y}{dv_i} +
    2 (A \cosh(\frac{B}{\sigma_Y}) - C) \sigma_Y
    \frac{d\sigma_Y}{dv_i}
    \f]

    \return derivative
  */
  /////////////////////////////////////////////////////////////////////////
  double
  evalDerivativeWRTPlasticityScalar(double trSig,
                                    double porosity,
                                    double sigY,
                                    double dsigYdV);

  /////////////////////////////////////////////////////////////////////////
  /*!
    \brief Evaluate the derivative of the yield function \f$ f \f$
    with respect to the porosity

    \f[
    \frac{df}{dphi} := \left[ 2 q_1
    cosh\left(\frac{q_2 Tr(\sigma)}{2 \sigma_Y}\right)
    - 2 q_3 \phi^* \right] \sigma_Y^2
    \f]

    \return derivative
  */
  /////////////////////////////////////////////////////////////////////////
  double
  evalDerivativeWRTPorosity(double trSig, double porosity, double sigY);

  /////////////////////////////////////////////////////////////////////////
  /*!
    \brief Evaluate the factor \f$h_1\f$ for porosity

    \f[
    h_1 = (1-\phi) Tr(\sigma) + A \frac{\sigma : f_{\sigma}}{(1-\phi) \sigma_Y}
    \f]

    \return factor
  */
  /////////////////////////////////////////////////////////////////////////
  double
  computePorosityFactor_h1(double sigma_f_sigma,
                           double tr_f_sigma,
                           double porosity,
                           double sigma_Y,
                           double A);

  /////////////////////////////////////////////////////////////////////////
  /*!
    \brief Evaluate the factor \f$h_2\f$ for plastic strain

    \f[
    h_2 = \frac{\sigma : f_{\sigma}}{(1-\phi) \sigma_Y}
    \f]

    \return factor
  */
  /////////////////////////////////////////////////////////////////////////
  double
  computePlasticStrainFactor_h2(double sigma_f_sigma,
                                double porosity,
                                double sigma_Y);

  /*! Compute h_alpha  where \f$d/dt(ep) = d/dt(gamma)~h_{\alpha}\f$ */
  double
  eval_h_alpha(const Uintah::Matrix3& xi, const ModelStateBase* state) override;

  /*! Compute h_phi  where \f$d/dt(phi) = d/dt(gamma)~h_{\phi}\f$ */
  double
  eval_h_phi(const Uintah::Matrix3& xi,
             const double& factorA,
             const ModelStateBase* state) override;

  /////////////////////////////////////////////////////////////////////////
  /*!
    \brief Compute the continuum elasto-plastic tangent modulus
    assuming associated flow rule.

    \f[
    C_{ep} = C_{e} - \frac{(C_e:f_{\sigma})\otimes(f_{\sigma}:C_e)}
    {-f_q.h_q + f_{\sigma}:C_e:f_{\sigma}}
    \f]

    \return TangentModulusTensor \f$ C_{ep} \f$.
  */
  /////////////////////////////////////////////////////////////////////////
  void
  computeTangentModulus(const Uintah::TangentModulusTensor& Ce,
                        const Uintah::Matrix3& f_sigma,
                        double f_q1,
                        double f_q2,
                        double h_q1,
                        double h_q2,
                        Uintah::TangentModulusTensor& Cep);

  /////////////////////////////////////////////////////////////////////////
  /*!
    \brief Compute the elastic-plastic tangent modulus.
  */
  /////////////////////////////////////////////////////////////////////////
  void
  computeElasPlasTangentModulus(const Uintah::TangentModulusTensor& Ce,
                                const Uintah::Matrix3& sigma,
                                double sigY,
                                double dsigYdV,
                                double porosity,
                                double voidNuclFac,
                                Uintah::TangentModulusTensor& Cep) override;

  //--------------------------------------------------------------
  // Compute df/dp  where p = volumetric stress = 1/3 Tr(sigma)
  //--------------------------------------------------------------
  double
  df_dp(const ModelStateBase* state) override
  {
    return 0.0;
  };

  //--------------------------------------------------------------
  // Compute df/dq  where q = sqrt(3 J_2), J_2 = 2nd invariant deviatoric stress
  //--------------------------------------------------------------
  double
  df_dq(const ModelStateBase* state) override
  {
    return 0.0;
  };

  /*! Derivative with respect to internal variables */
  void
  df_dintvar(const ModelStateBase* state,
             MetalIntVar& df_dintvar) const override;

  //--------------------------------------------------------------
  // Compute d/depse_v(df/dp)
  //--------------------------------------------------------------
  double
  d2f_dp_depsVol(const ModelStateBase* state,
                 const MPMEquationOfState* eos,
                 const ShearModulusModel* shear) override
  {
    return 0.0;
  };

  //--------------------------------------------------------------
  // Compute d/depse_s(df/dp)
  //--------------------------------------------------------------
  double
  d2f_dp_depsDev(const ModelStateBase* state,
                 const MPMEquationOfState* eos,
                 const ShearModulusModel* shear) override
  {
    return 0.0;
  };

  //--------------------------------------------------------------
  // Compute d/depse_v(df/dq)
  //--------------------------------------------------------------
  double
  d2f_dq_depsVol(const ModelStateBase* state,
                 const MPMEquationOfState* eos,
                 const ShearModulusModel* shear) override
  {
    return 0.0;
  };

  //--------------------------------------------------------------
  // Compute d/depse_s(df/dq)
  //--------------------------------------------------------------
  double
  d2f_dq_depsDev(const ModelStateBase* state,
                 const MPMEquationOfState* eos,
                 const ShearModulusModel* shear) override
  {
    return 0.0;
  };

  //--------------------------------------------------------------
  // Compute df/depse_v
  //--------------------------------------------------------------
  double
  df_depsVol(const ModelStateBase* state,
             const MPMEquationOfState* eos,
             const ShearModulusModel* shear) override
  {
    return 0.0;
  };

  //--------------------------------------------------------------
  // Compute df/depse_s
  //--------------------------------------------------------------
  double
  df_depsDev(const ModelStateBase* state,
             const MPMEquationOfState* eos,
             const ShearModulusModel* shear) override
  {
    return 0.0;
  };

  double
  getInternalPoint(const ModelStateBase* state_old,
                   const ModelStateBase* state_new) override
  {
    return 0.0;
  }

private:
  CMData d_CM;
  IntVar_Metal* d_intvar;
  const Uintah::FlowStressModel* d_flow;

  /*! Derivative with respect to the plastic strain (\f$\epsilon^p \f$)*/
  double
  df_dplasticStrain(const ModelStateBase* state) const;

  /*! Derivative with respect to the porosity (\f$\phi\f$)*/
  double
  df_dporosity(const ModelStateBase* state) const;

};

} // End namespace Uintah

#endif // __BB_GURSON_YIELD_MODEL_H__
