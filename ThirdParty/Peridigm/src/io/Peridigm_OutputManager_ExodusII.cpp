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

#include <fstream>
#include <time.h>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>

#include <netcdf.h>
#include <exodusII.h>

#include <Epetra_Comm.h>
#include "Teuchos_StandardParameterEntryValidators.hpp"
#include <Teuchos_Assert.hpp>

#include "Peridigm.hpp"
#include "Peridigm_OutputManager_ExodusII.hpp"
#include "Peridigm_Field.hpp"

PeridigmNS::OutputManager_ExodusII::OutputManager_ExodusII(const Teuchos::RCP<Teuchos::ParameterList>& params, 
                                                           PeridigmNS::Peridigm *peridigm_,
                                                           Teuchos::RCP< std::vector<PeridigmNS::Block> > blocks) 
  : peridigm(peridigm_) {
  
  // No input to validate; no output requested
  iWrite = true;
  if (params == Teuchos::null) {
    iWrite = false;
    return;
  }

  // Throws exception if parameters not present or of wrong type
  // Teuchos::ParameterList validator can't validate all input -- it mainly checks for presence of invalid input and invalid input types
  // Additional checking needed below 
  Teuchos::ParameterList validParameterList = getValidParameterList();
  bool isValid = true;
  try {
    params->validateParameters(validParameterList);
  }
  catch(Teuchos::Exceptions::InvalidParameterName &excpt)  {std::cout<<excpt.what(); isValid=false;}
  catch(Teuchos::Exceptions::InvalidParameterType &excpt)  {std::cout<<excpt.what(); isValid=false;}
  catch(Teuchos::Exceptions::InvalidParameterValue &excpt) {std::cout<<excpt.what(); isValid=false;}
  catch(...) {isValid=false;}
  if (!isValid){
    std::cout.flush();
    TEUCHOS_TEST_FOR_EXCEPTION(1, std::invalid_argument, "PeridigmNS::OutputManager_ExodusII:::OutputManager_ExodusII() -- Invalid parameter, type or value.");
  }

  try {
    numProc = params->INVALID_TEMPLATE_QUALIFIER get<int>("NumProc");
  }
  catch ( const std::exception& e) {
    TEUCHOS_TEST_FOR_EXCEPTION(1, std::invalid_argument, "PeridigmNS::OutputManager_ExodusII:::OutputManager_ExodusII() -- numProc not present.");
  }

  try {
    myPID = params->INVALID_TEMPLATE_QUALIFIER get<int>("MyPID");
  }
  catch ( const std::exception& e) {
    TEUCHOS_TEST_FOR_EXCEPTION(1,  std::invalid_argument, "PeridigmNS::OutputManager_ExodusII:::OutputManager_ExodusII() -- MyPID not present.");
  }

  // Default to no output
  frequency = params->get<int>("Output Frequency",-1); 

  // Default to BINARY output
  outputFormat = params->get<string>("Output Format","BINARY"); 
  TEUCHOS_TEST_FOR_EXCEPTION( outputFormat != "BINARY",  std::invalid_argument, "PeridigmNS::OutputManager_ExodusII:::OutputManager_ExodusII() -- Output format must be BINARY for ExodusII.");

  // Default to not write full neighborlist
  writeNeighborlist = params->get<bool>("Bond Family",false); 
  TEUCHOS_TEST_FOR_EXCEPTION( (numProc != 1) && (writeNeighborlist),  std::invalid_argument, "PeridigmNS::OutputManager_ExodusII:::OutputManager_ExodusII() -- Parallel write of bond families not currently supported.");

  // Output filename base
  filenameBase = params->get<string>("Output Filename","dump"); 

  // User-requested fields for output 
  outputVariables = sublist(params, "Output Variables");

  // Determine if the database will contain only global data
  globalDataOnly = true;
  for (Teuchos::ParameterList::ConstIterator it = outputVariables->begin(); it != outputVariables->end(); ++it) {
    string name = it->first;
    PeridigmNS::FieldSpec spec = PeridigmNS::FieldManager::self().getFieldSpec(name);
    if (spec.getRelation() != PeridigmField::GLOBAL)
      globalDataOnly = false;
  }

  // Initialize count (number of times write() has been called)
  // Initialize exodusCount (number of timesteps data actually written to exodus file)
  // Initialize to 0 because first call to write() corresponds to timestep 1
  exodusCount = count = 0;

  // Sentinal value for file handle
  file_handle = -1;

  // Rebalance count 
  rebalanceCount = 0;

  // Number of times new Exodus database written
  newDatabaseCount = 0;

  // Default to storing and writing doubles
  CPU_word_size = IO_word_size = sizeof(double);
  
  // Not called yet
  initializeExodusDatabaseCalled = false;

  // Initialize the exodus database
  // initializeExodusDatabase(blocks);
}

Teuchos::ParameterList PeridigmNS::OutputManager_ExodusII::getValidParameterList() {

  // prevent Teuchos from converting parameter types
  Teuchos::AnyNumberParameterEntryValidator::AcceptedTypes intParam(false), dblParam(false), strParam(false);
  intParam.allowInt(true);
  dblParam.allowDouble(true);
  strParam.allowString(true);

  // Construct a ParameterList containing valid enteries for Output
  Teuchos::ParameterList validParameterList("Output");
  setIntParameter("MyPID",0,"Process ID",&validParameterList,intParam);
  setIntParameter("NumProc",0,"Number of Process IDs",&validParameterList,intParam);
  validParameterList.set("Output File Type","ExodusII");
  validParameterList.set("Output Filename","dump");
  Teuchos::setStringToIntegralParameter<int>("Output Format","BINARY","ASCII or BINARY",Teuchos::tuple<string>("ASCII","BINARY"),&validParameterList);
  setIntParameter("Output Frequency",-1,"Frequency of Output",&validParameterList,intParam);
  validParameterList.set("Parallel Write",true);

  // Create a vector of valid output variables
  // Do not include bond data, since we can not output it
  std::vector<PeridigmNS::FieldSpec> validOutputFieldSpecs;
  FieldManager& fieldManager = FieldManager::self();
  std::vector<PeridigmNS::FieldSpec> allFieldSpecs = fieldManager.getFieldSpecs();
  for(std::vector<PeridigmNS::FieldSpec>::iterator it = allFieldSpecs.begin() ; it != allFieldSpecs.end() ; it++){
    if(it->getRelation() != PeridigmField::BOND)
      validOutputFieldSpecs.push_back(*it);
  }

  // Add in any remaining field specs that are known to be valid
  procNumFieldId = fieldManager.getFieldId("Proc_Num");
  elementIdFieldId = fieldManager.getFieldId("Element_Id");
  validOutputFieldSpecs.push_back(fieldManager.getFieldSpec(procNumFieldId));
  validOutputFieldSpecs.push_back(fieldManager.getFieldSpec(elementIdFieldId));

  // Sort and remove duplicates for consistency
  std::sort(validOutputFieldSpecs.begin(), validOutputFieldSpecs.end());
  std::vector<PeridigmNS::FieldSpec>::iterator newEnd = std::unique(validOutputFieldSpecs.begin(), validOutputFieldSpecs.end());
  validOutputFieldSpecs.erase(newEnd, validOutputFieldSpecs.end());

  // Convert the vector into a ParameterList and append it to validParametersList
  Teuchos::ParameterList& validOutputVariablesParameterList = validParameterList.sublist("Output Variables");
  for(unsigned int i=0 ; i<validOutputFieldSpecs.size() ; ++i)
    validOutputVariablesParameterList.set(validOutputFieldSpecs[i].getLabel(), false);

  return validParameterList;
}

PeridigmNS::OutputManager_ExodusII::~OutputManager_ExodusII() {
}

void PeridigmNS::OutputManager_ExodusII::write(Teuchos::RCP< std::vector<PeridigmNS::Block> > blocks, double current_time) {

  if (!iWrite) return;

  // increment index count
  count = count + 1;

  // Only write if frequency count match
  if (frequency<=0 || (count-1)%frequency!=0) return;

  // increment exodus_count index
  exodusCount = exodusCount + 1;

  // Call compute manager; Updated any computed quantities before write
  peridigm->computeManager->compute(blocks);

  // If the database contains only global data, then it will be output only by the root processor
  if (globalDataOnly && myPID != 0)
    return;

  // Initialize new exodus database if needed
  // Each block is always rebalanced at the same time, so each datamanager should always return the same
  // rebalance count. Hence, we keep only a single static int for the rebalance count. If the first block 
  // rebalanced since last write, then all of them did. Force reinit database.
  if ( (numProc > 1) && (rebalanceCount != blocks->begin()->getDataManager()->getRebalanceCount()) ) {
    rebalanceCount = blocks->begin()->getDataManager()->getRebalanceCount();
    // Rebalance does not affect databases containing only global data
    if(!globalDataOnly){
      initializeExodusDatabase(blocks);
      exodusCount = 1;
    }
  }

  // If first call, intialize database
  if (!initializeExodusDatabaseCalled) {
    if(globalDataOnly)
      initializeExodusDatabaseWithOnlyGlobalData(blocks);
    else
      initializeExodusDatabase(blocks);
  }

  // Open exodus database for writing
  float version;
  file_handle = ex_open(filename.str().c_str(), EX_WRITE, &CPU_word_size,&IO_word_size, &version);
  if (file_handle < 0) reportExodusError(file_handle, "write", "ex_open");

  // Write time value
  int retval = ex_put_time(file_handle,exodusCount,&current_time);
  if (retval!= 0) reportExodusError(retval, "write", "ex_put_time");

  int num_nodes(1);
  if(!globalDataOnly)
    num_nodes = peridigm->getOneDimensionalMap()->NumMyElements();

  // Allocate temporary storage for all mothership-like data
  std::vector<double> x_vec(num_nodes), y_vec(num_nodes), z_vec(num_nodes);
  double *xptr = &x_vec[0];
  double *yptr = &y_vec[0];
  double *zptr = &z_vec[0];

  // allocate temporate storage for globals
  int num_global_vars = global_output_field_map.size();
  std::vector<double> globals_vec(num_global_vars);
  double *globals = &globals_vec[0];
  unsigned int globalsIndex = 0;

  for (Teuchos::ParameterList::ConstIterator it = outputVariables->begin(); it != outputVariables->end(); ++it) {

    string name = it->first;
    PeridigmNS::FieldSpec spec = PeridigmNS::FieldManager::self().getFieldSpec(name);

    double *block_ptr = NULL;
    if (spec.getRelation() == PeridigmField::GLOBAL) {
      // global vars are static within a block, so only need to reference first block
      if (spec.getLength() == PeridigmField::SCALAR) {
        TEUCHOS_TEST_FOR_EXCEPTION(globalsIndex >= globals_vec.size(), std::invalid_argument, "PeridigmNS::OutputManager_ExodusII::write() -- error writing global variable.");
        if (spec.getTemporal() == PeridigmField::CONSTANT)
          globals[globalsIndex++] = (*(blocks->begin()->getData(spec.getId(), PeridigmField::STEP_NONE)))[0];
        else
          globals[globalsIndex++] = (*(blocks->begin()->getData(spec.getId(), PeridigmField::STEP_NP1)))[0];
      }
      else if (spec.getLength() == PeridigmField::VECTOR) {
        TEUCHOS_TEST_FOR_EXCEPTION(globalsIndex >= globals_vec.size(), std::invalid_argument, "PeridigmNS::OutputManager_ExodusII::write() -- error writing global variable.");
        if (spec.getTemporal() == PeridigmField::CONSTANT){
          globals[globalsIndex++] = (*(blocks->begin()->getData(spec.getId(), PeridigmField::STEP_NONE)))[0];
          globals[globalsIndex++] = (*(blocks->begin()->getData(spec.getId(), PeridigmField::STEP_NONE)))[1];
          globals[globalsIndex++] = (*(blocks->begin()->getData(spec.getId(), PeridigmField::STEP_NONE)))[2];
        }
        else{
          globals[globalsIndex++] = (*(blocks->begin()->getData(spec.getId(), PeridigmField::STEP_NP1)))[0];
          globals[globalsIndex++] = (*(blocks->begin()->getData(spec.getId(), PeridigmField::STEP_NP1)))[1];
          globals[globalsIndex++] = (*(blocks->begin()->getData(spec.getId(), PeridigmField::STEP_NP1)))[2];
        }
      }
      else {
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "PeridigmNS::OutputManager_ExodusII::write() -- unsupported global type (must be scalar or vector).");
      }
      retval = ex_put_glob_vars(file_handle, exodusCount, num_global_vars, globals);
      if (retval!= 0) reportExodusError(retval, "write", "ex_put_glob_vars");
    }
    // Exodus ignores element blocks when writing nodal variables
    else if (spec.getRelation() == PeridigmField::NODE) {
      // Loop over all blocks, copying data from each block into mothership-like vector
      std::vector<PeridigmNS::Block>::iterator blockIt;
      for(blockIt = blocks->begin(); blockIt != blocks->end() ; blockIt++) {
        Teuchos::RCP<Epetra_Vector> epetra_vector;
        PeridigmField::Step step = PeridigmField::STEP_NONE;
        if(spec.getTemporal() == PeridigmField::TWO_STEP)
          step = PeridigmField::STEP_NP1;
        epetra_vector = blockIt->getData(spec.getId(), step);
        int block_num_nodes = (blockIt->getDataManager()->getOwnedScalarPointMap())->NumMyElements();
        epetra_vector->ExtractView(&block_ptr);
        // switch on dimension of data
        if (spec.getLength() == PeridigmField::SCALAR) {
          // loop over contents of block vector; fill mothership-like vector
          for (int j=0;j<block_num_nodes; j++) {
            int GID = blockIt->getOwnedVectorPointMap()->GID(j);
            int msLID = peridigm->getOneDimensionalMap()->LID(GID);
            xptr[msLID] = block_ptr[j];
          }
        }
        else if (spec.getLength() == PeridigmField::VECTOR) {
          // loop over contents of block vector; fill mothership-like vector
          for (int j=0;j<block_num_nodes; j++) {
            int GID = blockIt->getOwnedVectorPointMap()->GID(j);
            int msLID = peridigm->getThreeDimensionalMap()->LID(GID);
            xptr[msLID] = block_ptr[3*j];
            yptr[msLID] = block_ptr[3*j+1];
            zptr[msLID] = block_ptr[3*j+2];
          }
        } // end switch on data dimension
      } // end loop over blocks
      // Mothership-like vectors filled now; pass data to exodus database (switch again on dimension of data)
      if (spec.getLength() == PeridigmField::SCALAR) {
        retval = ex_put_nodal_var(file_handle, exodusCount, node_output_field_map[name], num_nodes, xptr);
        if (retval!= 0) reportExodusError(retval, "write", "ex_put_nodal_var");
      }
      else if (spec.getLength() == PeridigmField::VECTOR) {
        // Writing all vector output as per-node data
        string tmpnameX = name+"X";
        string tmpnameY = name+"Y";
        string tmpnameZ = name+"Z";
        retval = ex_put_nodal_var(file_handle, exodusCount, node_output_field_map[tmpnameX], num_nodes, xptr);
        if (retval!= 0) reportExodusError(retval, "write", "ex_put_nodal_var");
        retval = ex_put_nodal_var(file_handle, exodusCount, node_output_field_map[tmpnameY], num_nodes, yptr);
        if (retval!= 0) reportExodusError(retval, "write", "ex_put_nodal_var");
        retval = ex_put_nodal_var(file_handle, exodusCount, node_output_field_map[tmpnameZ], num_nodes, zptr);
        if (retval!= 0) reportExodusError(retval, "write", "ex_put_nodal_var");
      }
    } // end if per-node variable
    // Exodus wants element data written individually for each element block
    else if (spec.getRelation() == PeridigmField::ELEMENT) {
      // Loop over all blocks, passing data from each block to exodus database
      std::vector<PeridigmNS::Block>::iterator blockIt;
      for(blockIt = blocks->begin(); blockIt != blocks->end() ; blockIt++) {
        int block_num_nodes = (blockIt->getDataManager()->getOwnedScalarPointMap())->NumMyElements();
        if (block_num_nodes == 0) continue; // Don't write data for empty blocks
        if (spec.getId() == elementIdFieldId) { // Handle special case of ID (int type)
          for (int j=0; j<block_num_nodes; j++)
            xptr[j] = (double)(((blockIt->getDataManager()->getOwnedScalarPointMap())->GID(j))+1);
          retval = ex_put_elem_var(file_handle, exodusCount, element_output_field_map[name], blockIt->getID(), block_num_nodes, xptr);
          if (retval!= 0) reportExodusError(retval, "write", "ex_put_elem_var");
        }
        else if (spec.getId() == procNumFieldId) { // Handle special case of Proc_Num (int type)
          for (int j=0; j<block_num_nodes; j++)
            xptr[j] = (double)myPID;
          retval = ex_put_elem_var(file_handle, exodusCount, element_output_field_map[name], blockIt->getID(), block_num_nodes, xptr);
          if (retval!= 0) reportExodusError(retval, "write", "ex_put_elem_var");
        }
        else {
          Teuchos::RCP<Epetra_Vector> epetra_vector;
          PeridigmField::Step step = PeridigmField::STEP_NONE;
          if(spec.getTemporal() == PeridigmField::TWO_STEP)
            step = PeridigmField::STEP_NP1;
          if( blockIt->hasData(spec.getId(), step) ) {
            epetra_vector = blockIt->getData(spec.getId(), step);
            epetra_vector->ExtractView(&block_ptr);
            // switch on dimension of data
            if (spec.getLength() == PeridigmField::SCALAR) {
              retval = ex_put_elem_var(file_handle, exodusCount, element_output_field_map[name], blockIt->getID(), block_num_nodes, block_ptr);
              if (retval!= 0) reportExodusError(retval, "write", "ex_put_elem_var");
            }
            else if (spec.getLength() == PeridigmField::VECTOR) {
              // no 3D vector element data currently implemented in Peridgm.
              TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "PeridigmNS::OutputManager_ExodusII::write() -- elementwise 3D vector quantities not currently supported.");
            } // end switch on data dimension
          }
        }
      } // end loop over blocks
    } // if per-element variable
  }

  // Flush write
  retval = ex_update(file_handle);
  if (retval!= 0) reportExodusError(retval, "write", "ex_update");
  retval = ex_close(file_handle);
  if (retval!= 0) reportExodusError(retval, "write", "ex_close");
}

void PeridigmNS::OutputManager_ExodusII::initializeExodusDatabase(Teuchos::RCP< std::vector<PeridigmNS::Block> > blocks) {

  /*
   * Determine name of output file
   */

  // Follow convention of replacing spaces or . with underscore
  if (!initializeExodusDatabaseCalled) {
    int warningFlag = 0;
    string outString;
    outString.append("\n\n***WARNING***\n");
    outString.append("PeridigmNS::OutputManager_ExodusII:::OutputManager_ExodusII() -- Avoid use of filenames containing '.' (period) and ' ' (space) with ExodusII.\n");
    outString.append("Changing ");
    outString.append(filenameBase);
    outString.append(" to ");
    for ( unsigned int i = 0; i < filenameBase.length(); i++) {
      if (filenameBase[i] ==' ' || filenameBase[i]=='.')  {
        filenameBase.replace(i,1,"_");
        warningFlag = 1;
      }
    }
    outString.append(filenameBase);
    outString.append(".\n\n\n");
    if (warningFlag) std::cout << outString;
    initializeExodusDatabaseCalled = true;
  }

  // Construct output filename
  filename.str(std::string());
  filename.clear();
  if (numProc > 1) {
    filename << filenameBase.c_str();
    // determine number of zeros to use when padding filenames
    std::ostringstream tmpstr;
    tmpstr << numProc;
    int len = tmpstr.str().length();
    if (peridigm->analysisHasRebalance || peridigm->analysisHasContact) {
      newDatabaseCount = newDatabaseCount + 1;
      filename << "-s" << newDatabaseCount;
    }
    filename << ".e";
    filename << ".";
    filename << std::setfill('0') << std::setw(len) << numProc;
    filename << ".";
    filename << std::setfill('0') << std::setw(len) << myPID;
  }
  else {
    filename << filenameBase.c_str() << ".e";
  }


  /*
   * Now, initialize ExodusII database
   */

  // Default to storing and writing doubles
  int CPU_word_size, IO_word_size;
  CPU_word_size = IO_word_size = sizeof(double);

  // Initialize exodus database; Overwrite any existing file with this name
  file_handle = ex_create(filename.str().c_str(),EX_CLOBBER,&CPU_word_size,&IO_word_size);
  if (file_handle < 0) reportExodusError(file_handle, "OutputManager_ExodusII", "ex_create");

  // clear the maps
  global_output_field_map.clear();
  element_output_field_map.clear();
  node_output_field_map.clear();

  // Initialize the database (assumes that Peridigm mothership vectors created and initialized)
  int num_dimensions = 3;
  int num_nodes = peridigm->getOneDimensionalMap()->NumMyElements();
  int num_elements = num_nodes;
  int num_element_blocks = blocks->size();
  int num_node_sets = 0;
  int num_side_sets = 0;
  // Initialize exodus file with parameters
  int retval = ex_put_init(file_handle,"Peridigm", num_dimensions, num_nodes, num_elements, num_element_blocks, num_node_sets, num_side_sets);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_init");

  // Write nodal coordinate values
  // Exodus requires pointer to x,y,z coordinates of nodes, but Peridigm stores this data using a blockmap, which interleaves the data
  // So, extract and copy the data to temporary storage that can be handed to the exodus api
  double *coord_values;
  peridigm->x->ExtractView( &coord_values );
  int numMyElements = peridigm->x->Map().NumMyElements();
  std::vector<double> xcoord_values_vec(numMyElements), ycoord_values_vec(numMyElements), zcoord_values_vec(numMyElements);
  double *xcoord_values = &xcoord_values_vec[0];
  double *ycoord_values = &ycoord_values_vec[0];
  double *zcoord_values = &zcoord_values_vec[0];
  for( int i=0 ; i<numMyElements ; i++ ) {
    int firstPoint = peridigm->x->Map().FirstPointInElement(i);
    xcoord_values[i] = coord_values[firstPoint];
    ycoord_values[i] = coord_values[firstPoint+1];
    zcoord_values[i] = coord_values[firstPoint+2];
  }
  retval = ex_put_coord(file_handle,xcoord_values,ycoord_values,zcoord_values);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_coord");

  // Write nodal coordinate names to database
  const char *coord_names[3] = {"x", "y", "z"};
  retval = ex_put_coord_names(file_handle,const_cast<char**>(coord_names));
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_coord_names");

  // Write element block parameters
  std::vector<int> num_elem_in_block_vec(blocks->size()), num_nodes_in_elem_vec(blocks->size()), elem_block_ID_vec(blocks->size());
  int *num_elem_in_block = &num_elem_in_block_vec[0];
  int *num_nodes_in_elem = &num_nodes_in_elem_vec[0];
  int *elem_block_ID     = &elem_block_ID_vec[0];
  std::vector<PeridigmNS::Block>::iterator blockIt;
  int i=0;
  for(i=0, blockIt = blocks->begin(); blockIt != blocks->end(); blockIt++, i++) {
    // Use only the number of owned elements
    num_elem_in_block[i] = (blockIt->getDataManager()->getOwnedScalarPointMap())->NumMyElements();
    num_nodes_in_elem[i] = 1; // always using sphere elements
    elem_block_ID[i]     = blockIt->getID();
    retval = ex_put_elem_block(file_handle,elem_block_ID[i],"SPHERE",num_elem_in_block[i],num_nodes_in_elem[i],0);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_elem_block");
  }

  // Write element connectivity
  for(blockIt = blocks->begin(); blockIt != blocks->end(); blockIt++) {
    int numMyElements = blockIt->getOwnedScalarPointMap()->NumMyElements();
    if (numMyElements == 0) continue; // don't insert connectivity info for empty blocks
    std::vector<int> connect_vec(numMyElements);
    int *connect = &connect_vec[0];
    for (int j=0;j<numMyElements;j++) {
      int GID = blockIt->getOwnedScalarPointMap()->GID(j);
      connect[j] = peridigm->getOneDimensionalMap()->LID(GID)+1;
    }
    retval = ex_put_elem_conn(file_handle, blockIt->getID(), connect);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_elem_conn");
  }

  // Write global node number map (global node IDs)
  std::vector<int> node_map_vec(num_nodes);
  int *node_map = &node_map_vec[0];
  for (i=0; i<num_nodes; i++){
    node_map[i] = peridigm->getOneDimensionalMap()->GID(i)+1;
  }
  retval = ex_put_node_num_map(file_handle, node_map);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_node_num_map");

  // Write global element number map (global element IDs)
  std::vector<int> elem_map_vec(num_nodes);
  int *elem_map = &elem_map_vec[0];
  int elem_map_index = 0;
  for(std::vector<PeridigmNS::Block>::iterator blockIt = blocks->begin(); blockIt != blocks->end() ; blockIt++) {
    Teuchos::RCP<const Epetra_BlockMap> map = blockIt->getOwnedScalarPointMap();
    for(int i=0; i<map->NumMyElements() ; ++i){
      TEUCHOS_TEST_FOR_EXCEPT_MSG(elem_map_index >= num_nodes, "\nPeridigmNS::OutputManager_ExodusII::initializeExodusDatabase(), Error processing element map!\n");
      elem_map[elem_map_index++] = map->GID(i)+1;
    }
  }
  retval = ex_put_elem_num_map(file_handle, elem_map);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_elem_num_map");

  // Create internal mapping of requested output fields to an integer.
  // The user requests output fields via strings, but Exodus wants an integer to index the output fields
  int global_output_field_index = 1;
  int node_output_field_index = 1;
  int element_output_field_index = 1;
  for (Teuchos::ParameterList::ConstIterator it = outputVariables->begin(); it != outputVariables->end(); ++it) {
    string name = it->first;
    PeridigmNS::FieldSpec spec = PeridigmNS::FieldManager::self().getFieldSpec(name);
    if (spec.getLength() == PeridigmField::SCALAR) {
      if (spec.getRelation() == PeridigmField::GLOBAL) {
        global_output_field_map.insert( std::pair<string,int>(name,global_output_field_index) );
        global_output_field_index = global_output_field_index + 1;
      }
      else if (spec.getRelation() == PeridigmField::NODE) {
        node_output_field_map.insert( std::pair<string,int>(name,node_output_field_index) );
        node_output_field_index = node_output_field_index + 1;
      }
      else if (spec.getRelation() == PeridigmField::ELEMENT) {
        element_output_field_map.insert( std::pair<string,int>(name,element_output_field_index) );
        element_output_field_index = element_output_field_index + 1;
      }
    }
    if (spec.getLength() == PeridigmField::VECTOR) {
      string tmpnameX = name+"X";
      string tmpnameY = name+"Y";
      string tmpnameZ = name+"Z";
      if (spec.getRelation() == PeridigmField::GLOBAL) {
        global_output_field_map.insert( std::pair<string,int>(tmpnameX,global_output_field_index) );
        global_output_field_index = global_output_field_index + 1;
        global_output_field_map.insert( std::pair<string,int>(tmpnameY,global_output_field_index) );
        global_output_field_index = global_output_field_index + 1;
        global_output_field_map.insert( std::pair<string,int>(tmpnameZ,global_output_field_index) );
        global_output_field_index = global_output_field_index + 1;
      }
      if (spec.getRelation() == PeridigmField::NODE) {
        node_output_field_map.insert( std::pair<string,int>(tmpnameX,node_output_field_index) );
        node_output_field_index = node_output_field_index + 1;
        node_output_field_map.insert( std::pair<string,int>(tmpnameY,node_output_field_index) );
        node_output_field_index = node_output_field_index + 1;
        node_output_field_map.insert( std::pair<string,int>(tmpnameZ,node_output_field_index) );
        node_output_field_index = node_output_field_index + 1;
      }
      else if (spec.getRelation() == PeridigmField::ELEMENT) {
        element_output_field_map.insert( std::pair<string,int>(tmpnameX,element_output_field_index) );
        element_output_field_index = element_output_field_index + 1;
        element_output_field_map.insert( std::pair<string,int>(tmpnameY,element_output_field_index) );
        element_output_field_index = element_output_field_index + 1;
        element_output_field_map.insert( std::pair<string,int>(tmpnameZ,element_output_field_index) );
        element_output_field_index = element_output_field_index + 1;
      }
    }
  }

  // Write information records

  // Write global var info
  int num_global_vars = global_output_field_map.size();
  char **global_var_names = NULL;
  if(num_global_vars > 0){
    char **global_var_names = new char*[num_global_vars];
    for (i=0;i<num_global_vars;i++) global_var_names[i] = new char[MAX_STR_LENGTH+1]; // MAX_STR_LENGTH defined in ExodusII.h
    for( std::map<string,int>::iterator it=global_output_field_map.begin() ; it != global_output_field_map.end(); it++ )
      strcpy(global_var_names[(it->second)-1], it->first.c_str() );
    retval = ex_put_var_param(file_handle, "G", num_global_vars);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_param");
    retval = ex_put_var_names (file_handle, "G", num_global_vars, global_var_names);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_param");
  }

  // Write node var info 
  int num_node_vars = node_output_field_map.size();
  char **node_var_names = NULL;
  if(num_node_vars > 0){
    node_var_names = new char*[num_node_vars];
    for (i=0;i<num_node_vars;i++) node_var_names[i] = new char[MAX_STR_LENGTH+1]; // MAX_STR_LENGTH defined in ExodusII.h
    for ( std::map<string,int>::iterator it=node_output_field_map.begin() ; it != node_output_field_map.end(); it++ )
      strcpy(node_var_names[(it->second)-1], it->first.c_str() );
    retval = ex_put_var_param(file_handle,"N",num_node_vars);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_param");
    retval = ex_put_var_names(file_handle,"N",num_node_vars,node_var_names);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_names");
  }

  // Write element var info 
  int num_element_vars = element_output_field_map.size();
  char **element_var_names = NULL;
  if(num_element_vars > 0){
    element_var_names = new char*[num_element_vars];
    for (i=0;i<num_element_vars;i++) element_var_names[i] = new char[MAX_STR_LENGTH+1]; // MAX_STR_LENGTH defined in ExodusII.h
    for (std::map<string,int>::iterator it=element_output_field_map.begin() ; it != element_output_field_map.end(); it++)
      strcpy(element_var_names[(it->second)-1], it->first.c_str() );
    retval = ex_put_var_param(file_handle,"E",num_element_vars);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_param");
    retval = ex_put_var_names(file_handle,"E",num_element_vars,element_var_names);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_names");
  }

  // Write element truth table (only if at least one element variable if defined)
  if (num_element_vars > 0) {
    std::vector<int> truthTableVec(blocks->size() * num_element_vars);
    int truthTableIndex = 0;
    for(blockIt = blocks->begin(); blockIt != blocks->end(); blockIt++){
      for(Teuchos::ParameterList::ConstIterator outputVariableIt = outputVariables->begin(); outputVariableIt != outputVariables->end(); ++outputVariableIt){
        string name = outputVariableIt->first;
        PeridigmNS::FieldSpec spec = PeridigmNS::FieldManager::self().getFieldSpec(name);
        if(spec.getRelation() == PeridigmField::ELEMENT){
          PeridigmField::Step step = PeridigmField::STEP_NONE;
          if(spec.getTemporal() == PeridigmField::TWO_STEP)
            step = PeridigmField::STEP_NP1;
          int truthTableValue = 0;
          if(blockIt->hasData(spec.getId(), step))
            truthTableValue = 1;
          // Global ID and processor number are special cases
          if(spec.getId() == elementIdFieldId || spec.getId() == procNumFieldId)
            truthTableValue = 1;
          truthTableVec[truthTableIndex++] = truthTableValue;
        }
      }
    }
    int *truthTable = &truthTableVec[0];
    retval = ex_put_elem_var_tab (file_handle, blocks->size(), num_element_vars, truthTable);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_tab");
  }

  // Close file; re-open with call to write()
  retval = ex_update(file_handle);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_update");
  retval = ex_close(file_handle);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_close");

  // Clean up
  if(global_var_names != NULL){
    for (i = num_global_vars; i>0; i--) delete[] global_var_names[i-1];
    delete[] global_var_names;
  }
  if(node_var_names != NULL){
    for (i = num_node_vars; i>0; i--) delete[] node_var_names[i-1];
    delete[] node_var_names;
  }
  if(element_var_names != NULL){
    for (i = num_element_vars; i>0; i--) delete[] element_var_names[i-1];
    delete[] element_var_names;
  }
}

void PeridigmNS::OutputManager_ExodusII::initializeExodusDatabaseWithOnlyGlobalData(Teuchos::RCP< std::vector<PeridigmNS::Block> > blocks) {

  /*
   * Determine name of output file
   */

  // Follow convention of replacing spaces or . with underscore
  if (!initializeExodusDatabaseCalled) {
    int warningFlag = 0;
    string outString;
    outString.append("\n\n***WARNING***\n");
    outString.append("PeridigmNS::OutputManager_ExodusII:::OutputManager_ExodusII() -- Avoid use of filenames containing '.' (period) and ' ' (space) with ExodusII.\n");
    outString.append("Changing ");
    outString.append(filenameBase);
    outString.append(" to ");
    for ( unsigned int i = 0; i < filenameBase.length(); i++) {
      if (filenameBase[i] ==' ' || filenameBase[i]=='.')  {
        filenameBase.replace(i,1,"_");
        warningFlag = 1;
      }
    }
    outString.append(filenameBase);
    outString.append(".\n\n\n");
    if (warningFlag) std::cout << outString;
    initializeExodusDatabaseCalled = true;
  }

  // Construct output filename
  filename.str(std::string());
  filename.clear();
  filename << filenameBase.c_str() << ".h";

  /*
   * Now, initialize ExodusII database
   */

  // Default to storing and writing doubles
  int CPU_word_size, IO_word_size;
  CPU_word_size = IO_word_size = sizeof(double);

  // Initialize exodus database; Overwrite any existing file with this name
  file_handle = ex_create(filename.str().c_str(),EX_CLOBBER,&CPU_word_size,&IO_word_size);
  if (file_handle < 0) reportExodusError(file_handle, "OutputManager_ExodusII", "ex_create");

  // clear the maps
  global_output_field_map.clear();
  element_output_field_map.clear();
  node_output_field_map.clear();

  // Initialize the database
  int num_dimensions = 3;
  int num_nodes = 1;
  int num_elements = 1;
  int num_element_blocks = 1;
  int num_node_sets = 0;
  int num_side_sets = 0;
  // Initialize exodus file with parameters
  int retval = ex_put_init(file_handle,"Peridigm", num_dimensions, num_nodes, num_elements, num_element_blocks, num_node_sets, num_side_sets);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_init");

  // Write dummy nodal coordinate values
  double xcoord_values(0.0);
  double ycoord_values(0.0);
  double zcoord_values(0.0);
  retval = ex_put_coord(file_handle,&xcoord_values,&ycoord_values,&zcoord_values);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_coord");

  // Write nodal coordinate names to database
  const char *coord_names[3] = {"x", "y", "z"};
  retval = ex_put_coord_names(file_handle,const_cast<char**>(coord_names));
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_coord_names");

  // Write element block parameters
  int num_elem_in_block(1);
  int num_nodes_in_elem(1);
  int elem_block_ID(1);
  retval = ex_put_elem_block(file_handle,elem_block_ID,"SPHERE",num_elem_in_block,num_nodes_in_elem,0);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_elem_block");

  // Write element connectivity
  int connect(1);
  retval = ex_put_elem_conn(file_handle, elem_block_ID, &connect);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_elem_conn");

  // Write global node number map (global node IDs)
  int node_map(1);
  retval = ex_put_node_num_map(file_handle, &node_map);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_node_num_map");

  // Write global element number map (global element IDs)
  int elem_map(1);
  retval = ex_put_elem_num_map(file_handle, &elem_map);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_elem_num_map");

  // Create internal mapping of requested output fields to an integer.
  // The user requests output fields via strings, but Exodus wants an integer to index the output fields
  int global_output_field_index = 1;
  for (Teuchos::ParameterList::ConstIterator it = outputVariables->begin(); it != outputVariables->end(); ++it) {
    string name = it->first;
    PeridigmNS::FieldSpec spec = PeridigmNS::FieldManager::self().getFieldSpec(name);
    if (spec.getLength() == PeridigmField::SCALAR) {
      global_output_field_map.insert( std::pair<string,int>(name,global_output_field_index) );
      global_output_field_index = global_output_field_index + 1;
    }
    if (spec.getLength() == PeridigmField::VECTOR) {
      string tmpnameX = name+"X";
      string tmpnameY = name+"Y";
      string tmpnameZ = name+"Z";
      global_output_field_map.insert( std::pair<string,int>(tmpnameX,global_output_field_index) );
      global_output_field_index = global_output_field_index + 1;
      global_output_field_map.insert( std::pair<string,int>(tmpnameY,global_output_field_index) );
      global_output_field_index = global_output_field_index + 1;
      global_output_field_map.insert( std::pair<string,int>(tmpnameZ,global_output_field_index) );
      global_output_field_index = global_output_field_index + 1;
    }
  }

  // Write information records

  // Write global var info
  int num_global_vars = global_output_field_map.size();
  char **global_var_names = NULL;
  if(num_global_vars > 0){
    char **global_var_names = new char*[num_global_vars];
    for (int i=0;i<num_global_vars;i++) global_var_names[i] = new char[MAX_STR_LENGTH+1]; // MAX_STR_LENGTH defined in ExodusII.h
    for( std::map<string,int>::iterator it=global_output_field_map.begin() ; it != global_output_field_map.end(); it++ )
      strcpy(global_var_names[(it->second)-1], it->first.c_str() );
    retval = ex_put_var_param(file_handle, "G", num_global_vars);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_param");
    retval = ex_put_var_names (file_handle, "G", num_global_vars, global_var_names);
    if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_put_var_param");
  }

  // Close file; re-open with call to write()
  retval = ex_update(file_handle);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_update");
  retval = ex_close(file_handle);
  if (retval!= 0) reportExodusError(retval, "initializeExodusDatabase", "ex_close");

  // Clean up
  if(global_var_names != NULL){
    for (int i = num_global_vars; i>0; i--) delete[] global_var_names[i-1];
    delete[] global_var_names;
  }
}

void PeridigmNS::OutputManager_ExodusII::reportExodusError(int errorCode, const char *methodName, const char*exodusMethodName) {
  std::stringstream ss;
  if (errorCode < 0) { // error
    if (numProc > 1) ss << "Error on PID #" << myPID << ": ";
    ss << "PeridigmNS::OutputManager_ExodusII::" << methodName << "() -- Error code: " << errorCode << " (" << exodusMethodName << ")";
    TEUCHOS_TEST_FOR_EXCEPTION(1, std::invalid_argument, ss.str());
  }  
  else {
    if (numProc > 1) ss << "Warning on PID #" << myPID << ": ";
    ss << "PeridigmNS::OutputManager_ExodusII::" << methodName << "() -- Warning code: " << errorCode << " (" << exodusMethodName << ")";
    std::cout << ss.str() << std::endl;
  }
}
