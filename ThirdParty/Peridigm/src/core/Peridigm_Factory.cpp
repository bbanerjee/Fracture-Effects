/*! \file Peridigm_Factory.cpp */

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

#include <Teuchos_RCP.hpp>
#include <Teuchos_XMLParameterListHelpers.hpp>

#include "Peridigm_Factory.hpp"
#include "Peridigm.hpp"

PeridigmNS::PeridigmFactory::PeridigmFactory(){}

Teuchos::RCP<PeridigmNS::Peridigm> PeridigmNS::PeridigmFactory::create(const std::string inputFile, const MPI_Comm& peridigmComm)
{
  using Teuchos::RCP;
  using Teuchos::rcp;

  Teuchos::RCP<Epetra_Comm> comm;
  #ifdef HAVE_MPI
    comm = rcp(new Epetra_MpiComm(peridigmComm));
  #else
    comm = rcp(new Epetra_SerialComm);
  #endif

  // Set application parameters to default values
  Teuchos::RCP<Teuchos::ParameterList> peridigmParams = rcp(new Teuchos::ParameterList());
  setPeridigmParamDefaults(peridigmParams.ptr());
  setSolverParamDefaults(peridigmParams.ptr());

  // Update parameters with data from xml file
  Teuchos::Ptr<Teuchos::ParameterList> peridigmParamsPtr(peridigmParams.get());
  Teuchos::updateParametersFromXmlFile(inputFile, peridigmParamsPtr);

  // Create new Peridigm object
  return rcp(new PeridigmNS::Peridigm(comm, peridigmParams));

}

void PeridigmNS::PeridigmFactory::setPeridigmParamDefaults(Teuchos::Ptr<Teuchos::ParameterList> peridigmParams_)
{
  peridigmParams_->set("Verbose", false);
}

void PeridigmNS::PeridigmFactory::setSolverParamDefaults(Teuchos::Ptr<Teuchos::ParameterList> peridigmParams_)
{
  Teuchos::ParameterList& solverParams = peridigmParams_->sublist("Solver");

  // general settings
  solverParams.set("Verbose", false);
}
