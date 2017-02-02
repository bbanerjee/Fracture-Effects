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

/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
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

//
//  Written by:
//   Eric Kuehne
//   Department of Computer Science
//   University of Utah
//   May 2000
//
//
//
//
// To add an Interface, a new class should be created in this file,
// and the appropriate query pure virtual should be added in the Field class.
//


#ifndef Datatypes_FieldInterface_h
#define Datatypes_FieldInterface_h

#include <Core/Datatypes/Datatype.h>
#include <Core/Geometry/Vector.h>
#include <Core/Geometry/Tensor.h>
#include <vector>

namespace Uintah {
  class Point;
  using std::vector;

class ScalarFieldInterface: public Datatype 
{
public:

  ScalarFieldInterface() {}
  ScalarFieldInterface(const ScalarFieldInterface&) {}
  virtual ~ScalarFieldInterface() {}

  virtual bool compute_min_max(double &minout, double &maxout,
			       bool cache = true) = 0;
  virtual bool interpolate(double &result, const Point &p) const = 0;
  virtual bool interpolate_many(vector<double> &results,
				const vector<Point> &points) const = 0;

  virtual double find_closest(double &result, const Point &p) const = 0;
};


class VectorFieldInterface: public Datatype 
{
public:
  VectorFieldInterface() {}
  VectorFieldInterface(const VectorFieldInterface&) {}
  virtual ~VectorFieldInterface() {}

  virtual bool compute_length_min_max(double &minout, double &maxout,
				      bool cache = true) = 0;
  virtual bool compute_min_max(Vector &minout, Vector &maxout,
			       bool cache = true) = 0;
  virtual bool interpolate(Vector &result, const Point &p) const = 0;
  virtual bool interpolate_many(vector<Vector> &results,
				const vector<Point> &points) const = 0;
  virtual double find_closest(Vector &result, const Point &p) const = 0;
};


class TensorFieldInterface: public Datatype
{
public:
  TensorFieldInterface() {}
  TensorFieldInterface(const TensorFieldInterface&) {}
  virtual ~TensorFieldInterface() {}

  virtual bool interpolate(Tensor &result, const Point &p) const = 0;
  virtual bool interpolate_many(vector<Tensor> &results,
				const vector<Point> &points) const = 0;
  virtual double find_closest(Tensor &result, const Point &p) const = 0;
};


} // end namespace Uintah


#endif // Datatypes_FieldInterface_h

