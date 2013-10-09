#include <cmath>
#include <math.h>
#include <stdlib.h>

#include <MPMdatawarehouse.h>
#include <MPMsaveutil.h>
#include <MPMShapeFunction.h>






using namespace MPM;

 MPMdatawarehouse::MPMdatawarehouse()
             : d_id(0), d_time(new MPMTime())
 {
 /* d_pointMomentum.reserve(1000);
  d_pointInitialVelocity.reserve(1000);
  d_pointInitialPosition.reserve(1000);
  d_pointExternalForce.reserve(1000);
  d_pointInternalForce.reserve(1000);
  d_pointContactForce.reserve(1000);
  d_pointContactMomentum.reserve(1000);
  d_pointMass.reserve(1000); */
 }

 MPMdatawarehouse::MPMdatawarehouse(Uintah::ProblemSpecP& ps)
 {
   d_shapefunction.initialise(ps);
 }


 ~MPMdatawarehouse::MPMdatawarehouse() {}


/* void
 MPMdatawarehouse::initialise(Uintah::ProblemSpecP& ps)
 {
   d_shapefunction.initialise(ps);
 } */


 void
 MPMdatawarehouse::saveData(double dt, MaterialSPArray& matlist)
 {
   if (checkSave(dt)) {
           d_out.outputFileCount(d_save.saveData(d_out.outputFileCount(), matlist));
        }
   incrementTime(dt);
   d_id += 1;
 }

 
 void
 MPMdatawarehouse::dumpData(double dt, MaterialSPArray& matlist)
 {
   if (checkSave(dt)) {
           d_out.outputFileCount(d_save.dumpData(d_out.outputFileCount(), matlist));
        }
   incrementTime(dt);
   d_id+=1;
 }


 bool
 MPMdatawarehouse::checkSave(double dt)
 {
  double dr=d_time.currentTime()/dt;
  double dt0=dt*std::min(dr-std::floor(dr), std::ceil(dr)-dr);
  return dt0<dt/2;
 }

 
 void
 MPMdatawarehouse::init(char lable, int dwi, std::vector val)
 {
  d_id_vec.insert(std::pair<char, std::vector>(lable, val));
 }

 void
 MPMdatawarehouse::append(char lable, int dwi, std::vector val)
 {
  for (auto vec_iter=val.begin(); vec_iter !=val.end(); ++vec_iter) {
            double cur_num = *vec_iter;
            d_id_vec[lable].emplace_back(cur_num);
           }
  }

 
 void
 MPMdatawarehouse::add(char lable, int dwi, std::vector val)
 {
  if (d_id_vec[lable].size() ==0) {
      init(lable, dwi, val);   
   } else {
      append(lable, dwi, val);
   }
 }


 void
 MPMdatawarehouse::zero(char lable, int dwi)
 {
  std::vector zero;
  d_id_vec[lable]=0;  //wrong, it should be modified
 }


 std::vector
 MPMdatawarehouse::get(char lable, int dwi)
 {
  return d_id_vec[lable];
 }


 std::vector < std::vector<double> >
 MPMdatawarehouse::getMult(std::vector<char> lables, int dwi)
 {
   std::vector < std::vector<double> > output;
   for (auto iter = labels.begin(); iter != lables.end(); iter++) {
       char cur_lbl = *iter;
       output.emplace_back(cur_lbl);
   }
   return output;  
 }


 void
 MPMdatawarehouse::addParticles(int dwi, ArrayMatrixVec&  pointsInitialPosition,
                                ArrayMatrixVec& pointsPosition, 
                                ArrayMatrixVec& pointsMass, 
                                ArrayMatrix& pointsGradientVelocity,
                                ArrayMatrix& pointsStressVelocity, 
                                ArrayMatrix& pointsDeformationMatrix,
                                ArrayIntMatrixVecShape& cIndex,
                                ArrayMatrixVecShape& cWeightFunction,
                                ArrayMatrixShape& cWeightGradient,
                                std::vector<double>& pointsVolume,
                                double volume, double density)
{
 int Zero = 0;
 double const initialZero = 0.0;
 double const initialOne = 1.0;

 int numberPoints = pointsInitialPosition.size();

 std::vector<char> lables = {"pointMomentum", "pointInitialVelocity", "pointInitialPosition", "pointExternalForce",   "pointGradientVelocity", "pointVolumeStress", "pointInternalForce", "pointContactForce", "pointContactMomentum"};

 initialise(initialZero, pointsInitialPosition);
 initialise(initialZero, pointsPosition);
 initialise(volume*density, pointsMass);
 initialise(initialZero, pointsGradientVelocity);
 initialise(initialZero, pointsStressVelocity);
 initialise(Zero, cIndex);
 initialise(initialZero, cWeightFunction);
 initialise(initialZero, cWeightGradient);


 pointsVolume.resize(numberPoints, volume);


 identityMatrix(initialOne, pointsDeformationMatrix);
} 


 void 
 MPMdatawarehouse::initialise(double initial, ArrayMarixVec& vec_matrix)
{
  vec_matrix.resize(numberPoints);
  for (auto iter = vec_matrix.begin(); iter != vec_matrix.end(); iter++) {
      MatrixVec  cur_matrix = *iter;
      cur_matrix(initial);
  }
}
    
          
 
 void 
 MPMdatawarehouse::initialise(double initial, ArrayMatrix& vec_matrix)
{
  vec_matrix.resize(numberPoints);
  for (auto iter = vec_matrix.begin(); iter != vec_matrix.end(); iter++) {
      Matrix  cur_matrix = *iter;
      cur_matrix(initial);
  }
}
        

void 
 MPMdatawarehouse::initialise(int initial,  ArrayIntMatrixVecShape& vec_matrix)
{
  vec_matrix.resize(numberPoints);
  for (auto iter = vec_matrix.begin(); iter != vec_matrix.end(); iter++) {
      IntMatrixVecShape  cur_matrix = *iter;
      cur_matrix(initial);
  }
}

void 
 MPMdatawarehouse::initialise(double initial, ArrayMatrixVecShape& vec_matrix)
{
  vec_matrix.resize(numberPoints);
  for (auto iter = vec_matrix.begin(); iter != vec_matrix.end(); iter++) {
      MatrixVecShape  cur_matrix = *iter;
      cur_matrix(initial);
  }
}

void 
 MPMdatawarehouse::initialise(double initial, ArrayMatrixShape& vec_matrix)
{
  vec_matrix.resize(numberPoints);
  for (auto iter = vec_matrix.begin(); iter != vec_matrix.end(); iter++) {
      MatrixShape  cur_matrix = *iter;
      cur_matrix(initial);
  }
}
 
 void 
 MPMdatawarehouse::identityMatrix(double initial, ArrayMatrix& vec_matrix)
{
  vec_matrix.resize(numberPoints);
  for (auto iter = vec_matrix.begin(); iter != vec_matrix.end(); iter++) {
      Matrix  cur_matrix = *iter;
      for (auto mat_iter = cur_matrix.begin(); mat_iter != cur_matrix.end(), mat_iter++) {
          cur_index = *mat_iter;
          div_t divresult;
          divresult = div (cur_index, d_dim);
          int quotion = divresult.quot;
          int remainder = divresult.rem;
          if (quotion == remainder) {
             cur_matrix.set(quotion, remainder, initial);
          }
          else {
             cur_matrix.set(quotion, remainder, 0.0);
          }
       
       } 
                   
  }



}






  
























       
                 
