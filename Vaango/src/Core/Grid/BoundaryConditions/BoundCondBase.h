/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
 * Copyright (c) 2014-2016 Parresia Research Limited, New Zealand
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

#ifndef UINTAH_GRID_BoundCondBase_H
#define UINTAH_GRID_BoundCondBase_H

#include <Core/Grid/BoundaryConditions/BoundCondBaseP.h>

#include <string>

namespace Uintah {
using std::string;
   
/**************************************

CLASS
   BoundCondBase
   
   
GENERAL INFORMATION

   BoundCondBase.h

   John A. Schmidt
   Department of Mechanical Engineering
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  

KEYWORDS
   BoundCondBase

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

  class BoundCondBase  {
  public:
  
    /**
     *  \enum   BoundCondValueTypeEnum
     *  \author Tony Saad
     *  \date   August 29, 2013
     *
     *  \brief An enum that lists the datatypes associated with the values specified for boundary conditions.
     */
    enum BoundCondValueTypeEnum
    {
      INT_TYPE,
      DOUBLE_TYPE,
      VECTOR_TYPE,
      STRING_TYPE,
      UNKNOWN_TYPE
    };

    BoundCondBase() {};
    virtual ~BoundCondBase() {};
    BoundCondBaseP clone() { 
      return std::shared_ptr<BoundCondBase>(cloneImpl());
    }
    const string getBCVariable() const { return d_variable; }
    const string getBCType__NEW() const { return d_type__NEW; }
    const std::string getBCFaceName() const { return d_face_label; }
    const std::string getFunctorName() const { return d_functor_name; }
    
  protected:

    virtual BoundCondBase* cloneImpl() = 0;

  protected:
    string d_variable;          // Pressure, Density, etc
    string d_type__NEW;         // Dirichlet, Neumann, etc
    std::string d_face_label;   // holds the user specified name of the bc face: left-wall, ox-inlet,...
    std::string d_functor_name; // holds the name of a functor to be applied on this boundary
  };
} // End namespace Uintah

#endif
