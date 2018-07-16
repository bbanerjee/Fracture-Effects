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

#include <CCA/Components/MPM/ConstitutiveModel/Models/ElasticModuliModel.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/InternalVar_Arena.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/InternalVar_BorjaPressure.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/InternalVar_SoilModelBrannonKappa.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/InternalVar_TabularCap.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/InternalVariableModelFactory.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ShearModulusModel.h>

#include <Core/Exceptions/ProblemSetupException.h>
#include <Core/Malloc/Allocator.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <fstream>
#include <iostream>
#include <string>

using namespace Vaango;
using Uintah::ProblemSpecP;
using Uintah::ProblemSetupException;
using std::cerr;
using std::ifstream;
using std::ofstream;

InternalVariableModel*
InternalVariableModelFactory::create(ProblemSpecP& ps)
{
  ProblemSpecP child = ps->findBlock("internal_variable_model");
  if (!child)
    throw ProblemSetupException("Cannot find internal_var_model tag", __FILE__,
                                __LINE__);
  std::string mat_type;
  if (!child->getAttribute("type", mat_type))
    throw ProblemSetupException("No type for internal_var_model", __FILE__,
                                __LINE__);
  if (mat_type == "soil_model_brannon_kappa") {
    return (scinew InternalVar_SoilModelBrannonKappa(child));
  } else if (mat_type == "tabular_cap") {
    return (scinew InternalVar_TabularCap(child));
  } else {
    throw ProblemSetupException(
      "Unknown InternalVariable Model (" + mat_type + ")", __FILE__, __LINE__);
  }
}

InternalVariableModel*
InternalVariableModelFactory::create(ProblemSpecP& ps, ShearModulusModel* shear)
{
  ProblemSpecP child = ps->findBlock("internal_variable_model");
  if (!child)
    throw ProblemSetupException("Cannot find internal_var_model tag", __FILE__,
                                __LINE__);
  std::string mat_type;
  if (!child->getAttribute("type", mat_type))
    throw ProblemSetupException("No type for internal_var_model", __FILE__,
                                __LINE__);
  if (mat_type == "borja_consolidation_pressure")
    return (scinew InternalVar_BorjaPressure(child, shear));
  else {
    throw ProblemSetupException(
      "Unknown InternalVariable Model (" + mat_type + ")", __FILE__, __LINE__);
  }
}

InternalVariableModel*
InternalVariableModelFactory::create(ProblemSpecP& ps,
                                     ElasticModuliModel* elastic)
{
  ProblemSpecP child = ps->findBlock("internal_variable_model");
  if (!child)
    throw ProblemSetupException("Cannot find internal_var_model tag", __FILE__,
                                __LINE__);
  std::string mat_type;
  if (!child->getAttribute("type", mat_type))
    throw ProblemSetupException("No type for internal_var_model", __FILE__,
                                __LINE__);
  if (mat_type == "arena")
    return (scinew InternalVar_Arena(child, elastic));
  else {
    throw ProblemSetupException(
      "Unknown InternalVariable Model (" + mat_type + ")", __FILE__, __LINE__);
  }
}

InternalVariableModel*
InternalVariableModelFactory::createCopy(const InternalVariableModel* pm)
{
  if (dynamic_cast<const InternalVar_BorjaPressure*>(pm))
    return (scinew InternalVar_BorjaPressure(
      dynamic_cast<const InternalVar_BorjaPressure*>(pm)));

  else if (dynamic_cast<const InternalVar_SoilModelBrannonKappa*>(pm))
    return (scinew InternalVar_SoilModelBrannonKappa(
      dynamic_cast<const InternalVar_SoilModelBrannonKappa*>(pm)));

  else if (dynamic_cast<const InternalVar_Arena*>(pm))
    return (
      scinew InternalVar_Arena(dynamic_cast<const InternalVar_Arena*>(pm)));

  else if (dynamic_cast<const InternalVar_TabularCap*>(pm))
    return (
      scinew InternalVar_TabularCap(dynamic_cast<const InternalVar_TabularCap*>(pm)));

  else {
    throw Uintah::ProblemSetupException(
      "Cannot create copy of unknown internal var model", __FILE__, __LINE__);
  }
}
