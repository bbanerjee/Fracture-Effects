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


#include <CCA/Components/MPM/ConstitutiveModel/Models/ShearModulusModelFactory.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ShearModulus_Constant.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ShearModulus_Nadal.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ShearModulus_Borja.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/PressureModel.h>
#include <Core/Exceptions/ProblemSetupException.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Malloc/Allocator.h>
#include <string>

using namespace std;
using namespace Uintah;
using namespace Vaango;

ShearModulusModel* ShearModulusModelFactory::create(Uintah::ProblemSpecP& ps,
                                                    PressureModel* eos)
{
   ProblemSpecP child = ps->findBlock("elastic_shear_modulus_model");
   if(!child) {
      cerr << "**WARNING** Attempting to create default (constant shear modulus) model" << endl;
      return(scinew ShearModulus_Constant(ps, eos));
   }
   string mat_type;
   if(!child->getAttribute("type", mat_type))
      throw ProblemSetupException("MPM::ConstitutiveModel:No type for shear modulus model.",
                                  __FILE__, __LINE__);
   
   if (mat_type == "constant_shear")
      return(scinew ShearModulus_Constant(child, eos));
   else if (mat_type == "np_shear")
      return(scinew ShearModulus_Nadal(child, eos));
   else if (mat_type == "borja_shear_modulus")
      return(scinew ShearModulus_Borja(child, eos));
   else {
      cerr << "**WARNING** Creating default (constant shear modulus) model" << endl;
      return(scinew ShearModulus_Constant(child, eos));
   }
}

ShearModulusModel* 
ShearModulusModelFactory::createCopy(const ShearModulusModel* smm)
{
   if (dynamic_cast<const ShearModulus_Constant*>(smm))
      return(scinew ShearModulus_Constant(dynamic_cast<const ShearModulus_Constant*>(smm)));
   else if (dynamic_cast<const ShearModulus_Nadal*>(smm))
      return(scinew ShearModulus_Nadal(dynamic_cast<const ShearModulus_Nadal*>(smm)));
   else if (dynamic_cast<const ShearModulus_Borja*>(smm))
      return(scinew ShearModulus_Borja(dynamic_cast<const ShearModulus_Borja*>(smm)));
   else {
      cerr << "**WARNING** Creating copy of default (constant shear modulus) model" << endl;
      return(scinew ShearModulus_Constant(dynamic_cast<const ShearModulus_Constant*>(smm)));
   }
}