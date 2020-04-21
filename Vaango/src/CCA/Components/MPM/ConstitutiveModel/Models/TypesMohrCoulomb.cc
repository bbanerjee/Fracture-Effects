/*
 * The MIT License
 *
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

#include <CCA/Components/MPM/ConstitutiveModel/Models/TypesMohrCoulomb.h>

#include <array>
#include <iostream>

namespace Uintah
{

std::ostream&
operator<<(std::ostream& out, const RegionType& dc)
{
  static std::array<const char*, 4> names = {
    { "APEX_REGION_1", "APEX_REGION_2", "APEX_REGION_3", "APEX_REGION_4" }
  };
  out << names[static_cast<int>(dc)];
  return out;
}

std::ostream&
operator<<(std::ostream& out, const DriftCorrection& dc)
{
  static std::array<const char*, 3> names = {
    { "NO_CORRECTION", "CORRECTION_AT_BEGIN", "CORRECTION_AT_END" }
  };
  out << names[static_cast<int>(dc)];
  return out;
}

std::ostream&
operator<<(std::ostream& out, const SolutionAlgorithm& sa)
{
  static std::array<const char*, 9> names = {
    { "RUNGE_KUTTA_SECOND_ORDER_MODIFIED_EULER",
      "RUNGE_KUTTA_THIRD_ORDER_NYSTROM", "RUNGE_KUTTA_THIRD_ORDER_BOGACKI",
      "RUNGE_KUTTA_FOURTH_ORDER", "RUNGE_KUTTA_FIFTH_ORDER_ENGLAND",
      "RUNGE_KUTTA_FIFTH_ORDER_CASH", "RUNGE_KUTTA_FIFTH_ORDER_DORMAND",
      "RUNGE_KUTTA_FIFTH_ORDER_BOGACKI", "EXTRAPOLATION_BULIRSCH" }
  };
  out << names[static_cast<int>(sa)];
  return out;
}

} // end namespace Uintah
