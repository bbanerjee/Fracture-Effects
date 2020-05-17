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
 * CellIndexVector.h
 *
 *  Created on: 18/10/2013
 *      Author: banerjee
 */

#ifndef CELLINDEXVECTOR_H_
#define CELLINDEXVECTOR_H_

#include <vector>

namespace BrMPM {

  class CellIndexVector
  {
  public:

    CellIndexVector() {}
    virtual ~CellIndexVector() {}

    inline CellIndexVector(const CellIndexVector& vec)
    {
      for (auto iter = (vec.d_data).begin(); iter != (vec.d_data).end(); ++iter) {
        d_data.emplace_back(*iter);
      }
    }

    inline CellIndexVector(const std::vector<int>& data)
    {
      for (auto iter = data.begin(); iter != data.end(); ++iter) {
        d_data.emplace_back(*iter);
      }
    }

    inline void operator=(const CellIndexVector& vec) {
      for (unsigned int ii = 0; ii < (vec.d_data).size(); ++ii) {
        d_data[ii] = (vec.d_data)[ii];
      }
    }

    inline unsigned int size() const {return d_data.size();}

    inline int& operator[](int index) {return d_data.at(index);}
    inline const int& operator[](int index) const {return d_data.at(index);}

    inline CellIndexVector operator*(const CellIndexVector& vec) const {
      return CellIndexVector(*this);
    }

    inline void zero() {
      for (unsigned int ii = 0; ii < d_data.size(); ++ii) {
        d_data[ii] = 0;
      }
    }

  private:

    std::vector<int> d_data;
  };

} /* namespace BrMPM */

#endif /* CELLINDEXVECTOR_H_ */