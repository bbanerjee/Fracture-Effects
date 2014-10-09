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
#include "pdneigh/PdZoltan.h"
#include "mesh_input/quick_grid/QuickGrid.h"
#include "pdneigh/NeighborhoodList.h"
#include "PdutMpiFixture.h"
#include <iostream>

using namespace Pdut;
using std::tr1::shared_ptr;
using namespace boost::unit_test;
using std::cout;

static size_t myRank;
static size_t numProcs;
const size_t nx = 3;
const size_t ny = 1;
const size_t nz = 1;
const double xStart = -1.5;
const double xLength = 3.0;
const double yStart = -0.5;
const double yLength = 1.0;
const double zStart = -0.5;
const double zLength = 1.0;
const QUICKGRID::Spec1D xSpec(nx,xStart,xLength);
const QUICKGRID::Spec1D ySpec(ny,yStart,yLength);
const QUICKGRID::Spec1D zSpec(nz,zStart,zLength);
const double _cellVolume = xSpec.getCellSize()*ySpec.getCellSize()*zSpec.getCellSize();
const double x1 = xStart+xSpec.getCellSize()/2.0;
const double x2 = x1 + xSpec.getCellSize();
const double x3 = x2 + xSpec.getCellSize();
const double y = yStart + ySpec.getCellSize()/2.0;
const double z = zStart + zSpec.getCellSize()/2.0;
const size_t numCells = nx*ny*nz;

static int _neighborList[] = {
		1,1,                     /* numNeigh, neighbors */
		2,0,2,                   /* numNeigh, neighbors */
		1,1                      /* numNeigh, neighbors */
};
static int _neighborListSizeP0 = 5;
static int _neighborListSizeP1 = 2;

QUICKGRID::QuickGridData getGrid() {
	double horizon = 1.1;

	QUICKGRID::TensorProduct3DMeshGenerator cellPerProcIter(numProcs,horizon,xSpec,ySpec,zSpec);
	QUICKGRID::QuickGridData gridData =  QUICKGRID::getDiscretization(myRank, cellPerProcIter);

	gridData=PDNEIGH::getLoadBalancedDiscretization(gridData);
	return gridData;
}


void p0()
{
	QUICKGRID::QuickGridData gridData = getGrid();

	BOOST_CHECK(0 == myRank);
	/*
	 * problem dimension is 3
	 */
	BOOST_CHECK(3 == gridData.dimension);

	/*
	 * Total number of cells in test
	 */
	BOOST_CHECK(nx*ny*nz == gridData.globalNumPoints);

	/*
	 * Number of cells on this processor
	 */
	int myNumPoints = gridData.numPoints;

	/*
	 * Zoltan load balances this such that 2 points end up on P0
	 */
	BOOST_CHECK(2 == myNumPoints);

	/*
	 * assert length of neighborhood list on this processor
	 */
	BOOST_CHECK( _neighborListSizeP0 == gridData.sizeNeighborhoodList );

	/*
	 * Assert global ids on this processor
	 * Assert neighborhood list
	 */
	shared_ptr<int> gIds = gridData.myGlobalIDs;
	int *gIdsPtr = gIds.get();
	int *neighborhoodList = gridData.neighborhood.get();
	int *_neighAns = _neighborList;
	int start = 0;
	for(size_t id=start;id<gridData.numPoints+start;id++,gIdsPtr++){
		BOOST_CHECK( *gIdsPtr == (int)id );
		int numNeigh = *_neighAns;

		BOOST_CHECK( numNeigh == *neighborhoodList ); _neighAns++; neighborhoodList++;
		for(int i=0;i<numNeigh;i++){
			BOOST_CHECK( *_neighAns == *neighborhoodList ); _neighAns++; neighborhoodList++;
		}
		/*
		 * coordinates
		 */
	}

	/*
	 * Assert coordinates
	 */
	int id=0;
	const double tolerance = 1.0e-15;
	double *r = gridData.myX.get();
	BOOST_CHECK_CLOSE(r[3*id+0],x1,tolerance);
	BOOST_CHECK_CLOSE(r[3*id+1],y,tolerance);
	BOOST_CHECK_CLOSE(r[3*id+2],z,tolerance);
	id=1;
	BOOST_CHECK_CLOSE(r[3*id+0],x2,tolerance);
	BOOST_CHECK_CLOSE(r[3*id+1],y,tolerance);
	BOOST_CHECK_CLOSE(r[3*id+2],z,tolerance);

	// assert cell volumes
	double *v = gridData.cellVolume.get();
	double *end = v+myNumPoints;
	for(; v != end ; v++){
		BOOST_CHECK_CLOSE(*v,_cellVolume,tolerance);
	}

}

void p1()
{
	QUICKGRID::QuickGridData gridData = getGrid();

	BOOST_CHECK(1 == myRank);
	/*
	 * problem dimension is 3
	 */
	BOOST_CHECK(3 == gridData.dimension);

	/*
	 * Total number of cells in test
	 */
	BOOST_CHECK(nx*ny*nz == gridData.globalNumPoints);

	/*
	 * Number of cells on this processor
	 */
	int myNumPoints = gridData.numPoints;

	/*
	 * Zoltan load balances this such that 1 points end up on P1
	 */
	BOOST_CHECK(1 == myNumPoints);

	/*
	 * assert length of neighborhood list on this processor
	 */
	BOOST_CHECK( _neighborListSizeP1 == gridData.sizeNeighborhoodList );

	/*
	 * Assert global ids on this processor
	 * Assert neighborhood list
	 */
	shared_ptr<int> gIds = gridData.myGlobalIDs;
	int *gIdsPtr = gIds.get();
	int *neighborhoodList = gridData.neighborhood.get();
	int *_neighAns = &_neighborList[5];
	int start = 2;
	for(size_t id=start;id<gridData.numPoints+start;id++,gIdsPtr++){
		BOOST_CHECK( *gIdsPtr == (int)id );
		int numNeigh = *_neighAns;
		BOOST_CHECK( numNeigh == *neighborhoodList ); _neighAns++; neighborhoodList++;
		for(int i=0;i<numNeigh;i++){
			BOOST_CHECK( *_neighAns == *neighborhoodList ); _neighAns++; neighborhoodList++;
		}
	}

	/*
	 * Assert coordinates
	 */
	int id=0; // this is a local id
	const double tolerance = 1.0e-15;
	double *r = gridData.myX.get();
	BOOST_CHECK_CLOSE(r[3*id+0],x3,tolerance);
	BOOST_CHECK_CLOSE(r[3*id+1],y,tolerance);
	BOOST_CHECK_CLOSE(r[3*id+2],z,tolerance);

	// assert cell volumes
	double *v = gridData.cellVolume.get();
	double *end = v+myNumPoints;
	for(; v != end ; v++){
		BOOST_CHECK_CLOSE(*v,_cellVolume,tolerance);
	}

}

bool init_unit_test_suite()
{
	// Add a suite for each processor in the test
	bool success=true;
	if(0 == myRank){
		test_suite* proc = BOOST_TEST_SUITE( "ut_QuickGrid_loadBal_np2_3x1x1p0" );
		proc->add(BOOST_TEST_CASE( &p0 ));
		framework::master_test_suite().add( proc );
		return success;
	}
	if(1 == myRank){
		test_suite* proc = BOOST_TEST_SUITE( "ut_QuickGrid_loadBal_np2_3x1x1p1" );
		proc->add(BOOST_TEST_CASE( &p1 ));
		framework::master_test_suite().add( proc );
		return success;
	}
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

	// Initialize MPI and timer
	PdutMpiFixture myMpi = PdutMpiFixture(argc,argv);

	// These are static (file scope) variables
	myRank = myMpi.rank;
	numProcs = myMpi.numProcs;
	/**
	 * This test only make sense for numProcs == 2
	 */
	if(2 != numProcs){
		std::cerr << "Unit test runtime ERROR: ut_QuickGrid_loadBal_np2_3x1x1 only makes sense on 2 processors" << std::endl;
		std::cerr << "\t Re-run unit test $mpiexec -np 2 ./ut_QuickGrid_loadBal_np2_3x1x1" << std::endl;
		myMpi.PdutMpiFixture::~PdutMpiFixture();
		std::exit(-1);
	}



	// Initialize UTF
	return unit_test_main( init_unit_test, argc, argv );
}
