/*
 * The MIT License
 *
 * Copyright (c) 2015-2018 Parresia Research Limited, New Zealand
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

#ifndef ___ELASTIC_MODULI_NEURAL_NET_MODEL_H__
#define ___ELASTIC_MODULI_NEURAL_NET_MODEL_H__

#include <CCA/Components/MPM/ConstitutiveModel/ElasticModuliModels/ElasticModuliModel.h>
#include <CCA/Components/MPM/ConstitutiveModel/ModelState/ModelStateBase.h>
#include <Core/ProblemSpec/ProblemSpecP.h>

#include <Eigen/Core>
#include <submodules/json/src/json.hpp>
#include "H5Cpp.h"

#include <limits>

namespace Vaango {

/*! \class ElasticModuli_NeuralNet
 *  \brief A fully connected multilayer perceptron neural net elasticity model
 *  \author Biswajit Banerjee,
 */
class ElasticModuli_NeuralNet : public ElasticModuliModel
{

public:

  ElasticModuli_NeuralNet() = delete;
  ElasticModuli_NeuralNet(const ElasticModuli_NeuralNet& smm) = delete;
  ~ElasticModuli_NeuralNet() = default;

  ElasticModuli_NeuralNet(Uintah::ProblemSpecP& ps);
  ElasticModuli_NeuralNet(const ElasticModuli_NeuralNet* smm);
  ElasticModuli_NeuralNet& operator=(const ElasticModuli_NeuralNet& smm) = delete;

  void outputProblemSpec(Uintah::ProblemSpecP& ps) override;

  /*! Get parameters */
  std::map<std::string, double> getParameters() const override
  {
    std::map<std::string, double> params;
    params["G0"] = d_shear.G0;
    params["nu"] = d_shear.nu;
    return params;
  }

  /*! Compute the elasticity */
  ElasticModuli getInitialElasticModuli() const override;
  ElasticModuli getCurrentElasticModuli(const ModelStateBase* state) override;

  ElasticModuli getElasticModuliLowerBound() const override
  {
    return getInitialElasticModuli();
  }
  ElasticModuli getElasticModuliUpperBound() const override
  {
    return ElasticModuli(std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::max());
  }

  /* Get the elastic moduli and their derivatives with respect to a single
     plastic internal variable */
  std::pair<ElasticModuli, ElasticModuli>
    getElasticModuliAndDerivatives(const ModelStateBase* state_input) const override;

private:

  template <typename T>
  struct NeuralNetworkLayer {
    using EigenMatrixRowMajor = 
      Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    std::string name;
    std::string activation;
    int units;
    int input_size;
    EigenMatrixRowMajor weights;
    EigenMatrixRowMajor bias; 
  };

  template <typename T>
  struct NeuralNetworkModel {
    std::vector<NeuralNetworkLayer<T>> d_layers;
    double d_minStrain;
    double d_maxStrain;
    double d_minPressure;
    double d_maxPressure;

    int inputLayerSize() const { return d_layers[0].input_size; } 
    void readNeuralNetworkHDF5(const std::string& filename);
    double predict(double totalVolStrain, double plasticVolStrain) const;
  };

  /* Tangent bulk modulus parameters */
  template <typename T>
  struct BulkModulusParameters
  {
    std::string d_filename;
    NeuralNetworkModel<T> d_model;

    BulkModulusParameters() = default;
    BulkModulusParameters(Uintah::ProblemSpecP& ps) {
      ps->require("filename", d_filename);
      ps->require("min_strain", d_model.d_minStrain);
      ps->require("max_strain", d_model.d_maxStrain);
      ps->require("min_pressure", d_model.d_minPressure);
      ps->require("max_pressure", d_model.d_maxPressure);
      d_model.readNeuralNetworkHDF5(d_filename);
      std::cout << "Completed reading model\n";
    }

    BulkModulusParameters(const BulkModulusParameters& bulk) {
      d_filename = bulk.d_filename;
      d_model = bulk.d_model;
    }

    BulkModulusParameters&
    operator=(const BulkModulusParameters& bulk) {
      if (this != &bulk) {
        d_filename = bulk.d_filename;
        d_model = bulk.d_model;
      }
      return *this;
    }

    void
    outputProblemSpec(Uintah::ProblemSpecP& ps) {
      ps->appendElement("filename", d_filename);
      ps->appendElement("min_strain", d_model.d_minStrain);
      ps->appendElement("max_strain", d_model.d_maxStrain);
      ps->appendElement("min_pressure", d_model.d_minPressure);
      ps->appendElement("max_pressure", d_model.d_maxPressure);
    }
  };

  /* Tangent shear modulus parameters */
  struct ShearModulusParameters
  {
    double G0;
    double nu;
  };

  BulkModulusParameters<double> d_bulk;
  ShearModulusParameters d_shear;

  void checkInputParameters();

  double computeBulkModulus(const double& elasticStrain,
                            const double& plasticStrain) const;
  double computeShearModulus(const double& bulkModulus) const;

};
} // End namespace Vaango

#endif // __ELASTIC_MODULI_NEURAL_NET_MODEL_H__