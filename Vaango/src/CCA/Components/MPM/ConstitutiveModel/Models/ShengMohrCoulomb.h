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

#ifndef __MPM_CONSTITUTIVEMODEL_MODELS_SHENG_MOHRCOULOMB__
#define __MPM_CONSTITUTIVEMODEL_MODELS_SHENG_MOHRCOULOMB__

#include "StateMohrCoulomb.h"

#include <cmath>

/* this Mohr-Coulomb like model uses a rounded Mohr-Coulomb surface, see
   eq 13 in Sheng D, Sloan SW & Yu HS Computational Mechanics 26:185-196 (2000)
   Springer
*/

namespace Uintah {


class ShengMohrCoulomb {

public:

  // used to check for accuracies below machine accuracy, and to accept anything
  // less as ok
  constexpr double TINY = 1.0e-14;

  enum class DriftCorrection
  {
    NO_CORRECTION = 1,
    CORRECTION_AT_BEGIN = 2;
    CORRECTION_AT_END = 3;
  };

  enum class ToleranceMethod
  {
    EPUS_RELATIVE_ERROR = 0,
    SLOAN = 1
  };

  enum class SolutionAlgorithm
  {
    RUNGE_KUTTA_SECOND_ORDER_MODIFIED_EULER = 1,
    RUNGE_KUTTA_THIRD_ORDER_NYSTROM = 2,
    RUNGE_KUTTA_THIRD_ORDER_BOGACKI = 3,
    RUNGE_KUTTA_FOURTH_ORDER = 4,
    RUNGE_KUTTA_FIFTH_ORDER_ENGLAND = 5,
    RUNGE_KUTTA_FIFTH_ORDER_CASH = 6,
    RUNGE_KUTTA_FIFTH_ORDER_DORMAND = 7,
    RUNGE_KUTTA_FIFTH_ORDER_BOGACKI = 8,
    EXTRAPOLATION_BULIRSCH = 9
  };

  ShengMohrCoulomb();
  ShengMohrCoulomb(double G, double K, double cohesion, double phi, double psi);

  ShengMohrCoulomb(const ShengMohrCoulomb& ) = delete;
  ShengMohrCoulomb& operator=(const ShengMohrCoulomb& ) = delete;
  ~ShengMohrCoulomb() = default;

  void setModelParameters(double G, double K, double cohesion, double phi, 
                          double psi);

  void setIntegrationParameters(int maxIterPegasus,
                                double integrationTolerance, 
                                double betaFactor, 
                                double yieldLocTolerance,
                                SolutionAlgorithm solutionAlgorithm,
                                ToleranceMethod toleranceMethod,
                                DriftCorrection driftCorrection);

  void integrate(const Vector6& strainIncrement, 
                 const StateMohrCoulomb& initialState);


  void integrate(double *strainIncrement, double suctionIncrement,
                 StateMohrCoulomb *initialPoint, double *stressIncrement,
                 double P0StarIncrement, double *plasticStrainIncrement);
  void integrateConst(double *strainIncrement, StateMohrCoulomb *initialPoint,
                      int stepNo, int method);
  void findElStrGradPQ(double nu0, double *s0, double *eps0, double *deps,
                       double *ds);
  void findElStrGrad(double nu0, double *s0, double *eps0, double *deps,
                     double *ds);
  double calculatepZero(StateMohrCoulomb *state);
  bool checkYield(StateMohrCoulomb *state);
  double computeYieldFunction(StateMohrCoulomb *state);
  double computeYieldFunctionNN(StateMohrCoulomb *state);
  void findYieldOriginal(double *state, double *s0, double *eps0, double *deps,
                         double *a);
  double findGradientPQ(StateMohrCoulomb *state, double *ds, double *dF,
                        double dsuction);
  void moveYieldaBit(double *state, double *s, double *ds, double *eps0,
                     double *deps, double *gradient, double F0);
  void paintLocus(double *state, double suction, int Max);
  void computeG1(StateMohrCoulomb *initialPoint, int retentionModel,
                 double *retentionParameters, double *G1);
  void computeG2(StateMohrCoulomb *initialPoint, int retentionModel,
                 double *retentionParameters, double *G2);

  double getk();
  double getLambdaZero();
  double getr();
  double getBeta();
  double getKappaP();
  double getpc();
  void read();
  void write();

  // *********************************** Plastic Procedures below
  // ******************************************************

  void getTangentMatrixPQ(StateMohrCoulomb *state, BBMMatrix *DEP);
  void calculateElastoPlasticTangentMatrixPQ(StateMohrCoulomb *state, BBMMatrix *DEP);
  void calculateElasticTangentMatrixPQ(StateMohrCoulomb *state, BBMMatrix *DEP);
  void getTangentMatrix(StateMohrCoulomb *state, BBMMatrix *DEP);
  void calculateElastoPlasticTangentMatrix(StateMohrCoulomb *state, BBMMatrix *DEP);
  void getDerivative(double meanStress, double shearStress, double suction,
                     double pZero, double *state, double *deriv);
  double getLambda(double *deriv, double stresspq[3], double strainpq[3]);
  double
  plasticEuler(StateMohrCoulomb *state, double *epStrain, double *absStress,
               int numberIterations); // returns elapsed time of computations
  double doRungeKutta(double A[][8], double *B, double *BRes, double *C,
                    StateMohrCoulomb *state, double *epStrain, double *absStress,
                    int *numberIter, double methodOrder, int methodSteps,
                    bool errorEstimate);
  double doRungeKuttaEqualStep(double A[][8], double *B, double *BRes, double *C,
                             StateMohrCoulomb *state, double *epStrain,
                             double *absStress, double *RelError,
                             int numberIter, double methodOrder,
                             int methodSteps, bool errorEstimate);
  double doRungeKuttaExtrapol(double A[][8], double *B, double *BRes, double *C,
                            StateMohrCoulomb *state, double *epStrain,
                            double *absStress, int *numberIter,
                            double methodOrder, int methodSteps,
                            bool errorEstimate);
  // Runge Kutta schemes
  double calculatePlasticConst(double *purelyPlasticStrain, StateMohrCoulomb *state,
                               int stepNo);
  double plasticRKErr8544(StateMohrCoulomb *state, double *epStrain, double *absStress,
                          int *numberIter); // Bogacki - Shimpine
  double plasticRKDP754(StateMohrCoulomb *state, double *epStrain, double *absStress,
                        int *numberIter); // Dormand Prince
  double plasticRKCK654(StateMohrCoulomb *state, double *epStrain, double *absStress,
                        int *numberIter); // Cash - Karp
  double plasticRKEng654(StateMohrCoulomb *state, double *epStrain, double *absStress,
                         int *numberIter); // England as given by Sloan
  double plasticRK543(StateMohrCoulomb *state, double *epStrain, double *absStress,
                      int *numberIter); // 4th order with 3rd ord estimate
  double plasticRK332(StateMohrCoulomb *state, double *epStrain, double *absStress,
                      int *numberIter); // 3rd order R-K scheme
  double plasticRKBog432(
      StateMohrCoulomb *state, double *epStrain, double *absStress,
      int *numberIter); // Bogacki - Shimpine 3rd order Runge Kutta scheme
  double plasticRKME221(StateMohrCoulomb *state, double *epStrain, double *absStress,
                        int *numberIter); // Modified Euler
  double plasticRKNoExTry(StateMohrCoulomb *state, double *epStrain, double *absStress,
                          int *numberIter); // using not in an extrapolation way
  // Extrapolation Schemes
  double plasticExtrapol(StateMohrCoulomb *state, double *epStrain, double *absStress,
                         int *numberIter);
  double doRKExtrapolation(double A[][8], double *B, double *BRes, double *C,
                         StateMohrCoulomb *state, double *epStrain, double *absStress,
                         StateMohrCoulomb *OldPoint, double *RelError, int *numberIter,
                         double methodOrder, int methodSteps,
                         bool errorEstimate);
  double plasticMidpoint(StateMohrCoulomb *state, double *epStrain, double *absStress,
                         int *numberIter);

  // Used Procedures before, not updated anymore, though, mostly, working.
  // void driftCorrect (StateMohrCoulomb state, double* epStrain, BBMMatrix* dSigma,
  // double* Lambda, double* dpZeroStar, double* fValue);
  // void correctDriftBeg (StateMohrCoulomb state, double* epStrain, BBMMatrix* dSigma,
  // double* Lambda, double* dpZeroStar, double* fValue);
  // double Plastic (StateMohrCoulomb* state, double* epStrain, double* absStress, int*
  // numberIter);		//returns elapsed time of computations
  // double PlasticNewSlow (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIterations);	//returns elapsed time of computations
  // double plasticRKErr6 (StateMohrCoulomb* state, double* epStrain, double* absStress,
  // int* numberIterations);	//returns elapsed time of computations
  // double plasticRKErr75 (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);

  // double plasticRK5Err4_2 (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticRK4Err3  (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticRK4Err3v2  (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticRK3Err2  (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticRK3Err2v2  (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticRK3Err2v3  (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticRKSloan (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIterations);	//returns elapsed time of computations
  // double plasticMidpointC (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticMidpointCN (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticMidpointC4 (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);
  // double plasticMidpointC6 (StateMohrCoulomb* state, double* epStrain, double*
  // absStress, int* numberIter);

private:

  // Elastic parameters
  struct Elastic {
    double d_G;  // Shear modulus
    double d_K;  // Bulk modulus
    double d_E;  // Young's modulus
    double d_nu; // Poisson's ratio

    void set(double G, double K) {
      d_G = G; d_K = K; 
      d_E = 9.0 * K * G / (G + 3.0 * K);
      d_nu = (3.0 * K - 2.0 * G) / (2.0 * G + 6.0 * K);
    }
  };

  // Mohr - Coulomb parameters
  struct Yield {
    double d_cohesion;
    double d_phi;
    double d_sin_phi;
    double d_cos_phi;

    // Rounded Mohr-Coulomb parameters
    double d_alpha;  // For the M parameter
    double d_alpha4; // alpha^4;

    void set(double c, double phi) {
      d_cohesion = c; 
      d_phi = phi * M_PI / 180.0; 
      d_sin_phi = std::sin(phi); d_cos_phi = std::cos(phi);
      d_alpha = (3.0 - d_sin_phi) / (3.0 + d_sin_phi);
      d_alpha4 = alpha * alpha * alpha * alpha;
    }
  };

  // If the flow rule is not associated;
  struct PotentialFn {
    double d_psi;
    double d_sin_psi;
    double d_cos_psi;

    void set(double psi) {
      d_psi = psi * M_PI / 180.0; 
      d_sin_psi = std::sin(psi); d_cos_psi = std::cos(psi);
    }
  };

  // Integration parameters
  struct IntegrationParameters {

    // Elastic parameters, used in the Pegasus algorithm
    int    d_maxIter;
    double d_alfaCheck;
    double d_alfaChange;
    double d_alfaRatio;

    // Yield tolerance
    double d_yieldTol;      // tolerance for the Yield locus (relatively defined, used
                            // to check if the stress state is on the YL)
    double d_integrationTol;
    double d_betaFactor;    // safety factor
    double d_minMeanStress; 
    double d_suctionTol;    // used in checking suction yield locus. advisible not to modify...

    // Algorithms
    DriftCorrection   d_driftCorrection;
    ToleranceMethod   d_tolMethod;
    SolutionAlgorithm d_solutionAlgorithm;

    void setDefaults() {
      d_maxIter    = 200;
      d_alfaCheck  = 1;
      d_alfaChange = 0.05;
      d_alfaRatio  = 10;

      d_yieldTol       = 1e-6; 
      d_integrationTol = 0.01; 
      d_betaFactor     = 0.9;   
      d_minMeanStress  = -1.0e8; 
      d_suctionTol     = 1e-8; 

      d_driftCorrection   = DriftCorrection::CORRECTION_AT_END;
      d_tolMethod         = ToleranceMethod::SLOAN; 
      d_solutionAlgorithm = SolutionAlgorithm::RUNGE_KUTTA_SECOND_ORDER_MODIFIED_EULER;
    }

    void setDefaults(const Yield& yield) {
      setDefaults();
      if (yield.sin_phi > 0) {
        d_minMeanStress = yield.cohesion * yield.cos_phi / yield.sin_phi;
      } 
    }
  }

  // Model parameters
  bool        d_nonAssociated;
  Elastic     d_elastic;
  Yield       d_yield;
  PotentialFn d_potential;

  // Integration parameters
  IntegrationParameters d_integration;

  bool checkYieldNormalized(const StateMohrCoulomb& state) const;

  double computeYieldNormalized(const Vector6& stress) const;

  void calcElastic(const Vector7& strain, const StateMohrCoulomb& initialPoint,
                   StateMohrCoulomb& finalPoint) const;

  Vector6 calcStressIncElast(double nu0, const Vector6& s0, const Vector7& eps0,
                             const Vector7& deps);

  bool checkGradient(const StateMohrCoulomb& initialState, 
                     const StateMohrCoulomb& finalState) const;

  Matrix67 calculateElasticTangentMatrix(const StateMohrCoulomb& state) const;
  Matrix66 calculateElasticTangentMatrix(double K, double G) const;

  double findGradient(const Vector6& s, const Vector6& ds, 
                      Vector6s& dF, double suction, double dsuction) const;

  Vector6 computeDfDsigma(const Vector6& stress) const;

  inline double firstInvariant(const Vector6& s) const {
    double I1 =  s(0) + s(1) + s(2);
    return I1;
  }

  inline double secondInvariant(const Vector6& s) const {
    double I2 = s(0) * s(1) + s(1) * s(2) +
                s(2) * s(0) - s(3) * s(3) -
                s(4) * s(4) - s(5) * s(5);
    return I2;
  }

  inline double thirdInvariant(const Vector6& s) const {
    double I3 =
      s(0) * s(1) * s(2) + 2 * s(3) * s(4) * s(5) -
      s(0) * s(5) * s(5) -     s(1) * s(4) * s(4) -
      s(2) * s(3) * s(3);
    return I3;
  }

  inline double firstDevInvariant(const Vector6& s) const {
    return 0.0;
  }

  inline double secondDevInvariant(const Vector6& s) const {
    double J2 = ((s(0) - s(1)) * (s(0) - s(1)) +
                 (s(0) - s(2)) * (s(0) - s(2)) +
                 (s(1) - s(2)) * (s(1) - s(2))) / 6.0 +
                 (s(3) * s(3) + s(4) * s(4) + s(5) * s(5));
    if (std::abs(J2) < TINY) {
      J2 = TINY;
    }
    return J2;
  }

  inline double thirdDevInvariant(const Vector6& s) const {
    double I1 = firstInvariant(s);
    double I2 = secondInvariant(s);
    double I3 = thirdInvariant(s);
    double J3 = I1 * I1 * I1 * 2.0 / 27.0 - I1 * I2 / 3.0 + I3;

    return J3;
  }

  inline std::tuple<double, double> vonMisesStress(const Vector6& s) const {
    double J2 = secondDevInvariant(s);
    double vmStress = std::sqrt(3.0 * J2);
    if (std::abs(vmStress) < TINY) {
      vmStress = TINY;
    }
    return std::make_tuple(J2, vmStress);
  }

  void findIntersectionUnloading(const Vector7& strainIncrement,
                                 const StateMohrCoulomb& initialState,
                                 Vector7& purelyElastic,
                                 Vector7& purelyPlastic) const;

  void findIntersection(const Vector7& strainIncrement,
                        const StateMohrCoulomb& initialState,
                        Vector7& elasticStrainInc,
                        Vector7& plasticStrainInc);

  double findYieldAlpha(const Vector3& state, 
                        const Vector6& s0, 
                        const Vector7& eps0,
                        const Vector7& deps) const;

  double findYieldModified(const Vector3& state, 
                           const Vector6& s0, 
                           const Vector7& eps0,
                           const Vector7& deps) const;

  double computeNu(const Matrix6& s, const Matrix3& state, double suction) const;

  double calculatePlastic(const Vector7& purelyPlasticStrain, 
                          const StateMohrCoulomb& state) const;

  int calcPlastic(const StateMohrCoulomb& state, const Vector7& epStrainInc,
                  Vector6& dSigma, Vector& dEps_p, double& dP0Star) const;

  std::tuple<double, int> plasticRKME221(StateMohrCoulomb& state, 
                                         const Vector7& epStrain) const;

  std::tuple<double, int> plasticRK332(StateMohrCoulomb& state,
                                       const Vector7& epStrain) const;

  std::tuple<double, int> plasticRKBog432(StateMohrCoulomb& point, 
                                          const Vector7& epStrain) const;

  std::tuple<double, int> plasticRK543(StateMohrCoulomb& state,
                                       const Vector7& epStrain) const;

  std::tuple<double, int> plasticRKEng654(StateMohrCoulomb& state,
                                          const Vector7& epStrain) const;

  std::tuple<double, int> plasticRKCK654(StateMohrCoulomb& state,
                                         const Vector7& epStrain) const;

  std::tuple<double, int> plasticRKDP754(StateMohrCoulomb& state,
                                         const Vector7& epStrain) const;

  std::tuple<double, int> plasticRKErr8544(StateMohrCoulomb& state,
                                           const Vector7& epStrain) const;

  template<int Order, int Steps>
  std::tuple<double, int> doRungeKutta(const Eigen::Matrix<double, Steps, Steps>& AA, 
                                       const Eigen::Matrix<double, Steps, 1>&     BB, 
                                       const Eigen::Matrix<double, Steps, 1>&     BRes, 
                                       const Eigen::Matrix<double, Steps, 1>&     CC,
                                       StateMohrCoulomb&       state, 
                                       const Vector7&          epStrain,
                                       bool                    errorEstimate) const;

  template<int Order, int Steps>
  std::tuple<double, int> doRungeKuttaErr(const Eigen::Matrix<double, Steps, Steps>& AA, 
                                          const Eigen::Matrix<double, Steps, 1>&     BB, 
                                          const Eigen::Matrix<double, Steps, 1>&     BRes, 
                                          const Eigen::Matrix<double, Steps, 1>&     CC,
                                          const Eigen::Matrix<double, Steps - 1, 1>& ErrCoef,
                                          StateMohrCoulomb&       state, 
                                          const Vector7&          epStrain,
                                          bool                    errorEstimate) const;

  double checkNorm(const Vector7& dSigma, double dP0Star,
                   const StateMohrCoulomb& initialState,
                   const Vector7& dError) const;

  double checkNormSloan(const Vector7& dSigma, double dP0Star,
                        const StateMohrCoulomb& initialState,
                        const Vector7& dError) const;

  void correctDriftBeg(StateMohrCoulomb& state, 
                       const StateMohrCoulomb* stateOld) const;

  void correctDriftEnd(StateMohrCoulomb& state) const;

  double plasticMidpoint(StateMohrCoulomb& state, 
                              const Vector7& epStrain,
                              Vector7& absStress,
                              int numIter);

  std::tuple<double, int> plasticExtrapol(StateMohrCoulomb& state,
                                          const Vector7& epStrain) const;
};

} // end namespace Uintah

#endif //__MPM_CONSTITUTIVEMODEL_MODELS_SHENG_MOHRCOULOMB__
