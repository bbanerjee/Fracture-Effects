/*
 * The MIT License
 *
 * Copyright (c) 2013-2014 Callaghan Innovation, New Zealand
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

#include <CCA/Components/Peridynamics/FailureModels/PeridynamicsFailureModelFactory.h>

#include <CCA/Components/Peridynamics/FailureModels/BondDamageModel.h>
#include <CCA/Components/Peridynamics/PeridynamicsFlags.h>

#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Exceptions/ProblemSetupException.h>

#include <fstream>
#include <iostream>
#include <string>

using namespace Vaango;

PeridynamicsFailureModel* 
PeridynamicsFailureModelFactory::create(Uintah::ProblemSpecP& ps,
                                        PeridynamicsFlags* flags)
{
  Uintah::ProblemSpecP child = ps->findBlock("failure_model");
  if(!child)
    throw Uintah::ProblemSetupException("Cannot find failure_model tag", __FILE__, __LINE__);
  std::string mat_type;
  if(!child->getAttribute("type", mat_type))
    throw Uintah::ProblemSetupException("No type for failure_model", __FILE__, __LINE__);
   
  if (mat_type == "bond_damage")
    return(scinew BondDamageModel(child, flags));
  else 
    throw Uintah::ProblemSetupException("Unknown peridynamic failure model type ("+mat_type+")", __FILE__, __LINE__);

  return 0;
}
