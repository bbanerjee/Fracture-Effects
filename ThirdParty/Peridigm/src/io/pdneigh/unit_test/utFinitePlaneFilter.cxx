//@HEADER
// ************************************************************************
//
//                             Peridigm
//                 Copyright (2011) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions?
// David J. Littlewood   djlittl@sandia.gov
// John A. Mitchell      jamitch@sandia.gov
// Michael L. Parks      mlparks@sandia.gov
// Stewart A. Silling    sasilli@sandia.gov
//
// ************************************************************************
//@HEADER

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/unit_test.hpp>
#include <boost/test/parameterized_test.hpp>
#include "../PdZoltan.h"
#include "PdVTK.h"
#include "quick_grid/QuickGrid.h"
#include "quick_grid/QuickGridData.h"
#include "../NeighborhoodList.h"
#include "../BondFilter.h"
#include "vtkIdList.h"
#include "vtkKdTree.h"
#include "vtkKdTreePointLocator.h"
#include <iostream>

using namespace PdBondFilter;
using std::tr1::shared_ptr;
using namespace boost::unit_test;
using std::cout;

static size_t myRank = 0;
static size_t numProcs = 1;
const size_t nx = 2;
const size_t ny = 2;
const size_t nz = 2;
/*
 * NOTE THAT THIS MAKES edges of cube of length 1.0;
 */
const double cube_edge_length=2.0;
const double xStart = -cube_edge_length/nx/2.0;
const double xLength = cube_edge_length;
const double yStart = -cube_edge_length/ny/2.0;
const double yLength = cube_edge_length;
const double zStart = -cube_edge_length/nz/2.0;
const double zLength = cube_edge_length;
const Spec1D xSpec(nx,xStart,xLength);
const Spec1D ySpec(ny,yStart,yLength);
const Spec1D zSpec(nz,zStart,zLength);
const size_t numCells = nx*ny*nz;
const double SCALE=1.1*sqrt(3);
const double horizon = SCALE*xSpec.getCellSize();

PdGridData getGrid() {

	/*
	 * 2x2x2 Grid of points/cells
	 */
	QUICKGRID::TensorProduct3DMeshGenerator cellPerProcIter(numProcs,horizon,xSpec,ySpec,zSpec);
	QUICKGRID::QuickGridData decomp =  QUICKGRID::getDiscretization(myRank, cellPerProcIter);

	// This load-balances
	decomp = getLoadBalancedDiscretization(decomp);

	{
		/*
		 * SANITY CHECK on Expected coordinates and IDs
		 */
		size_t numOverlapPoints = decomp.numPoints;
		int ids[] = {0,1,2,3,4,5,6,7};
		double x[]={
				0.0,0.0,0.0,
				1.0,0.0,0.0,
				0.0,1.0,0.0,
				1.0,1.0,0.0,
				0.0,0.0,1.0,
				1.0,0.0,1.0,
				0.0,1.0,1.0,
				1.0,1.0,1.0
		};
		BOOST_CHECK(8==numOverlapPoints);
		for(int j=0;j<numOverlapPoints;j++){
			BOOST_CHECK(decomp.myGlobalIDs.get()[j]==ids[j]);
			BOOST_CHECK(decomp.myX.get()[j*3+0]==x[j*3+0]);
			BOOST_CHECK(decomp.myX.get()[j*3+1]==x[j*3+1]);
			BOOST_CHECK(decomp.myX.get()[j*3+2]==x[j*3+2]);
		}

	}

	return decomp;
}

FinitePlane getCase_1a(){
	double sqrt2=sqrt(2.0);
	double n[3]; n[0]=-1.0/sqrt2;n[1]=1.0/sqrt2;n[2]=0.0;
	double r0[3]; r0[0]=0.0; r0[1]=0.0; r0[2]=0.0;
	double ub[3]; ub[0]=1.0/sqrt2; ub[1]=1.0/sqrt2;ub[2]=0.0;
	double b=sqrt(2), a=1.0;
	return FinitePlane(n,r0,ub,b,a);
}

FinitePlane getCase_1b(){
	double sqrt2=sqrt(2.0);
	double n[3]; n[0]=1.0/sqrt2;n[1]=1.0/sqrt2;n[2]=0.0;
	double r0[3]; r0[0]=0.0; r0[1]=1.0; r0[2]=0.0;
	double ub[3]; ub[0]=1.0/sqrt2; ub[1]=-1.0/sqrt2;ub[2]=0.0;
	double b=sqrt(2), a=1.0;
	return FinitePlane(n,r0,ub,b,a);
}

FinitePlane getCase_2a(){
	double sqrt2=sqrt(2.0);
	double n[3]; n[0]=-1.0/sqrt2;n[1]=0.0; n[2]=1.0/sqrt2;
	double r0[3]; r0[0]=0.0; r0[1]=0.0; r0[2]=0.0;
	double ub[3]; ub[0]=0.0; ub[1]=1.0;ub[2]=0.0;
	double b=1.0, a=sqrt2;
	return FinitePlane(n,r0,ub,b,a);
}

FinitePlane getCase_2b(){
	double sqrt2=sqrt(2.0);
	double n[3]; n[0]=1.0/sqrt2;n[1]=0.0; n[2]=1.0/sqrt2;
	double r0[3]; r0[0]=1.0; r0[1]=1.0; r0[2]=0.0;
	double ub[3]; ub[0]=0.0; ub[1]=-1.0;ub[2]=0.0;
	double b=1.0, a=sqrt2;
	return FinitePlane(n,r0,ub,b,a);
}

FinitePlane getCase_3a(){
	double sqrt2=sqrt(2.0);
	double n[3]; n[0]=0.0; n[1]=-1.0/sqrt2; n[2]=1.0/sqrt2;
	double r0[3]; r0[0]=0.0; r0[1]=0.0; r0[2]=0.0;
	double ub[3]; ub[0]=0.0; ub[1]=1.0/sqrt2;ub[2]=1.0/sqrt2;
	double a=1.0, b=sqrt2;
	return FinitePlane(n,r0,ub,b,a);
}

void case_1a() {

	PdGridData decomp = getGrid();
	FinitePlane plane = getCase_1a();
	shared_ptr<BondFilter> filterPtr=shared_ptr<BondFilter>(new FinitePlaneFilter(plane));

	/*
	 * Create KdTree; Since this is serial xOwned = xOverlap and numOwned = numOverlap
	 */
	shared_ptr<double> xOwnedPtr = decomp.myX;
	shared_ptr<double> xOverlapPtr = decomp.myX;
	size_t numOverlapPoints = decomp.numPoints;
	vtkSmartPointer<vtkUnstructuredGrid> overlapGrid = PdVTK::getGrid(xOverlapPtr,numOverlapPoints);
	vtkKdTreePointLocator* kdTree = vtkKdTreePointLocator::New();
	kdTree->SetDataSet(overlapGrid);

	/*
	 * ANSWERS for each ID
	 * list size for each point
	 */
	// known local ids
	size_t ids[] = {0,1,2,3,4,5,6,7};
	bool markForExclusion[8];
	// Expected: filter should evaluate this list size for each id
	size_t size[] = {4,2,2,4,4,2,2,4};
	// Expected: filter should return these flags for each local id
	bool n0[]={1,1,1,0,0,1,1,0};
	bool n1[]={1,1,1,1,1,0,1,1};
	bool n2[]={1,1,1,1,1,1,0,1};
	bool n3[]={0,1,1,1,0,1,1,0};
	bool n4[]={0,1,1,0,1,1,1,0};
	bool n5[]={1,0,1,1,1,1,1,1};
	bool n6[]={1,1,0,1,1,1,1,1};
	bool n7[]={0,1,1,0,0,1,1,1};
	bool * expectedFlags[] = {n0,n1,n2,n3,n4,n5,n6,n7};
	{
		for(size_t i=0;i<8;i++){
			/*
			 * look at neighborhood of id = 0
			 */
			size_t id=ids[i];
			vtkIdList* kdTreeList = vtkIdList::New();
			/*
			 * Note that list returned includes this point *
			 */
			double *x = decomp.myX.get()+3*id;
			kdTree->FindPointsWithinRadius(horizon, x, kdTreeList);

			/*
			 * Now determine which points are included
			 */
			filterPtr->filterBonds(kdTreeList,x,id,decomp.myX.get(),markForExclusion);
			bool *flags = expectedFlags[i];
			/*
			 * Assert flags
			 */
			for(int j=0;j<8;j++){
//				cout << "filter flag, expected flag = " << *(markForExclusion+j) << ", " << *(flags+j) << endl;
				BOOST_CHECK(*(flags+j)==*(markForExclusion+j));
			}

			// delete tree list for this point
			kdTreeList->Delete();
		}
	}

	decomp = createAndAddNeighborhood(decomp,horizon,filterPtr);

	/*
	 * Assert neighbors
	 */
	int *neigh = decomp.neighborhood.get();
	for(size_t n=0;n<8;n++){
		/*
		 * Assert number of neighbors
		 */
		int numNeigh = *neigh; neigh++;
		/*
		 * Note that we subtract 1 here because the list
		 * size is always '1' greater than the number of
		 * neighbors in order to store 'num neighbors'
		 * in the list.
		 */
		BOOST_CHECK((size[n]-1)==numNeigh);

		/*
		 * Expected
		 */
		bool *flags = expectedFlags[n];
		for(size_t j=0;j<numNeigh;j++,neigh++){
			int id = *neigh;
			BOOST_CHECK((flags+id));
		}

	}


}

void case_1b() {

	PdGridData decomp = getGrid();
	FinitePlane plane = getCase_1b();
	RCP<BondFilter> filterPtr=RCP<BondFilter>(new FinitePlaneFilter(plane));

	/*
	 * Create KdTree; Since this is serial xOwned = xOverlap and numOwned = numOverlap
	 */
	std::tr1::shared_ptr<double> xOwnedPtr = decomp.myX;
	std::tr1::shared_ptr<double> xOverlapPtr = decomp.myX;
	size_t numOverlapPoints = decomp.numPoints;
	vtkSmartPointer<vtkUnstructuredGrid> overlapGrid = PdVTK::getGrid(xOverlapPtr,numOverlapPoints);
	vtkKdTreePointLocator* kdTree = vtkKdTreePointLocator::New();
	kdTree->SetDataSet(overlapGrid);

	/*
	 * ANSWERS for each ID
	 * list size for each point
	 */
	// known local ids
	size_t ids[] = {0,1,2,3,4,5,6,7};
	bool markForExclusion[8];
	// Expected: filter should evaluate this list size for each id
	size_t size[] = {2,4,4,2,2,4,4,2};
	// Expected: filter should return these flags for each local id
	bool n0[]={1,1,1,1,0,1,1,1};
	bool n1[]={1,1,0,1,1,0,0,1};
	bool n2[]={1,0,1,1,1,0,0,1};
	bool n3[]={1,1,1,1,1,1,1,0};
	bool n4[]={0,1,1,1,1,1,1,1};
	bool n5[]={1,0,0,1,1,1,0,1};
	bool n6[]={1,0,0,1,1,0,1,1};
	bool n7[]={1,1,1,0,1,1,1,1};
	bool * expectedFlags[] = {n0,n1,n2,n3,n4,n5,n6,n7};
	size_t numCheck=8;
	{
		for(size_t i=0;i<numCheck;i++){
			/*
			 * look at neighborhood of id = 0
			 */
			size_t id=ids[i];
			vtkIdList* kdTreeList = vtkIdList::New();
			/*
			 * Note that list returned includes this point *
			 */
			double *x = decomp.myX.get()+3*id;
			kdTree->FindPointsWithinRadius(horizon, x, kdTreeList);

			/*
			 * Now determine which points are included
			 */
			filterPtr->filterBonds(kdTreeList,x,id,decomp.myX.get(),markForExclusion);
			bool *flags = expectedFlags[i];
			/*
			 * Assert flags
			 */
			for(int j=0;j<8;j++){
//				cout << "filter flag, expected flag = " << *(markForExclusion+j) << ", " << *(flags+j) << endl;
				BOOST_CHECK(*(flags+j)==*(markForExclusion+j));
			}

			// delete tree list for this point
			kdTreeList->Delete();
		}
	}

	decomp = createAndAddNeighborhood(decomp,horizon,filterPtr);

	/*
	 * Assert neighbors
	 */
	int *neigh = decomp.neighborhood.get();
	for(size_t n=0;n<numCheck;n++){
		/*
		 * Assert number of neighbors
		 */
		int numNeigh = *neigh; neigh++;
		/*
		 * Note that we subtract 1 here because the list
		 * size is always '1' greater than the number of
		 * neighbors in order to store 'num neighbors'
		 * in the list.
		 */
		BOOST_CHECK((size[n]-1)==numNeigh);

		/*
		 * Expected
		 */
		bool *flags = expectedFlags[n];
		for(size_t j=0;j<numNeigh;j++,neigh++){
			int id = *neigh;
			BOOST_CHECK((flags+id));
		}

	}


}

void case_2a() {

	PdGridData decomp = getGrid();
	FinitePlane plane = getCase_2a();
	RCP<BondFilter> filterPtr=RCP<BondFilter>(new FinitePlaneFilter(plane));

	/*
	 * Create KdTree; Since this is serial xOwned = xOverlap and numOwned = numOverlap
	 */
	std::tr1::shared_ptr<double> xOwnedPtr = decomp.myX;
	std::tr1::shared_ptr<double> xOverlapPtr = decomp.myX;
	size_t numOverlapPoints = decomp.numPoints;
	vtkSmartPointer<vtkUnstructuredGrid> overlapGrid = PdVTK::getGrid(xOverlapPtr,numOverlapPoints);
	vtkKdTreePointLocator* kdTree = vtkKdTreePointLocator::New();
	kdTree->SetDataSet(overlapGrid);

	/*
	 * ANSWERS for each ID
	 * list size for each point
	 */
	// known local ids
	size_t ids[] = {0,1,2,3,4,5,6,7};
	bool markForExclusion[8];
	// Expected: filter should evaluate this list size for each id
	size_t size[] = {4,2,4,2,2,4,2,4};
	// Expected: filter should return these flags for each local id
	bool n0[]={1,1,0,1,1,0,1,0};
	bool n1[]={1,1,1,0,1,1,1,1};
	bool n2[]={0,1,1,1,1,0,1,0};
	bool n3[]={1,0,1,1,1,1,1,1};
	bool n4[]={1,1,1,1,1,1,0,1};
	bool n5[]={0,1,0,1,1,1,1,0};
	bool n6[]={1,1,1,1,0,1,1,1};
	bool n7[]={0,1,0,1,1,0,1,1};
	bool * expectedFlags[] = {n0,n1,n2,n3,n4,n5,n6,n7};
	{
		for(size_t i=0;i<8;i++){
			/*
			 * look at neighborhood of id = 0
			 */
			size_t id=ids[i];
			vtkIdList* kdTreeList = vtkIdList::New();
			/*
			 * Note that list returned includes this point *
			 */
			double *x = decomp.myX.get()+3*id;
			kdTree->FindPointsWithinRadius(horizon, x, kdTreeList);

			/*
			 * Now determine which points are included
			 */
			filterPtr->filterBonds(kdTreeList,x,id,decomp.myX.get(),markForExclusion);
			bool *flags = expectedFlags[i];
			/*
			 * Assert flags
			 */
			for(int j=0;j<8;j++){
//				cout << "filter flag, expected flag = " << *(markForExclusion+j) << ", " << *(flags+j) << endl;
				BOOST_CHECK(*(flags+j)==*(markForExclusion+j));
			}

			// delete tree list for this point
			kdTreeList->Delete();
		}
	}

	decomp = createAndAddNeighborhood(decomp,horizon,filterPtr);

	/*
	 * Assert neighbors
	 */
	int *neigh = decomp.neighborhood.get();
	for(size_t n=0;n<8;n++){
		/*
		 * Assert number of neighbors
		 */
		int numNeigh = *neigh; neigh++;
		/*
		 * Note that we subtract 1 here because the list
		 * size is always '1' greater than the number of
		 * neighbors in order to store 'num neighbors'
		 * in the list.
		 */
		BOOST_CHECK((size[n]-1)==numNeigh);

		/*
		 * Expected
		 */
		bool *flags = expectedFlags[n];
		for(size_t j=0;j<numNeigh;j++,neigh++){
			int id = *neigh;
			BOOST_CHECK((flags+id));
		}

	}

}


void case_2b() {

	PdGridData decomp = getGrid();
	FinitePlane plane = getCase_2b();
	RCP<BondFilter> filterPtr=RCP<BondFilter>(new FinitePlaneFilter(plane));

	/*
	 * Create KdTree; Since this is serial xOwned = xOverlap and numOwned = numOverlap
	 */
	std::tr1::shared_ptr<double> xOwnedPtr = decomp.myX;
	std::tr1::shared_ptr<double> xOverlapPtr = decomp.myX;
	size_t numOverlapPoints = decomp.numPoints;
	vtkSmartPointer<vtkUnstructuredGrid> overlapGrid = PdVTK::getGrid(xOverlapPtr,numOverlapPoints);
	vtkKdTreePointLocator* kdTree = vtkKdTreePointLocator::New();
	kdTree->SetDataSet(overlapGrid);

	/*
	 * ANSWERS for each ID
	 * list size for each point
	 */
	// known local ids
	size_t ids[] = {0,1,2,3,4,5,6,7};
	bool markForExclusion[8];
	// Expected: filter should evaluate this list size for each id
	size_t size[] = {2,4,2,4,4,2,4,2};
	// Expected: filter should return these flags for each local id
	bool n0[]={1,1,0,1,1,1,1,1};
	bool n1[]={1,1,1,0,0,1,0,1};
	bool n2[]={0,1,1,1,1,1,1,1};
	bool n3[]={1,0,1,1,0,1,0,1};
	bool n4[]={1,0,1,0,1,1,0,1};
	bool n5[]={1,1,1,1,1,1,1,0};
	bool n6[]={1,0,1,0,0,1,1,1};
	bool n7[]={1,1,1,1,1,0,1,1};
	bool * expectedFlags[] = {n0,n1,n2,n3,n4,n5,n6,n7};
	size_t numCheck=8;
	{
		for(size_t i=0;i<numCheck;i++){
			/*
			 * look at neighborhood of id = 0
			 */
			size_t id=ids[i];
			vtkIdList* kdTreeList = vtkIdList::New();
			/*
			 * Note that list returned includes this point *
			 */
			double *x = decomp.myX.get()+3*id;
			kdTree->FindPointsWithinRadius(horizon, x, kdTreeList);

			/*
			 * Now determine which points are included
			 */
			filterPtr->filterBonds(kdTreeList,x,id,decomp.myX.get(),markForExclusion);
			bool *flags = expectedFlags[i];
			/*
			 * Assert flags
			 */
			for(int j=0;j<8;j++){
//				cout << "filter flag, expected flag = " << *(markForExclusion+j) << ", " << *(flags+j) << endl;
				BOOST_CHECK(*(flags+j)==*(markForExclusion+j));
			}

			// delete tree list for this point
			kdTreeList->Delete();
		}
	}

	decomp = createAndAddNeighborhood(decomp,horizon,filterPtr);

	/*
	 * Assert neighbors
	 */
	int *neigh = decomp.neighborhood.get();
	for(size_t n=0;n<numCheck;n++){
		/*
		 * Assert number of neighbors
		 */
		int numNeigh = *neigh; neigh++;
		/*
		 * Note that we subtract 1 here because the list
		 * size is always '1' greater than the number of
		 * neighbors in order to store 'num neighbors'
		 * in the list.
		 */
		BOOST_CHECK((size[n]-1)==numNeigh);

		/*
		 * Expected
		 */
		bool *flags = expectedFlags[n];
		for(size_t j=0;j<numNeigh;j++,neigh++){
			int id = *neigh;
			BOOST_CHECK((flags+id));
		}

	}


}

void case_3a() {

	PdGridData decomp = getGrid();
	FinitePlane plane = getCase_3a();
	RCP<BondFilter> filterPtr=RCP<BondFilter>(new FinitePlaneFilter(plane));

	/*
	 * Create KdTree; Since this is serial xOwned = xOverlap and numOwned = numOverlap
	 */
	std::tr1::shared_ptr<double> xOwnedPtr = decomp.myX;
	std::tr1::shared_ptr<double> xOverlapPtr = decomp.myX;
	size_t numOverlapPoints = decomp.numPoints;
	vtkSmartPointer<vtkUnstructuredGrid> overlapGrid = PdVTK::getGrid(xOverlapPtr,numOverlapPoints);
	vtkKdTreePointLocator* kdTree = vtkKdTreePointLocator::New();
	kdTree->SetDataSet(overlapGrid);

	/*
	 * ANSWERS for each ID
	 * list size for each point
	 */
	// known local ids
	size_t ids[] = {0,1,2,3,4,5,6,7};
	bool markForExclusion[8];
	// Expected: filter should evaluate this list size for each id
	size_t size[] = {4,4,2,2,2,2,4,4};
	// Expected: filter should return these flags for each local id
	bool n0[]={1,0,1,1,1,1,0,0};
	bool n1[]={0,1,1,1,1,1,0,0};
	bool n2[]={1,1,1,0,1,1,1,1};
	bool n3[]={1,1,0,1,1,1,1,1};
	bool n4[]={1,1,1,1,1,0,1,1};
	bool n5[]={1,1,1,1,0,1,1,1};
	bool n6[]={0,0,1,1,1,1,1,0};
	bool n7[]={0,0,1,1,1,1,0,1};
	bool * expectedFlags[] = {n0,n1,n2,n3,n4,n5,n6,n7};
	{
		for(size_t i=0;i<8;i++){
			/*
			 * look at neighborhood of id = 0
			 */
			size_t id=ids[i];
			vtkIdList* kdTreeList = vtkIdList::New();
			/*
			 * Note that list returned includes this point *
			 */
			double *x = decomp.myX.get()+3*id;
			kdTree->FindPointsWithinRadius(horizon, x, kdTreeList);

			/*
			 * Now determine which points are included
			 */
			filterPtr->filterBonds(kdTreeList,x,id,decomp.myX.get(),markForExclusion);
			bool *flags = expectedFlags[i];
			/*
			 * Assert flags
			 */
			for(int j=0;j<8;j++){
//				cout << "filter flag, expected flag = " << *(markForExclusion+j) << ", " << *(flags+j) << endl;
				BOOST_CHECK(*(flags+j)==*(markForExclusion+j));
			}

			// delete tree list for this point
			kdTreeList->Delete();
		}
	}

	decomp = createAndAddNeighborhood(decomp,horizon,filterPtr);

	/*
	 * Assert neighbors
	 */
	int *neigh = decomp.neighborhood.get();
	for(size_t n=0;n<8;n++){
		/*
		 * Assert number of neighbors
		 */
		int numNeigh = *neigh; neigh++;
		/*
		 * Note that we subtract 1 here because the list
		 * size is always '1' greater than the number of
		 * neighbors in order to store 'num neighbors'
		 * in the list.
		 */
		BOOST_CHECK((size[n]-1)==numNeigh);

		/*
		 * Expected
		 */
		bool *flags = expectedFlags[n];
		for(size_t j=0;j<numNeigh;j++,neigh++){
			int id = *neigh;
			BOOST_CHECK((flags+id));
		}

	}


}

bool init_unit_test_suite()
{
	// Add a suite for each processor in the test
	bool success=true;
	test_suite* proc = BOOST_TEST_SUITE( "utFinitePlaneFilter" );
	proc->add(BOOST_TEST_CASE( &case_1a ));
	proc->add(BOOST_TEST_CASE( &case_1b ));
	proc->add(BOOST_TEST_CASE( &case_2a ));
	proc->add(BOOST_TEST_CASE( &case_2b ));
	proc->add(BOOST_TEST_CASE( &case_3a ));
	framework::master_test_suite().add( proc );
	return success;
}


bool init_unit_test()
{
	init_unit_test_suite();
	return true;
}

int main
(
		int argc,
		char* argv[]
)
{


	// Initialize UTF
	return unit_test_main( init_unit_test, argc, argv );
}
