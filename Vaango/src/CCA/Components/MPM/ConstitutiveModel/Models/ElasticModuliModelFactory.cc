/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
 * Copyright (c) 2013-2014 Callaghan Innovation, New Zealand
 * Copyright (c) 2015 Parresia Research Limited, New Zealand
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

#include <CCA/Components/MPM/ConstitutiveModel/Models/ElasticModuliModelFactory.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ElasticModuli_Arena.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ElasticModuli_ArenaMixture.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ElasticModuli_Arenisca.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ElasticModuli_Constant.h>
#include <Core/Exceptions/ProblemSetupException.h>
#include <Core/Malloc/Allocator.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <string>

using namespace Vaango;

ElasticModuliModel*
ElasticModuliModelFactory::create(Uintah::ProblemSpecP& ps)
{
  Uintah::ProblemSpecP child = ps->findBlock("elastic_moduli_model");
  if (!child) {
    std::ostringstream out;
    out << "**Error** No Elastic modulus model provided."
        << " Default (constant elasticity) model needs at least two input "
           "parameters."
        << std::endl;
    throw Uintah::ProblemSetupException(out.str(), __FILE__, __LINE__);
  }

  std::string mat_type;
  if (!child->getAttribute("type", mat_type)) {
    std::ostringstream out;
    out << "MPM::ConstitutiveModel:No type provided for elasticity model.";
    throw Uintah::ProblemSetupException(out.str(), __FILE__, __LINE__);
  }

  if (mat_type == "constant")
    return (scinew ElasticModuli_Constant(child));
  else if (mat_type == "arenisca")
    return (scinew ElasticModuli_Arenisca(child));
  else if (mat_type == "arena")
    return (scinew ElasticModuli_Arena(child));
  else if (mat_type == "arena_mixture")
    return (scinew ElasticModuli_ArenaMixture(child));
  else {
    std::cerr << "**WARNING** No elasticity model provided. "
              << "Creating default (constant elasticity) model" << std::endl;
    return (scinew ElasticModuli_Constant(child));
  }
}

ElasticModuliModel*
ElasticModuliModelFactory::createCopy(const ElasticModuliModel* smm)
{
  if (dynamic_cast<const ElasticModuli_Constant*>(smm))
    return (scinew ElasticModuli_Constant(
      dynamic_cast<const ElasticModuli_Constant*>(smm)));
  else if (dynamic_cast<const ElasticModuli_Arenisca*>(smm))
    return (scinew ElasticModuli_Arenisca(
      dynamic_cast<const ElasticModuli_Arenisca*>(smm)));
  else if (dynamic_cast<const ElasticModuli_Arena*>(smm))
    return (scinew ElasticModuli_Arena(
      dynamic_cast<const ElasticModuli_Arena*>(smm)));
  else if (dynamic_cast<const ElasticModuli_ArenaMixture*>(smm))
    return (scinew ElasticModuli_ArenaMixture(
      dynamic_cast<const ElasticModuli_ArenaMixture*>(smm)));
  else {
    std::cerr << "**WARNING** No elasticity model provided. "
              << "Creating default (constant elasticity) model" << std::endl;
    return (scinew ElasticModuli_Constant(
      dynamic_cast<const ElasticModuli_Constant*>(smm)));
  }
}
