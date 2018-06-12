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

#ifndef __MODEL_STATE_VISITOR_H__
#define __MODEL_STATE_VISITOR_H__

namespace Vaango {

class ModelStateBase;
class ModelState_SoilModelBrannon;
class ModelState_Arenisca3;
class ModelState_CamClay;

/////////////////////////////////////////////////////////////////////////////
/*!
  \class ModelStateVisitor
  \brief A visitor function that allows modifications to the ModelStateBase
         class objects as new models are introduced to the software
  \author Biswajit Banerjee \n
*/
/////////////////////////////////////////////////////////////////////////////
class ModelStateVisitorBase
{

public:
  virtual ~ModelStateVisitorBase();

  virtual void visit(ModelStateBase& state) = 0;
  virtual void visit(ModelState_SoilModelBrannon& state) = 0;
  virtual void visit(ModelState_Arenisca3& state) = 0;
  virtual void visit(ModelState_CamClay& state) = 0;
};

class ModelStateVisitor : public ModelStateVisitorBase
{

public:
  ~ModelStateVisitor() override = default;

  void visit(ModelStateBase& state) override {}

  void visit(ModelState_SoilModelBrannon& state) override {}

  void visit(ModelState_Arenisca3& state) override {}

  void visit(ModelState_CamClay& state) override {}
};

} // End namespace Vaango

#endif // __MODEL_STATE_VISITOR_H__