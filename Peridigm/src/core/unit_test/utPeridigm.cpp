/*! \file utPeridigm.cpp */

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
#include "Peridigm.hpp"
#include "Peridigm_Field.hpp"

using namespace boost::unit_test;
using namespace Teuchos;
using namespace PeridigmNS;

Teuchos::RCP<PeridigmNS::Peridigm> createTwoPointModel()
{
  Teuchos::RCP<Epetra_Comm> comm;
  #ifdef HAVE_MPI
    comm = rcp(new Epetra_MpiComm(MPI_COMM_WORLD));
  #else
    comm = rcp(new Epetra_SerialComm);
  #endif

  // set up parameter lists
  // these data would normally be read from an input xml file
  Teuchos::RCP<Teuchos::ParameterList> peridigmParams = rcp(new Teuchos::ParameterList());

  // discretization parameters
  ParameterList& discretizationParams = peridigmParams->sublist("Discretization");
  discretizationParams.set("Type", "PdQuickGrid");
  discretizationParams.set("Horizon", 2.1);

  // pdQuickGrid tensor product mesh generator parameters
  ParameterList& pdQuickGridParams = discretizationParams.sublist("TensorProduct3DMeshGenerator");
  pdQuickGridParams.set("Type", "PdQuickGrid");
  pdQuickGridParams.set("X Origin", -2.0);
  pdQuickGridParams.set("Y Origin", -0.5);
  pdQuickGridParams.set("Z Origin", -0.5);
  pdQuickGridParams.set("X Length",  4.0);
  pdQuickGridParams.set("Y Length",  1.0);
  pdQuickGridParams.set("Z Length",  1.0);
  pdQuickGridParams.set("Number Points X", 2);
  pdQuickGridParams.set("Number Points Y", 1);
  pdQuickGridParams.set("Number Points Z", 1);

  // material parameters
  ParameterList& materialParams = peridigmParams->sublist("Materials");
  ParameterList& linearElasticMaterialParams = materialParams.sublist("My Elastic Material");
  linearElasticMaterialParams.set("Material Model", "Elastic");
  linearElasticMaterialParams.set("Density", 7800.0);
  linearElasticMaterialParams.set("Bulk Modulus", 130.0e9);
  linearElasticMaterialParams.set("Shear Modulus", 78.0e9);

  // block parameters
  ParameterList& blockParams = peridigmParams->sublist("Blocks");
  ParameterList& blockGroupParams = blockParams.sublist("My Group of Blocks");
  blockGroupParams.set("Block Names", "block_1");
  blockGroupParams.set("Material", "My Elastic Material");

  // boundary conditions
  ParameterList& bcParams = peridigmParams->sublist("Boundary Conditions");
  // node sets
  // these sets associate a name with a list of node ids, stored as a string
  // in this case there's only one node per set
  bcParams.set("Min X Node Set", "0");
  bcParams.set("Max X Node Set", "1");
  // initial velocity boundary conditions
  // each boundary condition is associated with a node set, defined above
  ParameterList& initialVelocityMinXFace = bcParams.sublist("Initial Velocity Min X Face");
  initialVelocityMinXFace.set("Type", "Initial Velocity");
  initialVelocityMinXFace.set("Node Set", "Min X Node Set");
  initialVelocityMinXFace.set("Coordinate", "x");
  initialVelocityMinXFace.set("Value", "-1.0");
  ParameterList& initialVelocityMaxXFace = bcParams.sublist("Initial Velocity Max X Face");
  initialVelocityMaxXFace.set("Type", "Initial Velocity");
  initialVelocityMaxXFace.set("Node Set", "Max X Node Set");
  initialVelocityMaxXFace.set("Coordinate", "x");
  initialVelocityMaxXFace.set("Value", "1.0");

  // solver parameters
  ParameterList& solverParams = peridigmParams->sublist("Solver");
  solverParams.set("Verbose", "false");
  ParameterList& verletParams = solverParams.sublist("Verlet");
  verletParams.set("Initial Time", 0.0);
  verletParams.set("Final Time", 1.0);
  verletParams.set("Fixed dt", 1.0);

  // output parameters
  ParameterList& outputParams = peridigmParams->sublist("Output");
  ParameterList& outputVariables = outputParams.sublist("Output Variables");
  outputVariables.set("Force", true);

  // create the Peridigm object
  Teuchos::RCP<PeridigmNS::Peridigm> peridigm = rcp(new PeridigmNS::Peridigm(comm, peridigmParams));

  return peridigm;
}

Teuchos::RCP<PeridigmNS::Peridigm> createFourPointModel()
{
  Teuchos::RCP<Epetra_Comm> comm;
  #ifdef HAVE_MPI
    comm = rcp(new Epetra_MpiComm(MPI_COMM_WORLD));
  #else
    comm = rcp(new Epetra_SerialComm);
  #endif

  // set up parameter lists
  // these data would normally be read from an input xml file
  Teuchos::RCP<Teuchos::ParameterList> peridigmParams = rcp(new Teuchos::ParameterList());

  // discretization parameters
  ParameterList& discretizationParams = peridigmParams->sublist("Discretization");
  discretizationParams.set("Type", "PdQuickGrid");
  discretizationParams.set("Horizon", 2.1);

  // pdQuickGrid tensor product mesh generator parameters
  ParameterList& pdQuickGridParams = discretizationParams.sublist("TensorProduct3DMeshGenerator");
  pdQuickGridParams.set("Type", "PdQuickGrid");
  pdQuickGridParams.set("X Origin", -2.0);
  pdQuickGridParams.set("Y Origin", -2.0);
  pdQuickGridParams.set("Z Origin", -2.0);
  pdQuickGridParams.set("X Length",  4.0);
  pdQuickGridParams.set("Y Length",  4.0);
  pdQuickGridParams.set("Z Length",  4.0);
  pdQuickGridParams.set("Number Points X", 2);
  pdQuickGridParams.set("Number Points Y", 2);
  pdQuickGridParams.set("Number Points Z", 2);

  // material parameters
  ParameterList& materialParams = peridigmParams->sublist("Materials");
  ParameterList& linearElasticMaterialParams = materialParams.sublist("My Elastic Material");
  linearElasticMaterialParams.set("Material Model", "Elastic");
  linearElasticMaterialParams.set("Density", 7800.0);
  linearElasticMaterialParams.set("Bulk Modulus", 130.0e9);
  linearElasticMaterialParams.set("Shear Modulus", 78.0e9);

  // block parameters
  ParameterList& blockParams = peridigmParams->sublist("Blocks");
  ParameterList& blockGroupParams = blockParams.sublist("My Group of Blocks");
  blockGroupParams.set("Block Names", "block_1");
  blockGroupParams.set("Material", "My Elastic Material");

  // solver parameters
  ParameterList& solverParams = peridigmParams->sublist("Solver");
  solverParams.set("Verbose", "false");
  ParameterList& verletParams = solverParams.sublist("Verlet");
  verletParams.set("Initial Time", 0.0);
  verletParams.set("Final Time", 1.0);
  verletParams.set("Fixed dt", 1.0);

  // output parameters
  ParameterList& outputParams = peridigmParams->sublist("Output");
  ParameterList& outputVariables = outputParams.sublist("Output Variables");
  outputVariables.set("Force", true);

  // create the Peridigm object
  Teuchos::RCP<PeridigmNS::Peridigm> peridigm = rcp(new PeridigmNS::Peridigm(comm, peridigmParams));

  return peridigm;
}

void initialize()
{
  Teuchos::RCP<PeridigmNS::Peridigm> peridigm = createTwoPointModel();

  BOOST_CHECK_EQUAL(peridigm->getOneDimensionalMap()->NumMyElements(), 2);
  BOOST_CHECK_EQUAL(peridigm->getOneDimensionalMap()->ElementSize(), 1);
  BOOST_CHECK_EQUAL(peridigm->getOneDimensionalOverlapMap()->NumMyElements(), 2);
  BOOST_CHECK_EQUAL(peridigm->getOneDimensionalOverlapMap()->ElementSize(), 1);
  BOOST_CHECK_EQUAL(peridigm->getThreeDimensionalMap()->NumMyElements(), 2);
  BOOST_CHECK_EQUAL(peridigm->getThreeDimensionalMap()->ElementSize(), 3);
  BOOST_CHECK_EQUAL(peridigm->getBondMap()->NumMyElements(), 2);

  // \todo Write additional asserts
}

//! This is a serial rebalance test; the rebalance should have no effect.
void rebalanceTwoPointModel()
{
  Teuchos::RCP<PeridigmNS::Peridigm> peridigm = createTwoPointModel();

  PeridigmNS::FieldManager& fieldManager = PeridigmNS::FieldManager::self();
  int volumeFieldId = fieldManager.getFieldId("Volume");
  int modelCoordinatesFieldId = fieldManager.getFieldId("Model_Coordinates");
  int coordinatesFieldId = fieldManager.getFieldId("Coordinates");
  int weightedVolumeFieldId = fieldManager.getFieldId("Weighted_Volume");
  int displacementFieldId = fieldManager.getFieldId("Displacement");
  int velocityFieldId = fieldManager.getFieldId("Velocity");
  int forceDensityFieldId = fieldManager.getFieldId("Force_Density");
  int forceFieldId = fieldManager.getFieldId("Force");
  int dilatationFieldId = fieldManager.getFieldId("Dilatation");
  int damageFieldId = fieldManager.getFieldId("Damage");
  int bondDamageFieldId = fieldManager.getFieldId("Bond_Damage");

  // Make copies of everything so that we can identify any changes
  // that might occur during rebalance (there should be none)
  Epetra_BlockMap oneDimensionalMap(*peridigm->getOneDimensionalMap());
  Epetra_BlockMap oneDimensionalOverlapMap(*peridigm->getOneDimensionalOverlapMap());
  Epetra_BlockMap threeDimensionalMap(*peridigm->getThreeDimensionalMap());
  Epetra_BlockMap bondMap(*peridigm->getBondMap());
  Epetra_Vector initialX(*peridigm->getX());
  Epetra_Vector initialU(*peridigm->getU());
  Epetra_Vector initialY(*peridigm->getY());
  Epetra_Vector initialV(*peridigm->getV());
  Epetra_Vector initialA(*peridigm->getA());
  Epetra_Vector initialForce(*peridigm->getForce());
  Epetra_Vector volume( *peridigm->getBlock(0)->getData(volumeFieldId, PeridigmField::STEP_NONE) );
  Epetra_Vector coord3d( *peridigm->getBlock(0)->getData(modelCoordinatesFieldId, PeridigmField::STEP_NONE)) ;
  Epetra_Vector weightedVolume( *peridigm->getBlock(0)->getData(weightedVolumeFieldId, PeridigmField::STEP_NONE) );
  Epetra_Vector displ3dN( *peridigm->getBlock(0)->getData(displacementFieldId, PeridigmField::STEP_N) );
  Epetra_Vector displ3dNP1( *peridigm->getBlock(0)->getData(displacementFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector curcoord3dN( *peridigm->getBlock(0)->getData(coordinatesFieldId, PeridigmField::STEP_N) );
  Epetra_Vector curcoord3dNP1( *peridigm->getBlock(0)->getData(coordinatesFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector veloc3dN( *peridigm->getBlock(0)->getData(velocityFieldId, PeridigmField::STEP_N) );
  Epetra_Vector veloc3dNP1( *peridigm->getBlock(0)->getData(velocityFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector force3dN( *peridigm->getBlock(0)->getData(forceDensityFieldId, PeridigmField::STEP_N) );
  Epetra_Vector force3dNP1( *peridigm->getBlock(0)->getData(forceDensityFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector dilatationN( *peridigm->getBlock(0)->getData(dilatationFieldId, PeridigmField::STEP_N) );
  Epetra_Vector dilatationNP1( *peridigm->getBlock(0)->getData(dilatationFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector damageN( *peridigm->getBlock(0)->getData(damageFieldId, PeridigmField::STEP_N) );
  Epetra_Vector damageNP1( *peridigm->getBlock(0)->getData(damageFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector bondDamageN( *peridigm->getBlock(0)->getData(bondDamageFieldId, PeridigmField::STEP_N) );
  Epetra_Vector bondDamageNP1( *peridigm->getBlock(0)->getData(bondDamageFieldId, PeridigmField::STEP_NP1) );
  PeridigmNS::NeighborhoodData neighborhoodData(*peridigm->getGlobalNeighborhoodData());

  // call the rebalance function, which should produce no changes in serial
  peridigm->rebalance();

  // check everything to make sure nothing changed
  // check maps
  BOOST_CHECK(peridigm->getOneDimensionalMap()->SameAs(oneDimensionalMap));
  BOOST_CHECK(peridigm->getOneDimensionalOverlapMap()->SameAs(oneDimensionalOverlapMap));
  BOOST_CHECK(peridigm->getThreeDimensionalMap()->SameAs(threeDimensionalMap));
  BOOST_CHECK(peridigm->getBondMap()->SameAs(bondMap));
  // check mothership vectors
  for(int i=0 ; i<initialX.MyLength(); ++i){
    BOOST_CHECK_CLOSE(initialX[i], (*peridigm->getX())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialU[i], (*peridigm->getU())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialY[i], (*peridigm->getY())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialV[i], (*peridigm->getV())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialA[i], (*peridigm->getA())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialForce[i], (*peridigm->getForce())[i], 1.0e-15);
  }
  // check field data
  Teuchos::RCP<PeridigmNS::Block> block = peridigm->getBlock(0);
  for(int i=0 ; i<block->getData(volumeFieldId, PeridigmField::STEP_NONE)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(volume[i], (*block->getData(volumeFieldId, PeridigmField::STEP_NONE))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(modelCoordinatesFieldId, PeridigmField::STEP_NONE)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(coord3d[i], (*block->getData(modelCoordinatesFieldId, PeridigmField::STEP_NONE))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(weightedVolumeFieldId, PeridigmField::STEP_NONE)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(weightedVolume[i], (*block->getData(weightedVolumeFieldId, PeridigmField::STEP_NONE))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(displacementFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(displ3dN[i], (*block->getData(displacementFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(displacementFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(displ3dNP1[i], (*block->getData(displacementFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(coordinatesFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(curcoord3dN[i], (*block->getData(coordinatesFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(coordinatesFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(curcoord3dNP1[i], (*block->getData(coordinatesFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(velocityFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(veloc3dN[i], (*block->getData(velocityFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(velocityFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(veloc3dNP1[i], (*block->getData(velocityFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(forceFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(force3dN[i], (*block->getData(forceFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(forceFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(force3dNP1[i], (*block->getData(forceFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(dilatationFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(dilatationN[i], (*block->getData(dilatationFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(dilatationFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(dilatationNP1[i], (*block->getData(dilatationFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(damageFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(damageN[i], (*block->getData(damageFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(damageFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(damageNP1[i], (*block->getData(damageFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(bondDamageFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(bondDamageN[i], (*block->getData(bondDamageFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(bondDamageFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(bondDamageNP1[i], (*block->getData(bondDamageFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  // check neighborhood data
  BOOST_CHECK_EQUAL(neighborhoodData.NumOwnedPoints(), peridigm->getGlobalNeighborhoodData()->NumOwnedPoints());
  BOOST_CHECK_EQUAL(neighborhoodData.NeighborhoodListSize(), peridigm->getGlobalNeighborhoodData()->NeighborhoodListSize());
  for(int i=0 ; i<peridigm->getGlobalNeighborhoodData()->NumOwnedPoints() ; ++i){
    BOOST_CHECK_EQUAL(neighborhoodData.OwnedIDs()[i], peridigm->getGlobalNeighborhoodData()->OwnedIDs()[i]);
    BOOST_CHECK_EQUAL(neighborhoodData.NeighborhoodPtr()[i], peridigm->getGlobalNeighborhoodData()->NeighborhoodPtr()[i]);
  }
  for(int i=0 ; i<peridigm->getGlobalNeighborhoodData()->NeighborhoodListSize() ; ++i){
    BOOST_CHECK_EQUAL(neighborhoodData.NeighborhoodList()[i], peridigm->getGlobalNeighborhoodData()->NeighborhoodList()[i]);
  }
}

//! This is a seial rebalance test; the rebalance should have no effect.
void rebalanceFourPointModel()
{
  Teuchos::RCP<PeridigmNS::Peridigm> peridigm = createFourPointModel();

  PeridigmNS::FieldManager& fieldManager = PeridigmNS::FieldManager::self();
  int volumeFieldId = fieldManager.getFieldId("Volume");
  int modelCoordinatesFieldId = fieldManager.getFieldId("Model_Coordinates");
  int coordinatesFieldId = fieldManager.getFieldId("Coordinates");
  int weightedVolumeFieldId = fieldManager.getFieldId("Weighted_Volume");
  int displacementFieldId = fieldManager.getFieldId("Displacement");
  int velocityFieldId = fieldManager.getFieldId("Velocity");
  int forceDensityFieldId = fieldManager.getFieldId("Force_Density");
  int forceFieldId = fieldManager.getFieldId("Force");
  int dilatationFieldId = fieldManager.getFieldId("Dilatation");
  int damageFieldId = fieldManager.getFieldId("Damage");
  int bondDamageFieldId = fieldManager.getFieldId("Bond_Damage");

  // Make copies of everything so that we can identify any changes
  // that might occur during rebalance (there should be none)
  Epetra_BlockMap oneDimensionalMap(*peridigm->getOneDimensionalMap());
  Epetra_BlockMap oneDimensionalOverlapMap(*peridigm->getOneDimensionalOverlapMap());
  Epetra_BlockMap threeDimensionalMap(*peridigm->getThreeDimensionalMap());
  Epetra_BlockMap bondMap(*peridigm->getBondMap());
  Epetra_Vector initialX(*peridigm->getX());
  Epetra_Vector initialU(*peridigm->getU());
  Epetra_Vector initialY(*peridigm->getY());
  Epetra_Vector initialV(*peridigm->getV());
  Epetra_Vector initialA(*peridigm->getA());
  Epetra_Vector initialForce(*peridigm->getForce());
  Epetra_Vector volume( *peridigm->getBlock(0)->getData(volumeFieldId, PeridigmField::STEP_NONE) );
  Epetra_Vector coord3d( *peridigm->getBlock(0)->getData(modelCoordinatesFieldId, PeridigmField::STEP_NONE) );
  Epetra_Vector weightedVolume( *peridigm->getBlock(0)->getData(weightedVolumeFieldId, PeridigmField::STEP_NONE) );
  Epetra_Vector displ3dN( *peridigm->getBlock(0)->getData(displacementFieldId, PeridigmField::STEP_N) );
  Epetra_Vector displ3dNP1( *peridigm->getBlock(0)->getData(displacementFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector curcoord3dN( *peridigm->getBlock(0)->getData(coordinatesFieldId, PeridigmField::STEP_N) );
  Epetra_Vector curcoord3dNP1( *peridigm->getBlock(0)->getData(coordinatesFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector veloc3dN( *peridigm->getBlock(0)->getData(velocityFieldId, PeridigmField::STEP_N) );
  Epetra_Vector veloc3dNP1( *peridigm->getBlock(0)->getData(velocityFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector force3dN( *peridigm->getBlock(0)->getData(forceFieldId, PeridigmField::STEP_N) );
  Epetra_Vector force3dNP1( *peridigm->getBlock(0)->getData(forceFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector dilatationN( *peridigm->getBlock(0)->getData(dilatationFieldId, PeridigmField::STEP_N) );
  Epetra_Vector dilatationNP1( *peridigm->getBlock(0)->getData(dilatationFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector damageN( *peridigm->getBlock(0)->getData(damageFieldId, PeridigmField::STEP_N) );
  Epetra_Vector damageNP1( *peridigm->getBlock(0)->getData(damageFieldId, PeridigmField::STEP_NP1) );
  Epetra_Vector bondDamageN( *peridigm->getBlock(0)->getData(bondDamageFieldId, PeridigmField::STEP_N) );
  Epetra_Vector bondDamageNP1( *peridigm->getBlock(0)->getData(bondDamageFieldId, PeridigmField::STEP_NP1) );
  PeridigmNS::NeighborhoodData neighborhoodData(*peridigm->getGlobalNeighborhoodData());

  // call the rebalance function, which should produce no changes in serial
  peridigm->rebalance();

  // check everything to make sure nothing changed
  // check maps
  BOOST_CHECK(peridigm->getOneDimensionalMap()->SameAs(oneDimensionalMap));
  BOOST_CHECK(peridigm->getOneDimensionalOverlapMap()->SameAs(oneDimensionalOverlapMap));
  BOOST_CHECK(peridigm->getThreeDimensionalMap()->SameAs(threeDimensionalMap));
  BOOST_CHECK(peridigm->getBondMap()->SameAs(bondMap));
  // check mothership vectors
  for(int i=0 ; i<initialX.MyLength(); ++i){
    BOOST_CHECK_CLOSE(initialX[i], (*peridigm->getX())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialU[i], (*peridigm->getU())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialY[i], (*peridigm->getY())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialV[i], (*peridigm->getV())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialA[i], (*peridigm->getA())[i], 1.0e-15);
    BOOST_CHECK_CLOSE(initialForce[i], (*peridigm->getForce())[i], 1.0e-15);
  }
  // check field data
  Teuchos::RCP<PeridigmNS::Block> block = peridigm->getBlock(0);
  for(int i=0 ; i<block->getData(volumeFieldId, PeridigmField::STEP_NONE)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(volume[i], (*block->getData(volumeFieldId, PeridigmField::STEP_NONE))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(modelCoordinatesFieldId, PeridigmField::STEP_NONE)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(coord3d[i], (*block->getData(modelCoordinatesFieldId, PeridigmField::STEP_NONE))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(weightedVolumeFieldId, PeridigmField::STEP_NONE)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(weightedVolume[i], (*block->getData(weightedVolumeFieldId, PeridigmField::STEP_NONE))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(displacementFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(displ3dN[i], (*block->getData(displacementFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(displacementFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(displ3dNP1[i], (*block->getData(displacementFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(coordinatesFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(curcoord3dN[i], (*block->getData(coordinatesFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(coordinatesFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(curcoord3dNP1[i], (*block->getData(coordinatesFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(velocityFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(veloc3dN[i], (*block->getData(velocityFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(velocityFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(veloc3dNP1[i], (*block->getData(velocityFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(forceDensityFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(force3dN[i], (*block->getData(forceDensityFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(forceDensityFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(force3dNP1[i], (*block->getData(forceDensityFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(dilatationFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(dilatationN[i], (*block->getData(dilatationFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(dilatationFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(dilatationNP1[i], (*block->getData(dilatationFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(damageFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(damageN[i], (*block->getData(damageFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(damageFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(damageNP1[i], (*block->getData(damageFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(bondDamageFieldId, PeridigmField::STEP_N)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(bondDamageN[i], (*block->getData(bondDamageFieldId, PeridigmField::STEP_N))[i], 1.0e-15);
  for(int i=0 ; i<block->getData(bondDamageFieldId, PeridigmField::STEP_NP1)->MyLength() ; ++i)
    BOOST_CHECK_CLOSE(bondDamageNP1[i], (*block->getData(bondDamageFieldId, PeridigmField::STEP_NP1))[i], 1.0e-15);
  // check neighborhood data
  BOOST_CHECK_EQUAL(neighborhoodData.NumOwnedPoints(), peridigm->getGlobalNeighborhoodData()->NumOwnedPoints());
  BOOST_CHECK_EQUAL(neighborhoodData.NeighborhoodListSize(), peridigm->getGlobalNeighborhoodData()->NeighborhoodListSize());
  for(int i=0 ; i<peridigm->getGlobalNeighborhoodData()->NumOwnedPoints() ; ++i){
    BOOST_CHECK_EQUAL(neighborhoodData.OwnedIDs()[i], peridigm->getGlobalNeighborhoodData()->OwnedIDs()[i]);
    BOOST_CHECK_EQUAL(neighborhoodData.NeighborhoodPtr()[i], peridigm->getGlobalNeighborhoodData()->NeighborhoodPtr()[i]);
  }
  for(int i=0 ; i<peridigm->getGlobalNeighborhoodData()->NeighborhoodListSize() ; ++i){
    BOOST_CHECK_EQUAL(neighborhoodData.NeighborhoodList()[i], peridigm->getGlobalNeighborhoodData()->NeighborhoodList()[i]);
  }
}

bool init_unit_test_suite()
{
	// Add a suite for each processor in the test
	bool success = true;

	test_suite* proc = BOOST_TEST_SUITE("utPeridigm");
	proc->add(BOOST_TEST_CASE(&initialize));
	proc->add(BOOST_TEST_CASE(&rebalanceTwoPointModel));
	proc->add(BOOST_TEST_CASE(&rebalanceFourPointModel));
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
