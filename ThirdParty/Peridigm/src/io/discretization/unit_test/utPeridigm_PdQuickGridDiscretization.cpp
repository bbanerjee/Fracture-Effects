/*! \file utPeridigm_PdQuickGridDiscretization.cpp */

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
#include <Epetra_ConfigDefs.h> // used to define HAVE_MPI
#ifdef HAVE_MPI
  #include <Epetra_MpiComm.h>
#else
  #include <Epetra_SerialComm.h>
#endif
#include "Peridigm_PdQuickGridDiscretization.hpp"

using namespace boost::unit_test;
using namespace Teuchos;
using namespace PeridigmNS;

void simpleTensorProductMesh()
{
  Teuchos::RCP<const Epetra_Comm> comm;
  #ifdef HAVE_MPI
    comm = rcp(new Epetra_MpiComm(MPI_COMM_WORLD));
  #else
    comm = rcp(new Epetra_SerialComm);
  #endif
  RCP<ParameterList> discParams = rcp(new ParameterList);

  // create a 2x2x2 discretization
  // specify a spherical neighbor search with the horizon a tad longer than the mesh spacing
  discParams->set("Type", "PdQuickGrid");
  discParams->set("NeighborhoodType", "Spherical");
  discParams->set("Horizon", 0.501);
  ParameterList& quickGridParams = discParams->sublist("TensorProduct3DMeshGenerator");
  quickGridParams.set("Type", "PdQuickGrid");
  quickGridParams.set("X Origin", 0.0);
  quickGridParams.set("Y Origin", 0.0);
  quickGridParams.set("Z Origin", 0.0);
  quickGridParams.set("X Length", 1.0);
  quickGridParams.set("Y Length", 1.0);
  quickGridParams.set("Z Length", 1.0);
  quickGridParams.set("Number Points X", 2);
  quickGridParams.set("Number Points Y", 2);
  quickGridParams.set("Number Points Z", 2);

  // create the discretization
  RCP<PdQuickGridDiscretization> discretization =
    rcp(new PdQuickGridDiscretization(comm, discParams));

  // sanity check, calling with a dimension other than 1 or 3 should throw an exception
  BOOST_CHECK_THROW(discretization->getGlobalOwnedMap(0), Teuchos::Exceptions::InvalidParameter);
  BOOST_CHECK_THROW(discretization->getGlobalOwnedMap(2), Teuchos::Exceptions::InvalidParameter);
  BOOST_CHECK_THROW(discretization->getGlobalOwnedMap(4), Teuchos::Exceptions::InvalidParameter);

  // basic checks on the 1d map
  Teuchos::RCP<const Epetra_BlockMap> map = discretization->getGlobalOwnedMap(1);
  BOOST_CHECK(map->NumGlobalElements() == 8);
  BOOST_CHECK(map->NumMyElements() == 8);
  BOOST_CHECK(map->ElementSize() == 1);
  BOOST_CHECK(map->IndexBase() == 0);
  BOOST_CHECK(map->UniqueGIDs() == true);
  int* myGlobalElements = map->MyGlobalElements();
  for(int i=0 ; i<map->NumMyElements() ; ++i)
    BOOST_CHECK(myGlobalElements[i] == i);

  // for the serial case, the map and the overlap map should match
  Teuchos::RCP<const Epetra_BlockMap> overlapMap = discretization->getGlobalOverlapMap(1);
  BOOST_CHECK(map->SameAs(*overlapMap) == true);

  // same checks for 3d map
  map = discretization->getGlobalOwnedMap(3);
  BOOST_CHECK(map->NumGlobalElements() == 8);
  BOOST_CHECK(map->NumMyElements() == 8);
  BOOST_CHECK(map->ElementSize() == 3);
  BOOST_CHECK(map->IndexBase() == 0);
  BOOST_CHECK(map->UniqueGIDs() == true);
  myGlobalElements = map->MyGlobalElements();
  for(int i=0 ; i<map->NumMyElements() ; ++i)
    BOOST_CHECK(myGlobalElements[i] == i);

  // for the serial case, the map and the overlap map should match
  overlapMap = discretization->getGlobalOverlapMap(3);
  BOOST_CHECK(map->SameAs(*overlapMap) == true);

  // check the bond map
  // the horizon was chosen such that each point should have three neighbors
  // note that if the NeighborhoodType parameter is not set to Spherical, this will fail
  Teuchos::RCP<const Epetra_BlockMap> bondMap = discretization->getGlobalBondMap();
  BOOST_CHECK(bondMap->NumGlobalElements() == 8);
  BOOST_CHECK(bondMap->NumMyElements() == 8);
  BOOST_CHECK(bondMap->ConstantElementSize() == 0);
  BOOST_CHECK(bondMap->IndexBase() == 0);
  BOOST_CHECK(bondMap->UniqueGIDs() == true);
  myGlobalElements = bondMap->MyGlobalElements();
  for(int i=0 ; i<bondMap->NumMyElements() ; ++i)
    BOOST_CHECK(myGlobalElements[i] == i); 

  BOOST_CHECK(discretization->getNumBonds() == 8*3);

  // check the initial positions
  // all three coordinates are contained in a single vector
  Teuchos::RCP<Epetra_Vector> initialX = discretization->getInitialX();
  BOOST_CHECK(initialX->MyLength() == 8*3);
  BOOST_CHECK(initialX->GlobalLength() == 8*3);

  BOOST_CHECK_CLOSE((*initialX)[0],  0.25, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[1],  0.25, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[2],  0.25, 1.0e-16);

  BOOST_CHECK_CLOSE((*initialX)[3],  0.75, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[4],  0.25, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[5],  0.25, 1.0e-16);
  
  BOOST_CHECK_CLOSE((*initialX)[6],  0.25, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[7],  0.75, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[8],  0.25, 1.0e-16);

  BOOST_CHECK_CLOSE((*initialX)[9],  0.75, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[10], 0.75, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[11], 0.25, 1.0e-16);

  BOOST_CHECK_CLOSE((*initialX)[12], 0.25, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[13], 0.25, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[14], 0.75, 1.0e-16);

  BOOST_CHECK_CLOSE((*initialX)[15], 0.75, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[16], 0.25, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[17], 0.75, 1.0e-16);

  BOOST_CHECK_CLOSE((*initialX)[18], 0.25, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[19], 0.75, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[20], 0.75, 1.0e-16);

  BOOST_CHECK_CLOSE((*initialX)[21], 0.75, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[22], 0.75, 1.0e-16);
  BOOST_CHECK_CLOSE((*initialX)[23], 0.75, 1.0e-16);

  // check cell volumes
  Teuchos::RCP<Epetra_Vector> volume = discretization->getCellVolume();
  BOOST_CHECK(volume->MyLength() == 8);
  BOOST_CHECK(volume->GlobalLength() == 8);
  for(int i=0 ; i<volume->MyLength() ; ++i)
    BOOST_CHECK_CLOSE((*volume)[i], 0.125, 1.0e-16);

  // check the neighbor lists
  Teuchos::RCP<PeridigmNS::NeighborhoodData> neighborhoodData = discretization->getNeighborhoodData();
  BOOST_CHECK(neighborhoodData->NumOwnedPoints() == 8);
  int* ownedIds = neighborhoodData->OwnedIDs();
  for(int i=0 ; i<neighborhoodData->NumOwnedPoints() ; ++i)
    BOOST_CHECK(ownedIds[i] == i);
  BOOST_CHECK(neighborhoodData->NeighborhoodListSize() == 32);
  int* neighborhood = neighborhoodData->NeighborhoodList();
  int* neighborhoodPtr = neighborhoodData->NeighborhoodPtr();

  BOOST_CHECK(neighborhoodPtr[0] == 0);
  BOOST_CHECK(neighborhood[0]    == 3);
  BOOST_CHECK(neighborhood[1]    == 1);
  BOOST_CHECK(neighborhood[2]    == 2);
  BOOST_CHECK(neighborhood[3]    == 4);

  BOOST_CHECK(neighborhoodPtr[1] == 4);
  BOOST_CHECK(neighborhood[4]    == 3);
  BOOST_CHECK(neighborhood[5]    == 0);
  BOOST_CHECK(neighborhood[6]    == 3);
  BOOST_CHECK(neighborhood[7]    == 5);

  BOOST_CHECK(neighborhoodPtr[2] == 8);
  BOOST_CHECK(neighborhood[8]    == 3);
  BOOST_CHECK(neighborhood[9]    == 0);
  BOOST_CHECK(neighborhood[10]   == 3);
  BOOST_CHECK(neighborhood[11]   == 6);

  BOOST_CHECK(neighborhoodPtr[3] == 12);
  BOOST_CHECK(neighborhood[12]   == 3);
  BOOST_CHECK(neighborhood[13]   == 1);
  BOOST_CHECK(neighborhood[14]   == 2);
  BOOST_CHECK(neighborhood[15]   == 7);

  BOOST_CHECK(neighborhoodPtr[4] == 16);
  BOOST_CHECK(neighborhood[16]   == 3);
  BOOST_CHECK(neighborhood[17]   == 0);
  BOOST_CHECK(neighborhood[18]   == 5);
  BOOST_CHECK(neighborhood[19]   == 6);

  BOOST_CHECK(neighborhoodPtr[5] == 20);
  BOOST_CHECK(neighborhood[20]   == 3);
  BOOST_CHECK(neighborhood[21]   == 1);
  BOOST_CHECK(neighborhood[22]   == 4);
  BOOST_CHECK(neighborhood[23]   == 7);

  BOOST_CHECK(neighborhoodPtr[6] == 24);
  BOOST_CHECK(neighborhood[24]   == 3);
  BOOST_CHECK(neighborhood[25]   == 2);
  BOOST_CHECK(neighborhood[26]   == 4);
  BOOST_CHECK(neighborhood[27]   == 7);

  BOOST_CHECK(neighborhoodPtr[7] == 28);
  BOOST_CHECK(neighborhood[28]   == 3);
  BOOST_CHECK(neighborhood[29]   == 3);
  BOOST_CHECK(neighborhood[30]   == 5);
  BOOST_CHECK(neighborhood[31]   == 6);
}

bool init_unit_test_suite()
{
	// Add a suite for each processor in the test
	bool success = true;

	test_suite* proc = BOOST_TEST_SUITE("utPeridigm_PdQuickGridDiscretization");
	proc->add(BOOST_TEST_CASE(&simpleTensorProductMesh));
	framework::master_test_suite().add(proc);

	return success;
}

bool init_unit_test()
{
	init_unit_test_suite();
	return true;
}

int main
(int argc, char* argv[])
{
  #ifdef HAVE_MPI
    MPI_Init(&argc,&argv);
  #endif

  // Initialize UTF
  return unit_test_main(init_unit_test, argc, argv);

  #ifdef HAVE_MPI
    MPI_Finalize();
  #endif
}
