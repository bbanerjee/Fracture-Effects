/*! \file Peridigm_Compute_Neighborhood_Volume.cpp */

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

#include <vector>

#include "Peridigm_Compute_Neighborhood_Volume.hpp"
#include "Peridigm_Field.hpp"

PeridigmNS::Compute_Neighborhood_Volume::Compute_Neighborhood_Volume(Teuchos::RCP<const Teuchos::ParameterList> params,
                                                                     Teuchos::RCP<const Epetra_Comm> epetraComm_)
  : Compute(params, epetraComm_), m_volumeFieldId(-1), m_partialVolumeFieldId(-1), m_neighborhoodVolumeFieldId(-1)
{
  FieldManager& fieldManager = FieldManager::self();
  m_volumeFieldId = fieldManager.getFieldId("Volume");
  m_partialVolumeFieldId = fieldManager.getFieldId(PeridigmField::BOND, PeridigmField::SCALAR, PeridigmField::CONSTANT, "Partial_Volume");
  m_neighborhoodVolumeFieldId = fieldManager.getFieldId(PeridigmField::ELEMENT, PeridigmField::SCALAR, PeridigmField::CONSTANT, "Neighborhood_Volume");
  m_fieldIds.push_back(m_volumeFieldId);
  m_fieldIds.push_back(m_partialVolumeFieldId);
  m_fieldIds.push_back(m_neighborhoodVolumeFieldId);
}

PeridigmNS::Compute_Neighborhood_Volume::~Compute_Neighborhood_Volume(){}

void PeridigmNS::Compute_Neighborhood_Volume::initialize( Teuchos::RCP< std::vector<PeridigmNS::Block> > blocks ) {

  std::vector<Block>::iterator blockIt;
  for(blockIt = blocks->begin() ; blockIt != blocks->end() ; blockIt++){
    Teuchos::RCP<NeighborhoodData> neighborhoodData = blockIt->getNeighborhoodData();
    const int numOwnedPoints = neighborhoodData->NumOwnedPoints();
    const int* neighborhoodList = neighborhoodData->NeighborhoodList();
    double *volume, *neighborhoodVolume;
    blockIt->getData(m_volumeFieldId, PeridigmField::STEP_NONE)->ExtractView(&volume);
    blockIt->getData(m_neighborhoodVolumeFieldId, PeridigmField::STEP_NONE)->ExtractView(&neighborhoodVolume);

    double *partialVolume = 0;
    if( blockIt->hasData(m_partialVolumeFieldId, PeridigmField::STEP_NONE) )
      blockIt->getData(m_partialVolumeFieldId, PeridigmField::STEP_NONE)->ExtractView(&partialVolume);

    int neighborhoodListIndex = 0;
    int bondIndex = 0;
    for(int iID=0 ; iID<numOwnedPoints ; ++iID){
      int numNeighbors = neighborhoodList[neighborhoodListIndex++];

      // Assume that the cell's volume is within its neighborhood
      neighborhoodVolume[iID] = volume[iID];

      for(int iNID=0 ; iNID<numNeighbors ; ++iNID){
        int neighborID = neighborhoodList[neighborhoodListIndex++];
        double neighborVolume = volume[neighborID];
        double fraction = 1.0;
        if(partialVolume != 0)
          fraction = partialVolume[bondIndex++];
        neighborhoodVolume[iID] += neighborVolume*fraction;
      }
    }
  }

}

int PeridigmNS::Compute_Neighborhood_Volume::compute( Teuchos::RCP< std::vector<PeridigmNS::Block> > blocks ) const {
  return 0;
}
