/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
 * Copyright (c) 2013-2014 Callaghan Innovation, New Zealand
 * Copyright (c) 2015-2016 Parresia Research Limited, New Zealand
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


#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldConditionFactory.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCond_vonMises.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCond_Gurson.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCond_CamClay.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCond_Arenisca3.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCond_Arena.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCond_ArenaMixture.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/InternalVariableModel.h>
#include <Core/Exceptions/ProblemSetupException.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Malloc/Allocator.h>
#include <string>

using namespace std;
using namespace Uintah;
using namespace Vaango;

/// Create an instance of a Yield Condition.
YieldCondition* YieldConditionFactory::create(Uintah::ProblemSpecP& ps)
{
   ProblemSpecP child = ps->findBlock("plastic_yield_condition");
   if(!child)
      throw ProblemSetupException("MPM::ConstitutiveModel:Cannot find yield condition.", __FILE__, __LINE__);
   string mat_type;
   if(!child->getAttribute("type", mat_type))
      throw ProblemSetupException("MPM::ConstitutiveModel:No type for yield condition.", __FILE__, __LINE__);
   
   if (mat_type == "vonMises")
      return(scinew YieldCond_vonMises(child));
   else if (mat_type == "gurson")
      return(scinew YieldCond_Gurson(child));
   else if (mat_type == "arena")
      return(scinew YieldCond_Arena(child));
   else if (mat_type == "arena_mixture")
      return(scinew YieldCond_ArenaMixture(child));
   else 
      throw ProblemSetupException("MPM::ConstitutiveModel:Unknown Yield Condition ("+mat_type+")",
                                  __FILE__, __LINE__);
}

/// Create an instance of a Yield Condition.
YieldCondition* YieldConditionFactory::create(Uintah::ProblemSpecP& ps,
                                              InternalVariableModel* intvar)
{
   ProblemSpecP child = ps->findBlock("plastic_yield_condition");
   if(!child)
      throw ProblemSetupException("MPM::ConstitutiveModel:Cannot find yield condition.", __FILE__, __LINE__);
   string mat_type;
   if(!child->getAttribute("type", mat_type))
      throw ProblemSetupException("MPM::ConstitutiveModel:No type for yield condition.", __FILE__, __LINE__);
   
   if (mat_type == "camclay")
      return(scinew YieldCond_CamClay(child, intvar));
   else if (mat_type == "arenisca3")
      return(scinew YieldCond_Arenisca3(child, intvar));
   else 
      throw ProblemSetupException("MPM::ConstitutiveModel:Unknown Yield Condition ("+mat_type+")",
                                  __FILE__, __LINE__);
}

YieldCondition* 
YieldConditionFactory::createCopy(const YieldCondition* yc)
{
   if (dynamic_cast<const YieldCond_vonMises*>(yc))
      return(scinew YieldCond_vonMises(dynamic_cast<const YieldCond_vonMises*>(yc)));

   else if (dynamic_cast<const YieldCond_Gurson*>(yc))
      return(scinew YieldCond_Gurson(dynamic_cast<const YieldCond_Gurson*>(yc)));

   else if (dynamic_cast<const YieldCond_CamClay*>(yc))
      return(scinew YieldCond_CamClay(dynamic_cast<const YieldCond_CamClay*>(yc)));

   else if (dynamic_cast<const YieldCond_Arenisca3*>(yc))
      return(scinew YieldCond_Arenisca3(dynamic_cast<const YieldCond_Arenisca3*>(yc)));

   else if (dynamic_cast<const YieldCond_Arena*>(yc))
      return(scinew YieldCond_Arena(dynamic_cast<const YieldCond_Arena*>(yc)));

   else if (dynamic_cast<const YieldCond_ArenaMixture*>(yc))
      return(scinew YieldCond_ArenaMixture(dynamic_cast<const YieldCond_ArenaMixture*>(yc)));

   else 
      throw ProblemSetupException("Cannot create copy of unknown yield condition", __FILE__, __LINE__);
}
