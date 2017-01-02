/*
 * The MIT License
 *
 * Copyright (c) 1997-2015 The University of Utah
 * Copyright (c) 2015-     Parresia Research Limited, New Zealand
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
#include <CCA/Components/MPM/AMRMPM.h>
#include <CCA/Components/MPM/ConstitutiveModel/ConstitutiveModel.h>
#include <CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <CCA/Components/MPM/Contact/Contact.h>                     // for Contact
#include <CCA/Components/MPM/Contact/ContactFactory.h>
#include <CCA/Components/MPM/MPMBoundCond.h>                        // for MPMBoundCond
#include <CCA/Components/MPM/ParticleCreator/ParticleCreator.h>
#include <CCA/Components/MPM/PhysicalBC/MPMPhysicalBC.h>
#include <CCA/Components/MPM/PhysicalBC/MPMPhysicalBCFactory.h>
#include <CCA/Components/MPM/PhysicalBC/PressureBC.h>
#include <CCA/Components/MPM/SerialMPM.h>                // for SerialMPM
#include <CCA/Components/Regridder/PerPatchVars.h>       // for PatchFlagP, etc
#include <CCA/Ports/DataWarehouse.h>                     // for DataWarehouse
#include <CCA/Ports/Output.h>                            // for Output
#include <CCA/Ports/Scheduler.h>                         // for Scheduler
#include <Core/Disclosure/TypeUtils.h>                   // for long64
#include <Core/Exceptions/InternalError.h>               // for InternalError
#include <Core/Exceptions/ProblemSetupException.h>
#include <Core/Geometry/IntVector.h>                     // for IntVector, operator<<, Max, etc
#include <Core/Geometry/Point.h>                         // for Point, operator<<
#include <Core/GeometryPiece/GeometryObject.h>           // for GeometryObject
#include <Core/Grid/AMR.h>
#include <Core/Grid/AMR_CoarsenRefine.h>                 // for fineToCoarseOperator
#include <Core/Grid/DbgOutput.h>                         // for printTask, printSchedule
#include <Core/Grid/Ghost.h>                             // for Ghost, etc
#include <Core/Grid/Grid.h>                              // for Grid
#include <Core/Grid/ParticleInterpolator.h>              // for ParticleInterpolator
#include <Core/Grid/SimulationState.h>                   // for SimulationState
#include <Core/Grid/Task.h>                              // for Task, Task::WhichDW::OldDW, etc
#include <Core/Grid/Variables/Array3.h>                  // for Array3
#include <Core/Grid/Variables/CCVariable.h>              // for CCVariable, etc
#include <Core/Grid/Variables/CellIterator.h>            // for CellIterator
#include <Core/Grid/Variables/GridVariableBase.h>        // for GridVariableBase
#include <Core/Grid/Variables/NodeIterator.h>            // for NodeIterator
#include <Core/Grid/Variables/ParticleSubset.h>          // for ParticleSubset, etc
#include <Core/Grid/Variables/ParticleVariable.h>        // for ParticleVariable, etc
#include <Core/Grid/Variables/ParticleVariableBase.h>
#include <Core/Grid/Variables/PerPatch.h>                // for PerPatch
#include <Core/Grid/Variables/Stencil7.h>                // for Stencil7
#include <Core/Grid/Variables/VarLabel.h>                // for VarLabel
#include <Core/Grid/Variables/VarTypes.h>                // for delt_vartype, etc
#include <Core/Labels/MPMLabel.h>                        // for MPMLabel
#include <Core/Malloc/Allocator.h>                       // for scinew
#include <Core/Math/Matrix3.h>                           // for Matrix3, swapbytes, etc
#include <Core/Parallel/Parallel.h>                      // for proc0cout
#include <Core/Parallel/ProcessorGroup.h>                // for ProcessorGroup
#include <Core/Parallel/UintahParallelComponent.h>
#include <Core/Parallel/UintahParallelPort.h>            // for UintahParallelPort
#include <Core/ProblemSpec/ProblemSpec.h>                // for Vector, IntVector, etc
#include <Core/ProblemSpec/ProblemSpecP.h>               // for ProblemSpecP
#include <Core/Util/DebugStream.h>                       // for DebugStream
#include <Core/Util/Handle.h>                            // for Handle

#include <algorithm>                                     // for max, min
#include <cmath>                                         // for cbrt, isinf, isnan
#include <iostream>                                      // for operator<<, basic_ostream, etc
#include <mpi.h>                                         // for Uintah::MPI::Pack_size
#include <stdlib.h>                                      // for abs
#include <string>                                        // for string, operator==, etc

using namespace Uintah;
using namespace std;

static DebugStream cout_doing("AMRMPM_cout", false);
static DebugStream amr_doing("AMRMPM_amr", false);

//__________________________________
//  To turn on debug flags
//  csh/tcsh : setenv SCI_DEBUG "AMRMPM_cout:+,AMRMPM_amr:+"
//  bash     : export SCI_DEBUG="AMRMPM_cout:+,AMRMPM_amr:+"
//  default is OFF
//#define USE_DEBUG_TASK
//#define DEBUG_VEL
//#define DEBUG_ACC
#undef CBDI_FLUXBCS
#define USE_FLUX_RESTRICTION

//__________________________________
//   TODO:
// - We only need to compute ZOI when the grid changes not every timestep
//
// - InterpolateParticlesToGrid_CFI()  Need to account for gimp when getting particles on coarse level.
//
// - scheduleTimeAdvance:  Do we need to schedule each task in a separate level loop?  I suspect that we only need
//                         to in the CFI tasks
//
//  What is going on in refine & coarsen
//  To Test:
//    Symetric BC
//    compilicated grids  
//    multi processors 
//
//  Need to Add gimp interpolation

// From ThreadPool.cc:  Used for syncing cerr'ing so it is easier to read.
//extern Mutex cerrLock;

AMRMPM::AMRMPM(const ProcessorGroup* myworld) :SerialMPM(myworld)
{
  flags->d_minGridLevel = 0;
  flags->d_maxGridLevel = 1000;

  d_SMALL_NUM_MPM=1e-200;
  contactModel   = 0;
  NGP     = -9;
  NGN     = -9;
  d_nPaddingCells_Coarse = -9;
  dataArchiver = 0;
  d_acc_ans = Vector(0,0,0);
  d_acc_tol = 1e-7;
  d_vel_ans = Vector(-100,0,0);
  d_vel_tol = 1e-7;

  d_gac = Ghost::AroundCells;  // for readability
  d_gan = Ghost::AroundNodes;
  d_gn  = Ghost::None;
  
  pDbgLabel = VarLabel::create("p.dbg",
                               ParticleVariable<double>::getTypeDescription());
  gSumSLabel= VarLabel::create("g.sum_S",
                               NCVariable<double>::getTypeDescription());
  RefineFlagXMaxLabel = VarLabel::create("RefFlagXMax",
                                         max_vartype::getTypeDescription() );
  RefineFlagXMinLabel = VarLabel::create("RefFlagXMin",
                                         min_vartype::getTypeDescription() );
  RefineFlagYMaxLabel = VarLabel::create("RefFlagYMax",
                                         max_vartype::getTypeDescription() );
  RefineFlagYMinLabel = VarLabel::create("RefFlagYMin",
                                         min_vartype::getTypeDescription() );
  RefineFlagZMaxLabel = VarLabel::create("RefFlagZMax",
                                         max_vartype::getTypeDescription() );
  RefineFlagZMinLabel = VarLabel::create("RefFlagZMin",
                                         min_vartype::getTypeDescription() );
  
  d_one_matl = scinew MaterialSubset();
  d_one_matl->add(0);
  d_one_matl->addReference();

}
//______________________________________________________________________
//
AMRMPM::~AMRMPM()
{
  VarLabel::destroy(pDbgLabel);
  VarLabel::destroy(gSumSLabel);
  VarLabel::destroy(RefineFlagXMaxLabel);
  VarLabel::destroy(RefineFlagYMaxLabel);
  VarLabel::destroy(RefineFlagZMaxLabel);
  VarLabel::destroy(RefineFlagXMinLabel);
  VarLabel::destroy(RefineFlagYMinLabel);
  VarLabel::destroy(RefineFlagZMinLabel);
  
  if (d_one_matl->removeReference())
    delete d_one_matl;
  
  for (int i = 0; i< (int)d_refine_geom_objs.size(); i++) {
    delete d_refine_geom_objs[i];
  }
}

//______________________________________________________________________
//
void AMRMPM::problemSetup(const ProblemSpecP& prob_spec, 
                          const ProblemSpecP& restart_prob_spec,
                          GridP& grid,
                          SimulationStateP& sharedState)
{
  cout_doing<<"Doing problemSetup\t\t\t\t\t AMRMPM"<<endl;
  
  d_sharedState = sharedState;

  dynamic_cast<Scheduler*>(getPort("scheduler"))->setPositionVar(lb->pXLabel);

  dataArchiver = dynamic_cast<Output*>(getPort("output"));

  if(!dataArchiver){
    throw InternalError("AMRMPM:couldn't get output port", __FILE__, __LINE__);
  }

  ProblemSpecP mat_ps = 0;
  if (restart_prob_spec){
    mat_ps = restart_prob_spec;
  } else{
    mat_ps = prob_spec;
  }

  ProblemSpecP mpm_soln_ps = mat_ps->findBlock("MPM");
  if (!mpm_soln_ps){
    ostringstream warn;
    warn<<"ERROR:MPM:\n missing MPM section in the AMRMPM input file\n";
    throw ProblemSetupException(warn.str(), __FILE__, __LINE__);
  }

  // Read all MPM flags (look in MPMFlags.cc)
  flags->readMPMFlags(mat_ps, dataArchiver);
  if (flags->d_integrator_type == "implicit"){
    throw ProblemSetupException("Can't use implicit integration with AMRMPM",
                                __FILE__, __LINE__);
  }

  //__________________________________
  //  Read in the AMR section
  ProblemSpecP mpm_ps;
  ProblemSpecP amr_ps = prob_spec->findBlock("AMR");
  if (amr_ps){
    mpm_ps = amr_ps->findBlock("MPM");
    flags->d_AMR=true;
  } else {
    string warn;
    warn ="\n INPUT FILE ERROR:\n <AMR>  block not found in input file \n";
    throw ProblemSetupException(warn, __FILE__, __LINE__);
  }
  
  
  if(!mpm_ps){
    string warn;
    warn ="\n INPUT FILE ERROR:\n <MPM>  block not found inside of <AMR> block \n";
    throw ProblemSetupException(warn, __FILE__, __LINE__);
  }

  ProblemSpecP refine_ps = mpm_ps->findBlock("Refinement_Criteria_Thresholds");
#if 0
  if(!refine_ps ){
    string warn;
    warn ="\n INPUT FILE ERROR:\n <Refinement_Criteria_Thresholds> "
         " block not found inside of <MPM> block \n";
    throw ProblemSetupException(warn, __FILE__, __LINE__);
  }
#endif

  //__________________________________
  // Pull out the refinement threshold criteria 
  if(refine_ps ){
    for (ProblemSpecP var_ps = refine_ps->findBlock("Variable");var_ps != 0;
                      var_ps = var_ps->findNextBlock("Variable")) {
      thresholdVar data;
      string name, value, matl;

      map<string,string> input;
      var_ps->getAttributes(input);
      name  = input["name"];
      value = input["value"];
      matl  = input["matl"];

      stringstream n_ss(name);
      stringstream v_ss(value);
      stringstream m_ss(matl);

      n_ss >> data.name;
      v_ss >> data.value;
      m_ss >> data.matl;

      if( !n_ss || !v_ss || (!m_ss && matl!="all") ) {
        cerr << "WARNING: AMRMPM.cc: stringstream failed...\n";
      }

      int numMatls = d_sharedState->getNumMatls();

      //__________________________________
      // if using "all" matls 
      if(matl == "all"){
        for (int m = 0; m < numMatls; m++){
          data.matl = m;
          d_thresholdVars.push_back(data);
        }
      }else{
        d_thresholdVars.push_back(data);
      }
    }
  }

  //__________________________________
  // override CFI_interpolator
  d_CFI_interpolator = flags->d_interpolator_type;
  mpm_ps->get("CFI_interpolator", d_CFI_interpolator );
  
  if(d_CFI_interpolator != flags->d_interpolator_type ){
    proc0cout << "______________________________________________________\n" 
              << "          AMRMPM:  WARNING\n"
              << "  The particle to grid interpolator at the CFI is (" << d_CFI_interpolator
              << "), however the over rest of the domain it is: " << flags->d_interpolator_type
              << "\n______________________________________________________________________" << endl;
  }
  
  // bulletproofing
  int maxLevel = grid->numLevels();
  for(int L = 0; L<maxLevel; L++){
    LevelP level = grid->getLevel(L);
    IntVector ec = level->getExtraCells();
    
    if( ec != IntVector(1,1,1) && flags->d_interpolator_type == "gimp" ){       // This should be generalized
      ostringstream msg;                                                        // Each interpolator should know how many EC needed.
      msg << "\n AMRMPM ERROR:\n The number of extraCells on level ("
          << level->getIndex() << ") is not equal to [1,1,1] required for the GIMP particle interpolator";
      throw ProblemSetupException(msg.str(), __FILE__, __LINE__);    
    }
  }
  
  
  
  

#if 0  // This allows defining regions to be refined using geometry objects
       // Jim was having a bit of trouble keeping this consistent with other
       // methods of defining finer levels.  Keep for now.
  ProblemSpecP refine_ps = mpm_ps->findBlock("Refine_Regions");
    // Read in the refined regions geometry objects
    int piece_num = 0;
    list<GeometryObject::DataItem> geom_obj_data;
    geom_obj_data.push_back(GeometryObject::DataItem("level", GeometryObject::Integer));

    for (ProblemSpecP geom_obj_ps = refine_ps->findBlock("geom_object");
         geom_obj_ps != 0;
         geom_obj_ps = geom_obj_ps->findNextBlock("geom_object") ) {

      vector<GeometryPieceP> pieces;
      GeometryPieceFactory::create(geom_obj_ps, pieces);

      GeometryPieceP mainpiece;
      if(pieces.size() == 0){
        throw ParameterNotFound("No piece specified in geom_object", __FILE__, __LINE__);
      } else if(pieces.size() > 1){
        mainpiece = scinew UnionGeometryPiece(pieces);
      } else {
        mainpiece = pieces[0];
      }
      piece_num++;
      d_refine_geom_objs.push_back(scinew GeometryObject(mainpiece,geom_obj_ps,geom_obj_data));
    }
  }  // if(refine_ps)
#endif

//__________________________________
//  bulletproofing
  if(!d_sharedState->isLockstepAMR()){
    ostringstream msg;
    msg << "\n ERROR: You must add \n"
        << " <useLockStep> true </useLockStep> \n"
        << " inside of the <AMR> section. \n"; 
    throw ProblemSetupException(msg.str(),__FILE__, __LINE__);
  }  
    
  if(flags->d_8or27==8){
    NGP=1;
    NGN=1;
  } else{
    NGP=2;
    NGN=2;
  }

  MPMPhysicalBCFactory::create(mat_ps, grid, flags);
  
  contactModel = ContactFactory::create(UintahParallelComponent::d_myworld,
                                        mat_ps,sharedState,lb,flags);

  // Determine extents for coarser level particle data
  // Linear Interpolation:  1 layer of coarse level cells
  // Gimp Interpolation:    2 layers
  
  /*`==========TESTING==========*/
  d_nPaddingCells_Coarse = 1;
  //  NGP = 1; 
  /*===========TESTING==========`*/

  d_sharedState->setParticleGhostLayer(Ghost::AroundNodes, NGP);

  materialProblemSetup(mat_ps, grid, d_sharedState,flags);

  // Create deformation gradient computer
  d_defGradComputer = scinew DeformationGradientComputer(flags, d_sharedState);

}

//______________________________________________________________________
//
void AMRMPM::outputProblemSpec(ProblemSpecP& root_ps)
{
  ProblemSpecP root = root_ps->getRootNode();

  ProblemSpecP flags_ps = root->appendChild("MPM");
  flags->outputProblemSpec(flags_ps);

  ProblemSpecP mat_ps = 0;
  mat_ps = root->findBlockWithOutAttribute("MaterialProperties");

  if (mat_ps == 0)
    mat_ps = root->appendChild("MaterialProperties");

  ProblemSpecP mpm_ps = mat_ps->appendChild("MPM");
  for (int i = 0; i < d_sharedState->getNumMPMMatls();i++) {
    MPMMaterial* mat = d_sharedState->getMPMMaterial(i);
    ProblemSpecP cm_ps = mat->outputProblemSpec(mpm_ps);
  }
  contactModel->outputProblemSpec(mpm_ps);

  ProblemSpecP physical_bc_ps = root->appendChild("PhysicalBC");
  ProblemSpecP mpm_ph_bc_ps = physical_bc_ps->appendChild("MPM");
  for (int ii = 0; ii<(int)MPMPhysicalBCFactory::mpmPhysicalBCs.size(); ii++) {
    MPMPhysicalBCFactory::mpmPhysicalBCs[ii]->outputProblemSpec(mpm_ph_bc_ps);
  }

}

//______________________________________________________________________
//
void AMRMPM::scheduleInitialize(const LevelP& level, SchedulerP& sched)
{

  if (flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels())){
    proc0cout << "doMPMOnLevel = " << level->getIndex() << endl;
  }
  else{
    proc0cout << "DontDoMPMOnLevel = " << level->getIndex() << endl;
  }
  
  if (!flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels()))
    return;
  Task* t = scinew Task("AMRMPM::actuallyInitialize",
                        this, &AMRMPM::actuallyInitialize);

  t->computes(lb->partCountLabel);
  t->computes(lb->pXLabel);
  t->computes(lb->pDispLabel);
  t->computes(lb->pFiberDirLabel);
  t->computes(lb->pMassLabel);
  t->computes(lb->pVolumeLabel);
  t->computes(lb->pTemperatureLabel);
  t->computes(lb->pTempPreviousLabel); // for therma  stress analysis
  t->computes(lb->pdTdtLabel);
  t->computes(lb->pVelocityLabel);
  t->computes(lb->pExternalForceLabel);
  t->computes(lb->pParticleIDLabel);
  t->computes(lb->pStressLabel);
  t->computes(lb->pTemperatureGradientLabel);
  t->computes(lb->pSizeLabel);
  t->computes(lb->pAreaLabel);
  t->computes(lb->pRefinedLabel);
  t->computes(lb->pLastLevelLabel);
  t->computes(d_sharedState->get_delt_label(),level.get_rep());
  t->computes(lb->pCellNAPIDLabel,d_one_matl);
  t->computes(lb->NC_CCweightLabel,d_one_matl);

  if(!flags->d_doGridReset){
    t->computes(lb->gDisplacementLabel);
  }
  
  // Debugging Scalar
  if (flags->d_with_color) {
    t->computes(lb->pColorLabel);
  }

  if (flags->d_useLoadCurves) {
    // Computes the load curve ID associated with each particle
    t->computes(lb->pLoadCurveIDLabel);
  }

  if (flags->d_reductionVars->accStrainEnergy) {
    // Computes accumulated strain energy
    t->computes(lb->AccStrainEnergyLabel);
  }
  
  if(flags->d_artificial_viscosity){
    t->computes(lb->p_qLabel);
  }

  int numMPM = d_sharedState->getNumMPMMatls();
  const PatchSet* patches = level->eachPatch();
  for(int m = 0; m < numMPM; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();

    // Add vel grad/def grad computes
    d_defGradComputer->addInitialComputesAndRequires(t, mpm_matl, patches);

    cm->addInitialComputesAndRequires(t, mpm_matl, patches);
  }

  // Add initialization of body force and coriolis importance terms
  // These are initialize to zero in ParticleCreator
  t->computes(lb->pCoriolisImportanceLabel);
  t->computes(lb->pBodyForceAccLabel);  

  sched->addTask(t, level->eachPatch(), d_sharedState->allMPMMaterials());

  if (level->getIndex() == 0 ) schedulePrintParticleCount(level, sched); 

  if (flags->d_useLoadCurves) {
    // Schedule the initialization of pressure BCs per particle
    cout << "Pressure load curves are untested for multiple levels" << endl;
    scheduleInitializePressureBCs(level, sched);
  }

}
//______________________________________________________________________
//
void AMRMPM::schedulePrintParticleCount(const LevelP& level, 
                                        SchedulerP& sched)
{
  Task* t = scinew Task("AMRMPM::printParticleCount",
                        this, &AMRMPM::printParticleCount);
  t->requires(Task::NewDW, lb->partCountLabel);
  t->setType(Task::OncePerProc);

  sched->addTask(t, sched->getLoadBalancer()->getPerProcessorPatchSet(level), d_sharedState->allMPMMaterials());
}
//______________________________________________________________________
//
void AMRMPM::scheduleComputeStableTimestep(const LevelP&,
                                           SchedulerP&)
{
  // Nothing to do here - delt is computed as a by-product of the
  // constitutive model
}

//______________________________________________________________________
//
void AMRMPM::scheduleTimeAdvance(const LevelP & level,
                                 SchedulerP   & sched)
{
  if(level->getIndex() > 0)  // only schedule once
    return;

  const MaterialSet* matls = d_sharedState->allMPMMaterials();
  int maxLevels = level->getGrid()->numLevels();
  GridP grid = level->getGrid();


  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    schedulePartitionOfUnity(               sched, patches, matls);
    scheduleComputeZoneOfInfluence(         sched, patches, matls);
    scheduleComputeParticleBodyForce(       sched, patches, matls);
    scheduleApplyExternalLoads(             sched, patches, matls);
  }

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleInterpolateParticlesToGrid(     sched, patches, matls);
    // Need to add a task to do the reductions on the max hydro stress - JG ???
  }

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleInterpolateParticlesToGrid_CFI( sched, patches, matls);
  }

#ifdef USE_DEBUG_TASK
  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleDebug_CFI( sched, patches, matls);
  }
#endif

  for (int l = 0; l < maxLevels-1; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleCoarsenNodalData_CFI(      sched, patches, matls, coarsenData);
  }

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleNormalizeNodalVelTempConc(sched, patches, matls);
    scheduleExMomInterpolated(        sched, patches, matls);
  }

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleComputeInternalForce(           sched, patches, matls);
  }

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleComputeInternalForce_CFI(       sched, patches, matls);
  }

  for (int l = 0; l < maxLevels-1; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleCoarsenNodalData_CFI2( sched, patches, matls);
  }

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleComputeAndIntegrateAcceleration(sched, patches, matls);
    scheduleExMomIntegrated(                sched, patches, matls);
    scheduleSetGridBoundaryConditions(      sched, patches, matls);
  }

#if 0  // Jim sees no need to do this task, at least not for linear interp.
  // zero the nodal data at the CFI on the coarse level 
  for (int l = 0; l < maxLevels-1; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleCoarsenNodalData_CFI( sched, patches, matls, zeroData);
  }
#endif

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleInterpolateToParticlesAndUpdate(sched, patches, matls);
  }

#if 0  // This may need to be reactivated when we enable GIMP
  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleInterpolateToParticlesAndUpdate_CFI(sched, patches, matls);
  }
#endif

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleComputeStressTensor(            sched, patches, matls);
  }

  if(flags->d_computeScaleFactor){
    for (int l = 0; l < maxLevels; l++) {
      const LevelP& level = grid->getLevel(l);
      const PatchSet* patches = level->eachPatch();
      scheduleComputeParticleScaleFactor(       sched, patches, matls);
    }
  }

  /*
  Commented out for now: Biswajit: Nov 12, 2015
  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleFinalParticleUpdate(            sched, patches, matls);
  }
  */

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    if(flags->d_refineParticles){
      scheduleAddParticles(                 sched, patches, matls);
    }
  }

  for (int l = 0; l < maxLevels; l++) {
    const LevelP& level = grid->getLevel(l);
    const PatchSet* patches = level->eachPatch();
    scheduleReduceFlagsExtents(             sched, patches, matls);
  }
}

//______________________________________________________________________
//
void AMRMPM::scheduleFinalizeTimestep( const LevelP& level, SchedulerP& sched)
{

  const PatchSet* patches = level->eachPatch();

  if (level->getIndex() == 0) {
    const MaterialSet* matls = d_sharedState->allMPMMaterials();
    sched->scheduleParticleRelocation(level, lb->pXLabel_preReloc,
                                      d_sharedState->d_particleState_preReloc,
                                      lb->pXLabel, 
                                      d_sharedState->d_particleState,
                                      lb->pParticleIDLabel, matls);
  }
  scheduleCountParticles(patches,sched);
}

//______________________________________________________________________
//
void AMRMPM::schedulePartitionOfUnity(SchedulerP& sched,
                                      const PatchSet* patches,
                                      const MaterialSet* matls)
{
  printSchedule(patches,cout_doing,"AMRMPM::schedulePartitionOfUnity");
  Task* t = scinew Task("AMRMPM::partitionOfUnity",
                        this, &AMRMPM::partitionOfUnity);
                  
  t->requires(Task::OldDW, lb->pXLabel,    Ghost::None);

  // Carry forward and update pSize if particles change levels
  t->requires(Task::OldDW, lb->pSizeLabel, Ghost::None);
  t->requires(Task::OldDW, lb->pLastLevelLabel, Ghost::None);

  t->computes(lb->pSizeLabel_preReloc);
  t->computes(lb->pLastLevelLabel_preReloc);
  t->computes(lb->pPartitionUnityLabel);
  t->computes(lb->MPMRefineCellLabel, d_one_matl);

  sched->addTask(t, patches, matls);
}

//______________________________________________________________________
//
void AMRMPM::scheduleComputeZoneOfInfluence(SchedulerP& sched,
                                            const PatchSet* patches,
                                            const MaterialSet* matls)
{
  const Level* level = getLevel(patches);
  int L_indx = level->getIndex();

  if(L_indx > 0 ){

    printSchedule(patches,cout_doing,"AMRMPM::scheduleComputeZoneOfInfluence");
    Task* t = scinew Task("AMRMPM::computeZoneOfInfluence",
                          this, &AMRMPM::computeZoneOfInfluence);

    t->computes(lb->gZOILabel, d_one_matl);

    sched->addTask(t, patches, matls);

  }
}

//______________________________________________________________________
//
void AMRMPM::scheduleInterpolateParticlesToGrid(SchedulerP& sched,
                                                const PatchSet* patches,
                                                const MaterialSet* matls)
{

  const Level* level = getLevel(patches);
  if (!flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels())){
    return;
  }

  printSchedule(patches,cout_doing,"AMRMPM::scheduleInterpolateParticlesToGrid");


  Task* t = scinew Task("AMRMPM::interpolateParticlesToGrid",
                        this,&AMRMPM::interpolateParticlesToGrid);

  t->requires(Task::OldDW, lb->pMassLabel,               d_gan,NGP);
  t->requires(Task::OldDW, lb->pVolumeLabel,             d_gan,NGP);
  t->requires(Task::OldDW, lb->pVelocityLabel,           d_gan,NGP);
  if (flags->d_GEVelProj) {
    t->requires(Task::OldDW, lb->pVelGradLabel,          d_gan,NGP);
  }
  t->requires(Task::OldDW, lb->pXLabel,                  d_gan,NGP);
  t->requires(Task::NewDW, lb->pExtForceLabel_preReloc,  d_gan,NGP);
  t->requires(Task::OldDW, lb->pTemperatureLabel,        d_gan,NGP);
  t->requires(Task::NewDW, lb->pSizeLabel_preReloc,      d_gan,NGP);
  t->requires(Task::OldDW, lb->pDefGradLabel, d_gan,NGP);

  t->computes(lb->gMassLabel);
  t->computes(lb->gVolumeLabel);
  t->computes(lb->gVelocityLabel);
  t->computes(lb->gTemperatureLabel);
  t->computes(lb->gTemperatureRateLabel);
  t->computes(lb->gExternalForceLabel);

  sched->addTask(t, patches, matls);
}
//______________________________________________________________________
//  You need particle data from the coarse levels at the CFI on the fine level
void AMRMPM::scheduleInterpolateParticlesToGrid_CFI(SchedulerP& sched,
                                                    const PatchSet* patches,
                                                    const MaterialSet* matls)
{
  const Level* fineLevel = getLevel(patches);
  int L_indx = fineLevel->getIndex();

  if(L_indx > 0 ){
    printSchedule(patches,cout_doing,
                  "AMRMPM::scheduleInterpolateParticlesToGrid_CFI");

    Task* t = nullptr;
    if( d_CFI_interpolator == "gimp" ){

      t = scinew Task("AMRMPM::interpolateParticlesToGrid_CFI_GIMP",
                 this,&AMRMPM::interpolateParticlesToGrid_CFI_GIMP);
    }else{
      t = scinew Task("AMRMPM::interpolateParticlesToGrid_CFI",
                          this,&AMRMPM::interpolateParticlesToGrid_CFI);
    }

    Task::MaterialDomainSpec  ND  = Task::NormalDomain;
    
/*`==========TESTING==========*/
    // Linear 1 coarse Level cells:
    // Gimp:  2 coarse level cells:
    int npc = d_nPaddingCells_Coarse;  
/*===========TESTING==========`*/
    
#define allPatches 0
#define allMatls 0
    //__________________________________
    //  Note: were using nPaddingCells to extract the region of coarse level
    // particles around every fine patch.   Technically, these are ghost
    // cells but somehow it works.
    t->requires(Task::NewDW, lb->gZOILabel,                d_one_matl,  Ghost::None, 0);
    t->requires(Task::OldDW, lb->pMassLabel,               allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    t->requires(Task::OldDW, lb->pVolumeLabel,             allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    t->requires(Task::OldDW, lb->pVelocityLabel,           allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    t->requires(Task::OldDW, lb->pXLabel,                  allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    t->requires(Task::NewDW, lb->pExtForceLabel_preReloc,  allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    t->requires(Task::OldDW, lb->pTemperatureLabel,        allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    t->requires(Task::OldDW, lb->pDefGradLabel, allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);

    t->modifies(lb->gMassLabel);
    t->modifies(lb->gVolumeLabel);
    t->modifies(lb->gVelocityLabel);
    t->modifies(lb->gTemperatureLabel);
    t->modifies(lb->gExternalForceLabel);

    sched->addTask(t, patches, matls);
  }
}

//______________________________________________________________________
//  This task does one of two operations on the coarse nodes along
//  the coarse fine interface.  The input parameter "flag" determines
//  which.
//  Coarsen:  copy fine patch node data to the coarse level at CFI
//  Zero:     zero the coarse level nodal data directly under the fine level
void AMRMPM::scheduleCoarsenNodalData_CFI(SchedulerP& sched,
                                          const PatchSet* patches,
                                          const MaterialSet* matls,
                                          const coarsenFlag flag)
{
  string txt = "(zero)";
  if (flag == coarsenData){
    txt = "(coarsen)";
  }

  printSchedule(patches,cout_doing,"AMRMPM::scheduleCoarsenNodalData_CFI"+txt);

  Task* t = scinew Task("AMRMPM::coarsenNodalData_CFI",
                        this,&AMRMPM::coarsenNodalData_CFI, flag);

  Task::MaterialDomainSpec  ND  = Task::NormalDomain;
#define allPatches 0
#define allMatls 0

  t->requires(Task::NewDW, lb->gMassLabel,          allPatches, Task::FineLevel,allMatls, ND, d_gn,0);
  t->requires(Task::NewDW, lb->gVolumeLabel,        allPatches, Task::FineLevel,allMatls, ND, d_gn,0);
  t->requires(Task::NewDW, lb->gVelocityLabel,      allPatches, Task::FineLevel,allMatls, ND, d_gn,0);
  t->requires(Task::NewDW, lb->gTemperatureLabel,   allPatches, Task::FineLevel,allMatls, ND, d_gn,0);
  t->requires(Task::NewDW, lb->gExternalForceLabel, allPatches, Task::FineLevel,allMatls, ND, d_gn,0);
  
  t->modifies(lb->gMassLabel);
  t->modifies(lb->gVolumeLabel);
  t->modifies(lb->gVelocityLabel);
  t->modifies(lb->gTemperatureLabel);
  t->modifies(lb->gExternalForceLabel);

  if (flag == zeroData){
    t->modifies(lb->gAccelerationLabel);
    t->modifies(lb->gVelocityStarLabel);
  }

  sched->addTask(t, patches, matls);
}

//______________________________________________________________________
//  This task copies fine patch node data to the coarse level at CFI
void AMRMPM::scheduleCoarsenNodalData_CFI2(SchedulerP& sched,
                                           const PatchSet* patches,
                                           const MaterialSet* matls)
{
  printSchedule( patches, cout_doing,"AMRMPM::scheduleCoarsenNodalData_CFI2" );

  Task* t = scinew Task( "AMRMPM::coarsenNodalData_CFI2",
                         this,&AMRMPM::coarsenNodalData_CFI2 );

  Task::MaterialDomainSpec  ND  = Task::NormalDomain;
#define allPatches 0
#define allMatls 0

  t->requires(Task::NewDW, lb->gMassLabel,          allPatches,
                                           Task::FineLevel,allMatls, ND, d_gn,0);
  t->requires(Task::NewDW, lb->gInternalForceLabel, allPatches,
                                           Task::FineLevel,allMatls, ND, d_gn,0);
  
  t->modifies(lb->gInternalForceLabel);
  sched->addTask(t, patches, matls);
}

//______________________________________________________________________
//  compute the nodal velocity and temperature after coarsening the fine
//  nodal data
void AMRMPM::scheduleNormalizeNodalVelTempConc(SchedulerP& sched,
                                               const PatchSet* patches,
                                               const MaterialSet* matls)
{
  printSchedule(patches,cout_doing,"AMRMPM::scheduleNormalizeNodalVelTempConc");

  Task* t = scinew Task("AMRMPM::normalizeNodalVelTempConc",
                        this,&AMRMPM::normalizeNodalVelTempConc);
                   
  t->requires(Task::NewDW, lb->gMassLabel,  d_gn);
  t->modifies(lb->gVelocityLabel);
  t->modifies(lb->gTemperatureLabel);
  
  sched->addTask(t, patches, matls);
}


//______________________________________________________________________
//
/////////////////////////////////////////////////////////////////////////
/*!  **WARNING** In addition to the stresses and deformations, the internal 
 *               heat rate in the particles (pdTdtLabel) 
 *               is computed here */
/////////////////////////////////////////////////////////////////////////
void AMRMPM::scheduleComputeStressTensor(SchedulerP& sched,
                                         const PatchSet* patches,
                                         const MaterialSet* matls)
{
  const Level* level = getLevel(patches);
  if (!flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels())){
    return;
  }
  
  // Schedule compute of the deformation gradient
  scheduleComputeDeformationGradient(sched, patches, matls);

  printSchedule(patches,cout_doing,"AMRMPM::scheduleComputeStressTensor");
  
  int numMatls = d_sharedState->getNumMPMMatls();
  Task* t = scinew Task("AMRMPM::computeStressTensor",
                        this, &AMRMPM::computeStressTensor);
                  
  for(int m = 0; m < numMatls; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    const MaterialSubset* matlset = mpm_matl->thisMaterial();
    
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    cm->addComputesAndRequires(t, mpm_matl, patches);
       
    t->computes(lb->p_qLabel_preReloc, matlset);
  }

  t->computes(d_sharedState->get_delt_label(),getLevel(patches));
  t->computes(lb->StrainEnergyLabel);

  sched->addTask(t, patches, matls);

  if (flags->d_reductionVars->accStrainEnergy) 
    scheduleComputeAccStrainEnergy(sched, patches, matls);
}

//______________________________________________________________________
//
void AMRMPM::scheduleUpdateErosionParameter(SchedulerP& sched,
                                            const PatchSet* patches,
                                            const MaterialSet* matls)
{
  const Level* level = getLevel(patches);
  if (!flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels())){
    return;
  }

  printSchedule(patches,cout_doing,"AMRMPM::scheduleUpdateErosionParameter");

  Task* t = scinew Task("AMRMPM::updateErosionParameter",
                        this, &AMRMPM::updateErosionParameter);

  int numMatls = d_sharedState->getNumMPMMatls();

  for(int m = 0; m < numMatls; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    cm->addRequiresDamageParameter(t, mpm_matl, patches);
  }

  sched->addTask(t, patches, matls);
}
//______________________________________________________________________
//
void AMRMPM::scheduleComputeInternalForce(SchedulerP& sched,
                                          const PatchSet* patches,
                                          const MaterialSet* matls)
{
  const Level* level = getLevel(patches);
  if (!flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels())){
    return;
  }

  printSchedule(patches,cout_doing,"AMRMPM::scheduleComputeInternalForce");
   
  Task* t = scinew Task("AMRMPM::computeInternalForce",
                        this, &AMRMPM::computeInternalForce);

  t->requires(Task::NewDW,lb->gVolumeLabel, d_gn);
  t->requires(Task::OldDW,lb->pStressLabel,               d_gan,NGP);
  t->requires(Task::OldDW,lb->pVolumeLabel,               d_gan,NGP);
  t->requires(Task::OldDW,lb->pXLabel,                    d_gan,NGP);
  t->requires(Task::NewDW,lb->pSizeLabel_preReloc,        d_gan,NGP);
  t->requires(Task::OldDW, lb->pDefGradLabel,  d_gan,NGP);
  if(flags->d_artificial_viscosity){
    t->requires(Task::OldDW, lb->p_qLabel,                d_gan,NGP);
  }
  
  t->computes( gSumSLabel );
  t->computes(lb->gInternalForceLabel);
  t->computes(lb->TotalVolumeDeformedLabel);
  t->computes(lb->gStressForSavingLabel);

  sched->addTask(t, patches, matls);
}
//______________________________________________________________________
//
void AMRMPM::scheduleComputeInternalForce_CFI(SchedulerP& sched,
                                              const PatchSet* patches,
                                              const MaterialSet* matls)
{
  const Level* fineLevel = getLevel(patches);
  int L_indx = fineLevel->getIndex();

  if(L_indx > 0 ){
    printSchedule(patches,cout_doing,"AMRMPM::scheduleComputeInternalForce_CFI");

    Task* t = scinew Task("AMRMPM::computeInternalForce_CFI",
                          this, &AMRMPM::computeInternalForce_CFI);

    Task::MaterialDomainSpec  ND  = Task::NormalDomain;

    /*`==========TESTING==========*/
    // Linear 1 coarse Level cells:
    // Gimp:  2 coarse level cells:
    int npc = d_nPaddingCells_Coarse;  
    /*===========TESTING==========`*/
 
#define allPatches 0
#define allMatls 0
    //__________________________________
    //  Note: were using nPaddingCells to extract the region of coarse level
    // particles around every fine patch.   Technically, these are ghost
    // cells but somehow it works.
    t->requires(Task::NewDW, lb->gZOILabel,     d_one_matl, Ghost::None,0);
    t->requires(Task::OldDW, lb->pXLabel,       allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    t->requires(Task::OldDW, lb->pStressLabel,  allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    t->requires(Task::OldDW, lb->pVolumeLabel,  allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    
    if(flags->d_artificial_viscosity){
      t->requires(Task::OldDW, lb->p_qLabel,    allPatches, Task::CoarseLevel,allMatls, ND, d_gac, npc);
    }
    
    t->modifies( gSumSLabel );
    t->modifies(lb->gInternalForceLabel);

    sched->addTask(t, patches, matls);
  }
}

//______________________________________________________________________
//
void AMRMPM::scheduleComputeAndIntegrateAcceleration(SchedulerP& sched,
                                                     const PatchSet* patches,
                                                     const MaterialSet* matls)
{
  const Level* level = getLevel(patches);
  if (!flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels())){
    return;
  }

  printSchedule(patches,cout_doing,
                "AMRMPM::scheduleComputeAndIntegrateAcceleration");

  Task* t = scinew Task("AMRMPM::computeAndIntegrateAcceleration",
                        this, &AMRMPM::computeAndIntegrateAcceleration);

  t->requires(Task::OldDW, d_sharedState->get_delt_label() );

  t->requires(Task::NewDW, lb->gMassLabel,          Ghost::None);
  t->requires(Task::NewDW, lb->gInternalForceLabel, Ghost::None);
  t->requires(Task::NewDW, lb->gExternalForceLabel, Ghost::None);
  t->requires(Task::NewDW, lb->gVelocityLabel,      Ghost::None);

  t->computes(lb->gVelocityStarLabel);
  t->computes(lb->gAccelerationLabel);

  sched->addTask(t, patches, matls);
}
//______________________________________________________________________
//
void AMRMPM::scheduleSetGridBoundaryConditions(SchedulerP& sched,
                                               const PatchSet* patches,
                                               const MaterialSet* matls)

{
  const Level* level = getLevel(patches);
  if (!flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels())){
    return;
  }

  printSchedule(patches,cout_doing,"AMRMPM::scheduleSetGridBoundaryConditions");

  Task* t=scinew Task("AMRMPM::setGridBoundaryConditions",
                      this,  &AMRMPM::setGridBoundaryConditions);

  const MaterialSubset* mss = matls->getUnion();
  t->requires(Task::OldDW, d_sharedState->get_delt_label() );
  
  t->modifies(             lb->gAccelerationLabel,     mss);
  t->modifies(             lb->gVelocityStarLabel,     mss);
  t->requires(Task::NewDW, lb->gVelocityLabel,   Ghost::None);

  if(!flags->d_doGridReset){
    t->requires(Task::OldDW, lb->gDisplacementLabel,    Ghost::None);
    t->computes(lb->gDisplacementLabel);
  }

  sched->addTask(t, patches, matls);
}
//______________________________________________________________________
//
void AMRMPM::scheduleInterpolateToParticlesAndUpdate(SchedulerP& sched,
                                                     const PatchSet* patches,
                                                     const MaterialSet* matls)

{
  const Level* level = getLevel(patches);
  if (!flags->doMPMOnLevel(level->getIndex(), level->getGrid()->numLevels())){
    return;
  }

  printSchedule(patches,cout_doing,
                "AMRMPM::scheduleInterpolateToParticlesAndUpdate");
  
  Task* t=scinew Task("AMRMPM::interpolateToParticlesAndUpdate",
                      this, &AMRMPM::interpolateToParticlesAndUpdate);
                
  t->requires(Task::OldDW, d_sharedState->get_delt_label() );

  t->requires(Task::NewDW, lb->gAccelerationLabel,              d_gac,NGN);
  t->requires(Task::NewDW, lb->gVelocityStarLabel,              d_gac,NGN);
  t->requires(Task::NewDW, lb->gTemperatureRateLabel,           d_gac,NGN);
  t->requires(Task::NewDW, lb->frictionalWorkLabel,             d_gac,NGN);
  
  t->requires(Task::OldDW, lb->pXLabel,                         d_gn);   
  t->requires(Task::OldDW, lb->pMassLabel,                      d_gn);   
  t->requires(Task::OldDW, lb->pParticleIDLabel,                d_gn);   
  t->requires(Task::OldDW, lb->pTemperatureLabel,               d_gn);   
  t->requires(Task::OldDW, lb->pVelocityLabel,                  d_gn);   
  t->requires(Task::OldDW, lb->pDispLabel,                      d_gn);   
  t->requires(Task::NewDW, lb->pSizeLabel_preReloc,             d_gn);   
  t->requires(Task::OldDW, lb->pVolumeLabel,                    d_gn);   
  t->requires(Task::OldDW, lb->pDefGradLabel,        d_gn);   
  //t->requires(Task::OldDW, lb->pLocalizedMPMLabel,              d_gn);

  t->computes(lb->pDispLabel_preReloc);
  t->computes(lb->pVelocityLabel_preReloc);
  t->computes(lb->pXLabel_preReloc);
  t->computes(lb->pParticleIDLabel_preReloc);
  t->computes(lb->pTemperatureLabel_preReloc);
  t->computes(lb->pTempPreviousLabel_preReloc); // for thermal stress
  t->computes(lb->pMassLabel_preReloc);
  //t->computes(lb->pXXLabel);

  // Carry Forward particle refinement flag
  if(flags->d_refineParticles){
    t->requires(Task::OldDW, lb->pRefinedLabel,                d_gn);
    t->computes(             lb->pRefinedLabel_preReloc);
  }

  t->requires(Task::OldDW, lb->NC_CCweightLabel, d_one_matl, Ghost::None);
  t->computes(             lb->NC_CCweightLabel, d_one_matl);

  t->computes(lb->TotalMassLabel);
  t->computes(lb->KineticEnergyLabel);
  t->computes(lb->ThermalEnergyLabel);
  t->computes(lb->CenterOfMassPositionLabel);
  t->computes(lb->TotalMomentumLabel);

#ifndef USE_DEBUG_TASK
  // debugging scalar
  if(flags->d_with_color) {
    t->requires(Task::OldDW, lb->pColorLabel,  d_gn);
    t->computes(lb->pColorLabel_preReloc);
  }
#endif  
  sched->addTask(t, patches, matls);
}
//______________________________________________________________________
//
void AMRMPM::scheduleComputeParticleScaleFactor(SchedulerP& sched,
                                                const PatchSet* patches,
                                                const MaterialSet* matls)

{
  if (!flags->doMPMOnLevel(getLevel(patches)->getIndex(),
                           getLevel(patches)->getGrid()->numLevels()))
    return;

  printSchedule(patches,cout_doing,
                "AMRMPM::scheduleComputeParticleScaleFactor");

  Task* t=scinew Task("AMRMPM::computeParticleScaleFactor",this,
                      &AMRMPM::computeParticleScaleFactor);

  t->requires(Task::NewDW, lb->pSizeLabel_preReloc,                Ghost::None);
  t->requires(Task::NewDW, lb->pDefGradLabel_preReloc,  Ghost::None);
  t->computes(lb->pScaleFactorLabel_preReloc);

  sched->addTask(t, patches, matls);
}
//______________________________________________________________________
//
void AMRMPM::scheduleFinalParticleUpdate(SchedulerP& sched,
                                         const PatchSet* patches,
                                         const MaterialSet* matls)
{
  if (!flags->doMPMOnLevel(getLevel(patches)->getIndex(),
                           getLevel(patches)->getGrid()->numLevels()))
    return;

  printSchedule(patches,cout_doing,"AMRMPM::scheduleFinalParticleUpdate");

  Task* t=scinew Task("AMRMPM::finalParticleUpdate",
                      this, &AMRMPM::finalParticleUpdate);

  t->requires(Task::OldDW, d_sharedState->get_delt_label() );

  t->requires(Task::NewDW, lb->pdTdtLabel,           d_gn);
  t->requires(Task::NewDW, lb->pMassLabel_preReloc,  d_gn);

  t->modifies(lb->pTemperatureLabel_preReloc);

  sched->addTask(t, patches, matls);
}
//______________________________________________________________________
//
void AMRMPM::scheduleAddParticles(SchedulerP& sched,
                                  const PatchSet* patches,
                                  const MaterialSet* matls)
{
  if (!flags->doMPMOnLevel(getLevel(patches)->getIndex(),
                           getLevel(patches)->getGrid()->numLevels()))
    return;

  printSchedule(patches,cout_doing,"AMRMPM::scheduleAddParticles");

  Task* t=scinew Task("AMRMPM::addParticles",this,
                      &AMRMPM::addParticles);

  t->modifies(lb->pParticleIDLabel_preReloc);
  t->modifies(lb->pXLabel_preReloc);
  t->modifies(lb->pVolumeLabel_preReloc);
  t->modifies(lb->pVelocityLabel_preReloc);
  t->modifies(lb->pMassLabel_preReloc);
  t->modifies(lb->pSizeLabel_preReloc);
  t->modifies(lb->pDispLabel_preReloc);
  t->modifies(lb->pStressLabel_preReloc);
  if (flags->d_with_color) {
    t->modifies(lb->pColorLabel_preReloc);
  }
  if (flags->d_useLoadCurves) {
    t->modifies(lb->pLoadCurveIDLabel_preReloc);
  }
  t->modifies(lb->pExtForceLabel_preReloc);
  t->modifies(lb->pTemperatureLabel_preReloc);
  t->modifies(lb->pTempPreviousLabel_preReloc);
  t->modifies(lb->pDefGradLabel_preReloc);
  t->modifies(lb->pRefinedLabel_preReloc);
    if(flags->d_computeScaleFactor){
  t->modifies(lb->pScaleFactorLabel_preReloc);
    }
  t->modifies(lb->pLastLevelLabel_preReloc);
  t->modifies(lb->pVelGradLabel_preReloc);
  t->modifies(lb->MPMRefineCellLabel, d_one_matl);

  // For body dorce + coriolis importance
  t->modifies(lb->pBodyForceAccLabel_preReloc);
  t->modifies(lb->pCoriolisImportanceLabel_preReloc);

  t->requires(Task::OldDW, lb->pCellNAPIDLabel, d_one_matl, Ghost::None);
  t->computes(             lb->pCellNAPIDLabel, d_one_matl);

    int numMatls = d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
      ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
      cm->addSplitParticlesComputesAndRequires(t, mpm_matl, patches);
    }

  sched->addTask(t, patches, matls);
}

//______________________________________________________________________
//
void AMRMPM::scheduleReduceFlagsExtents(SchedulerP& sched,
                                        const PatchSet* patches,
                                        const MaterialSet* matls)
{
  const Level* level = getLevel(patches);

//  if( !level->hasFinerLevel() ){
  if(level->getIndex() > 0 ){
    printSchedule(patches,cout_doing,"AMRMPM::scheduleReduceFlagsExtents");

    Task* t=scinew Task("AMRMPM::reduceFlagsExtents",
                        this, &AMRMPM::reduceFlagsExtents);

    t->requires(Task::NewDW, lb->MPMRefineCellLabel, d_one_matl, Ghost::None);

    t->computes(RefineFlagXMaxLabel);
    t->computes(RefineFlagXMinLabel);
    t->computes(RefineFlagYMaxLabel);
    t->computes(RefineFlagYMinLabel);
    t->computes(RefineFlagZMaxLabel);
    t->computes(RefineFlagZMinLabel);

    sched->addTask(t, patches, matls);
  }
}

//______________________________________________________________________
////
void AMRMPM::scheduleRefine(const PatchSet* patches, SchedulerP& sched)
{
  printSchedule(patches,cout_doing,"AMRMPM::scheduleRefine");
  Task* t = scinew Task("AMRMPM::refineGrid", this, &AMRMPM::refineGrid);

  t->computes(lb->pXLabel);
  t->computes(lb->pDispLabel);
  t->computes(lb->pMassLabel);
  t->computes(lb->pVolumeLabel);
  t->computes(lb->pTemperatureLabel);
  t->computes(lb->pTempPreviousLabel); // for thermal  stress analysis
  t->computes(lb->pdTdtLabel);
  t->computes(lb->pVelocityLabel);
  t->computes(lb->pExternalForceLabel);
  t->computes(lb->pParticleIDLabel);
  //t->computes(lb->pDefGradLabel);
  t->computes(lb->pVolumeLabel);
  t->computes(lb->pStressLabel);
  t->computes(lb->pLastLevelLabel);
  t->computes(lb->pRefinedLabel);
  t->computes(lb->pSizeLabel);
  //t->computes(lb->pVelGradLabel);
  t->computes(lb->pCellNAPIDLabel, d_one_matl);
  t->computes(lb->NC_CCweightLabel);

  // Debugging Scalar
  if (flags->d_with_color) {
    t->computes(lb->pColorLabel);
  }

  if (flags->d_useLoadCurves) {
    // Computes the load curve ID associated with each particle
    t->computes(lb->pLoadCurveIDLabel);
  }

  if (flags->d_reductionVars->accStrainEnergy) {
    // Computes accumulated strain energy
    t->computes(lb->AccStrainEnergyLabel);
  }
  
  if(flags->d_artificial_viscosity){
    t->computes(lb->p_qLabel);
  }

  int numMPM = d_sharedState->getNumMPMMatls();
  for(int m = 0; m < numMPM; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);

    // Add requires and computes for vel grad/def grad
    d_defGradComputer->addComputesOnly(t, mpm_matl, patches);

    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    cm->addInitialComputesAndRequires(t, mpm_matl, patches);
  }

  t->computes(d_sharedState->get_delt_label(),getLevel(patches));

  // For body dorce + coriolis importance
  t->computes(lb->pBodyForceAccLabel);
  t->computes(lb->pCoriolisImportanceLabel);

  sched->addTask(t, patches, d_sharedState->allMPMMaterials());
}
//______________________________________________________________________
//
void AMRMPM::scheduleRefineInterface(const LevelP& /*fineLevel*/, 
                                     SchedulerP& /*scheduler*/,
                                     bool, bool)
{
  // do nothing for now
}
//______________________________________________________________________
//
void AMRMPM::scheduleCoarsen(const LevelP& coarseLevel,
                             SchedulerP& sched)
{
  // Coarsening the refineCell data so that errorEstimate will have it
  // on all levels
  Task* task = scinew Task("AMRMPM::coarsen",this,
                           &AMRMPM::coarsen);

  Task::MaterialDomainSpec oims = Task::OutOfDomain;  //outside of ice matlSet.  
  const MaterialSet* all_matls = d_sharedState->allMaterials();
  const PatchSet* patch_set = coarseLevel->eachPatch();

  bool  fat = true;  // possibly (F)rom (A)nother (T)askgraph

  task->requires(Task::NewDW, lb->MPMRefineCellLabel,
               0, Task::FineLevel,  d_one_matl,oims, d_gn, 0, fat);

  task->requires(Task::NewDW, RefineFlagXMaxLabel);
  task->requires(Task::NewDW, RefineFlagXMinLabel);
  task->requires(Task::NewDW, RefineFlagYMaxLabel);
  task->requires(Task::NewDW, RefineFlagYMinLabel);
  task->requires(Task::NewDW, RefineFlagZMaxLabel);
  task->requires(Task::NewDW, RefineFlagZMinLabel);

  task->modifies(lb->MPMRefineCellLabel, d_one_matl, oims, fat);

  sched->addTask(task, patch_set, all_matls);
}
//______________________________________________________________________
//
void AMRMPM::coarsen(const ProcessorGroup*,
                     const PatchSubset* patches,
                     const MaterialSubset* matls,
                     DataWarehouse*,
                     DataWarehouse* new_dw)
{
  const Level* coarseLevel = getLevel(patches);
  const Level* fineLevel = coarseLevel->getFinerLevel().get_rep();
  GridP grid = coarseLevel->getGrid();
  int numLevels = grid->numLevels();
  IntVector RR = fineLevel->getRefinementRatio();

  for(int p=0;p<patches->size();p++){
    const Patch* coarsePatch = patches->get(p);
    cout_doing << "  patch " << coarsePatch->getID()<< endl;

    CCVariable<double> refineCell;
    new_dw->getModifiable(refineCell, lb->MPMRefineCellLabel, 0, coarsePatch);
    bool computesAve = true;

    fineToCoarseOperator<double>(refineCell, computesAve,
                                 lb->MPMRefineCellLabel, 0,   new_dw,
                                 coarsePatch, coarseLevel, fineLevel);

    if( coarseLevel->getIndex() == numLevels - 2 ){
//    cout << "coarseLevelIndex = " << coarseLevel->getIndex() << endl;
      max_vartype xmax,ymax,zmax;
      min_vartype xmin,ymin,zmin;
      new_dw->get(xmax, RefineFlagXMaxLabel);
      new_dw->get(ymax, RefineFlagYMaxLabel);
      new_dw->get(zmax, RefineFlagZMaxLabel);
      new_dw->get(xmin, RefineFlagXMinLabel);
      new_dw->get(ymin, RefineFlagYMinLabel);
      new_dw->get(zmin, RefineFlagZMinLabel);

//    cout << "xmax = " << xmax << endl;
//    cout << "ymax = " << ymax << endl;
//    cout << "zmax = " << zmax << endl;
//    cout << "xmin = " << xmin << endl;
//    cout << "ymin = " << ymin << endl;
//    cout << "zmin = " << zmin << endl;

      IntVector fineXYZMaxMin(xmax,ymax,zmax);
      IntVector fineXYZMinMax(xmin,ymin,zmin);
      IntVector fineXZMaxYMin(xmax,ymin,zmax);
      IntVector fineXZMinYMax(xmin,ymax,zmin);
      IntVector fineXYMaxZMin(xmax,ymax,zmin);
      IntVector fineXYMinZMax(xmin,ymin,zmax);
      IntVector fineXMinYZMax(xmin,ymax,zmax);
      IntVector fineXMaxYZMin(xmax,ymin,zmin);

      IntVector coarseMinMax[8];

      coarseMinMax[0] = fineXYZMaxMin/RR;
      coarseMinMax[1] = fineXYZMinMax/RR;

      // Set the refine flags to 1 in all cells in the interior of the minimum
      // and maximum to ensure a rectangular region is refined.
      int imax,jmax,kmax,imin,jmin,kmin;
      imax = coarseMinMax[0].x();
      jmax = coarseMinMax[0].y();
      kmax = coarseMinMax[0].z();
      imin = coarseMinMax[1].x();
      jmin = coarseMinMax[1].y();
      kmin = coarseMinMax[1].z();
      for(CellIterator iter=coarsePatch->getCellIterator(); !iter.done();iter++){
        IntVector c = *iter;
        if(c.x() >= imin && c.x() <= imax &&
           c.y() >= jmin && c.y() <= jmax &&
           c.z() >= kmin && c.z() <= kmax){
          refineCell[c]=1;
        }
      }

    }  // end if level
  } // end patch
}

//______________________________________________________________________
// Schedule to mark flags for AMR regridding
void AMRMPM::scheduleErrorEstimate(const LevelP& coarseLevel,
                                   SchedulerP& sched)
{
//  cout << "scheduleErrorEstimate" << endl;
  printSchedule(coarseLevel,cout_doing,"AMRMPM::scheduleErrorEstimate");
  
  Task* task = scinew Task("AMRMPM::errorEstimate", this, 
                           &AMRMPM::errorEstimate);

  task->modifies(d_sharedState->get_refineFlag_label(),
                 d_sharedState->refineFlagMaterials());
  task->modifies(d_sharedState->get_refinePatchFlag_label(),
                 d_sharedState->refineFlagMaterials());
  task->requires(Task::NewDW, lb->MPMRefineCellLabel, Ghost::None);

  sched->addTask(task, coarseLevel->eachPatch(),
                 d_sharedState->allMPMMaterials());
}
//______________________________________________________________________
// Schedule to mark initial flags for AMR regridding
void AMRMPM::scheduleInitialErrorEstimate(const LevelP& coarseLevel,
                                          SchedulerP& sched)
{
//  cout << "scheduleInitialErrorEstimate" << endl;
//  cout << "Doing nothing for now" << endl;
  
//  scheduleErrorEstimate(coarseLevel, sched);
}

//______________________________________________________________________
//
void AMRMPM::printParticleCount(const ProcessorGroup* pg,
                                const PatchSubset*,
                                const MaterialSubset*,
                                DataWarehouse*,
                                DataWarehouse* new_dw)
{
  sumlong_vartype pcount;
  new_dw->get(pcount, lb->partCountLabel);
  
  if(pg->myrank() == 0){
    std::cout << "Created " << (long) pcount << " total particles" << std::endl;
  }
}
//______________________________________________________________________
//
void AMRMPM::actuallyInitialize(const ProcessorGroup*,
                                const PatchSubset* patches,
                                const MaterialSubset* matls,
                                DataWarehouse*,
                                DataWarehouse* new_dw)
{
  const Level* level = getLevel(patches);
  int levelIndex = level->getIndex();
  particleIndex totalParticles=0;
  
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    
    
    printTask(patches, patch,cout_doing,"Doing AMRMPM::actuallyInitialize");

    CCVariable<int> cellNAPID;
    new_dw->allocateAndPut(cellNAPID, lb->pCellNAPIDLabel, 0, patch);
    cellNAPID.initialize(0);

    NCVariable<double> NC_CCweight;
    new_dw->allocateAndPut(NC_CCweight, lb->NC_CCweightLabel,    0, patch);

    //__________________________________
    // - Initialize NC_CCweight = 0.125
    // - Find the walls with symmetry BC and double NC_CCweight
    NC_CCweight.initialize(0.125); 
    for(Patch::FaceType face = Patch::startFace; face <= Patch::endFace;
        face=Patch::nextFace(face)){
      int mat_id = 0;

      if (patch->haveBC(face,mat_id,"symmetry","Symmetric")) {
        for(CellIterator iter = patch->getFaceIterator(face,Patch::FaceNodes);
                                                  !iter.done(); iter++) {
          NC_CCweight[*iter] = 2.0*NC_CCweight[*iter];
        }
      }
    }

    for(int m=0;m<matls->size();m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int indx = mpm_matl->getDWIndex();
      
      if(!flags->d_doGridReset){
        NCVariable<Vector> gDisplacement;
        new_dw->allocateAndPut(gDisplacement,lb->gDisplacementLabel,indx,patch);
        gDisplacement.initialize(Vector(0.));
      }
      
      particleIndex numParticles = mpm_matl->createParticles(cellNAPID, patch, new_dw);

      totalParticles+=numParticles;

      // Initialize deformation gradient
      d_defGradComputer->initializeGradient(patch, mpm_matl, new_dw);

      mpm_matl->getConstitutiveModel()->initializeCMData(patch,mpm_matl,new_dw);
      
      //__________________________________
      // color particles according to what level they're on
      if (flags->d_with_color) {
        ParticleSubset* pset = new_dw->getParticleSubset(indx, patch);
        ParticleVariable<double> pColor;
        new_dw->getModifiable(pColor, lb->pColorLabel, pset);

        ParticleSubset::iterator iter = pset->begin();
        for(;iter != pset->end(); iter++){
          particleIndex idx = *iter;
          pColor[idx] =levelIndex;
        }
      }
    }  // matl loop
  }

  // Determine dimensionality for particle splitting
  IntVector lowNode, highNode;
  level->findNodeIndexRange(lowNode, highNode);
  string interp_type = flags->d_interpolator_type;
  d_ndim=3;
  if((interp_type=="linear" && (highNode.z() - lowNode.z()==2)) ||
     (interp_type=="gimp"   && (highNode.z() - lowNode.z()==4)) ||
     (interp_type=="cpdi"   && (highNode.z() - lowNode.z()==4))){
     d_ndim=2;
  }

  if (flags->d_reductionVars->accStrainEnergy) {
    // Initialize the accumulated strain energy
    new_dw->put(max_vartype(0.0), lb->AccStrainEnergyLabel);
  }

  new_dw->put(sumlong_vartype(totalParticles), lb->partCountLabel);

}
//______________________________________________________________________
//
void AMRMPM::actuallyComputeStableTimestep(const ProcessorGroup*,
                                           const PatchSubset*,
                                           const MaterialSubset*,
                                           DataWarehouse*,
                                           DataWarehouse*)
{
}
//______________________________________________________________________
//  This task computes the partition of unity for each particle
//
void AMRMPM::partitionOfUnity(const ProcessorGroup*,
                              const PatchSubset* patches,
                              const MaterialSubset* ,
                              DataWarehouse* old_dw,
                              DataWarehouse* new_dw)
{
  const Level* curLevel = getLevel(patches);
  int curLevelIndex = curLevel->getIndex();
  Vector dX = curLevel->dCell();
  Vector dX_fine   = 0.5*dX;
  Vector dX_coarse = 2.0*dX;
  Vector RRC = dX/dX_coarse;
  Vector RRF = dX/dX_fine;
  if(curLevel->hasFinerLevel()){
    dX_fine = curLevel->getFinerLevel()->dCell();
    RRF=dX/dX_fine;
    RRC=Vector(1./RRF.x(),1./RRF.y(),1./RRF.z());
  }
  if(curLevel->hasCoarserLevel()){
    dX_coarse = curLevel->getCoarserLevel()->dCell();
    RRC=dX/dX_coarse;
    RRF=Vector(1./RRC.x(),1./RRC.y(),1./RRC.z());
  }

  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    printTask(patches,patch,cout_doing,"Doing AMRMPM::partitionOfUnity");

    // Create and Initialize refine flags to be modified later
    CCVariable<double> refineCell;
    new_dw->allocateAndPut(refineCell, lb->MPMRefineCellLabel, 0, patch);
    refineCell.initialize(0.0);

    int numMatls = d_sharedState->getNumMPMMatls();
    ParticleInterpolator* interpolator = flags->d_interpolator->clone(patch);
    vector<IntVector> ni(interpolator->size());
    vector<double> S(interpolator->size());
    const Matrix3 nU;

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
      constParticleVariable<Point> px;
      constParticleVariable<Matrix3> psize;
      constParticleVariable<int>plastlevel;
      ParticleVariable<Matrix3> psizenew;
      ParticleVariable<int>plastlevelnew;
      ParticleVariable<double>partitionUnity;
    
      old_dw->get(px,                lb->pXLabel,          pset);
      old_dw->get(psize,             lb->pSizeLabel,       pset);
      old_dw->get(plastlevel,        lb->pLastLevelLabel,  pset);
      new_dw->allocateAndPut(psizenew,       lb->pSizeLabel_preReloc,     pset);
      new_dw->allocateAndPut(plastlevelnew,  lb->pLastLevelLabel_preReloc,pset);
      new_dw->allocateAndPut(partitionUnity, lb->pPartitionUnityLabel,    pset);
      
      for (ParticleSubset::iterator iter = pset->begin();
           iter != pset->end(); iter++){
        particleIndex idx = *iter;

        Matrix3 ps = psize[idx];

        if(curLevelIndex<plastlevel[idx]){
          psizenew[idx]=Matrix3(ps(0,0)*RRC.x(),ps(0,1)*RRC.x(),ps(0,2)*RRC.x(),
                                ps(1,0)*RRC.y(),ps(1,1)*RRC.y(),ps(1,2)*RRC.y(),
                                ps(2,0)*RRC.z(),ps(2,1)*RRC.z(),ps(2,2)*RRC.z());
        } else if(curLevelIndex>plastlevel[idx]){
          psizenew[idx]=Matrix3(ps(0,0)*RRF.x(),ps(0,1)*RRF.x(),ps(0,2)*RRF.x(),
                                ps(1,0)*RRF.y(),ps(1,1)*RRF.y(),ps(1,2)*RRF.y(),
                                ps(2,0)*RRF.z(),ps(2,1)*RRF.z(),ps(2,2)*RRF.z());
        } else {
          psizenew[idx]  = psize[idx];
        }

        plastlevelnew[idx]= curLevelIndex;

        partitionUnity[idx] = 0;

        int NN = interpolator->findCellAndWeights(px[idx],ni,S,psize[idx],nU);

        for(int k = 0; k < NN; k++) {
          partitionUnity[idx] += S[k];
        }
      }
    }  // loop over materials
    delete interpolator;
  }  // loop over patches
}
//______________________________________________________________________
//
void AMRMPM::interpolateParticlesToGrid(const ProcessorGroup*,
                                        const PatchSubset* patches,
                                        const MaterialSubset* ,
                                        DataWarehouse* old_dw,
                                        DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    printTask(patches,patch,cout_doing,
              "Doing AMRMPM::interpolateParticlesToGrid");

    int numMatls = d_sharedState->getNumMPMMatls();
    ParticleInterpolator* interpolator = flags->d_interpolator->clone(patch);
    vector<IntVector> ni(interpolator->size());
    vector<double> S(interpolator->size());

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      // Create arrays for the particle data
      constParticleVariable<Point>  px;
      constParticleVariable<double> pmass, pvolume, pTemperature;
      constParticleVariable<double> pConcentration;
      constParticleVariable<Vector> pvelocity, pexternalforce;
      constParticleVariable<Matrix3> psize;
      constParticleVariable<Matrix3> pDefGrad;
      constParticleVariable<Matrix3> pStress;
      constParticleVariable<Matrix3> pVelGrad;

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch,
                                                       d_gan, NGP, lb->pXLabel);

      old_dw->get(px,                   lb->pXLabel,                  pset);
      old_dw->get(pmass,                lb->pMassLabel,               pset);
      old_dw->get(pvolume,              lb->pVolumeLabel,             pset);
      old_dw->get(pvelocity,            lb->pVelocityLabel,           pset);
      old_dw->get(pTemperature,         lb->pTemperatureLabel,        pset);

      new_dw->get(psize,                lb->pSizeLabel_preReloc,      pset);
      old_dw->get(pDefGrad,  lb->pDefGradLabel, pset);
      new_dw->get(pexternalforce,       lb->pExtForceLabel_preReloc,  pset);
      if (flags->d_GEVelProj){
        old_dw->get(pVelGrad,           lb->pVelGradLabel,            pset);
      }
      // Create arrays for the grid data
      NCVariable<double> gmass;
      NCVariable<double> gvolume;
      NCVariable<Vector> gvelocity;
      NCVariable<Vector> gexternalforce;
      NCVariable<double> gTemperature;
      NCVariable<double> gTemperatureRate;
      NCVariable<double> gconcentration;
      NCVariable<double> gextscalarflux;
      NCVariable<double> ghydrostaticstress;

      new_dw->allocateAndPut(gmass,            lb->gMassLabel,           dwi,patch);
      new_dw->allocateAndPut(gvolume,          lb->gVolumeLabel,         dwi,patch);
      new_dw->allocateAndPut(gvelocity,        lb->gVelocityLabel,       dwi,patch);
      new_dw->allocateAndPut(gTemperature,     lb->gTemperatureLabel,    dwi,patch);
      new_dw->allocateAndPut(gTemperatureRate, lb->gTemperatureRateLabel,dwi,patch);
      new_dw->allocateAndPut(gexternalforce,   lb->gExternalForceLabel,  dwi,patch);

      gmass.initialize(d_SMALL_NUM_MPM);
      gvolume.initialize(d_SMALL_NUM_MPM);
      gvelocity.initialize(Vector(0,0,0));
      gexternalforce.initialize(Vector(0,0,0));
      gTemperature.initialize(0);
      gTemperatureRate.initialize(0);

      Vector pmom;

      for (ParticleSubset::iterator iter  = pset->begin();
           iter != pset->end(); iter++){
        particleIndex idx = *iter;

        // Get the node indices that surround the cell
        int NN = interpolator->findCellAndWeights(px[idx],ni,S,psize[idx],
                                         pDefGrad[idx]);

        pmom = pvelocity[idx]*pmass[idx];

        // Add each particles contribution to the local mass & velocity 
        IntVector node;
        for(int k = 0; k < NN; k++) {
          node = ni[k];
          if(patch->containsNode(node)) {
            if (flags->d_GEVelProj){
              Point gpos = patch->getNodePosition(node);
              Vector distance = px[idx] - gpos;
              Vector pvel_ext = pvelocity[idx] - pVelGrad[idx]*distance;
              pmom = pvel_ext*pmass[idx];
            }
            gmass[node]          += pmass[idx]                     * S[k];
            gvelocity[node]      += pmom                           * S[k];
            gvolume[node]        += pvolume[idx]                   * S[k];
            gexternalforce[node] += pexternalforce[idx]            * S[k];
            gTemperature[node]   += pTemperature[idx] * pmass[idx] * S[k];
          }
        }
      }  // End of particle loop


      // gvelocity and gtemperature are divided by gmass in 
      // AMRMPM::NormalizeNodalVelTempConc() task
      
    }  // End loop over materials
    delete interpolator;
  }  // End loop over patches
}

//______________________________________________________________________
//  At the CFI fine patch nodes add contributions from the coarse level particles.
void AMRMPM::interpolateParticlesToGrid_CFI(const ProcessorGroup*,
                                            const PatchSubset* finePatches,
                                            const MaterialSubset* ,
                                            DataWarehouse* old_dw,
                                            DataWarehouse* new_dw)
{
  const Level* fineLevel = getLevel(finePatches);
  const Level* coarseLevel = fineLevel->getCoarserLevel().get_rep();
  IntVector refineRatio(fineLevel->getRefinementRatio());
  
  for(int fp=0; fp<finePatches->size(); fp++){
    const Patch* finePatch = finePatches->get(fp);
    printTask(finePatches,finePatch,cout_doing,
              "Doing AMRMPM::interpolateParticlesToGrid_CFI");

    int numMatls = d_sharedState->getNumMPMMatls();
    ParticleInterpolator* interpolator =flags->d_interpolator->clone(finePatch);

    constNCVariable<Stencil7> zoi_fine;
    new_dw->get(zoi_fine, lb->gZOILabel, 0, finePatch, d_gn, 0 );

    // Determine extents for coarser level particle data
    // Linear Interpolation:  1 layer of coarse level cells
    // Gimp Interpolation:    2 layers
/*`==========TESTING==========*/
    IntVector nLayers(d_nPaddingCells_Coarse,
                      d_nPaddingCells_Coarse, 
                      d_nPaddingCells_Coarse);
    IntVector nPaddingCells = nLayers * (fineLevel->getRefinementRatio());
    //cout << " nPaddingCells " << nPaddingCells << "nLayers " << nLayers << endl;
/*===========TESTING==========`*/

    int nGhostCells = 0;
    bool returnExclusiveRange=false;
    IntVector cl_tmp, ch_tmp, fl, fh;

    getCoarseLevelRange(finePatch, coarseLevel, cl_tmp, ch_tmp, fl, fh, 
                        nPaddingCells, nGhostCells,returnExclusiveRange);
                        
    //  expand cl_tmp when a neighor patch exists.
    //  This patch owns the low nodes.  You need particles
    //  from the neighbor patch.
    cl_tmp -= finePatch->neighborsLow() * nLayers;

    // find the coarse patches under the fine patch.
    // You must add a single layer of padding cells.
    int padding = 1;
    Level::selectType coarsePatches;
    finePatch->getOtherLevelPatches55902(-1, coarsePatches, padding);

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      // get fine level nodal data
      NCVariable<double> gMass_fine;
      NCVariable<double> gVolume_fine;
      NCVariable<Vector> gVelocity_fine;
      NCVariable<Vector> gExternalforce_fine;
      NCVariable<double> gTemperature_fine;
      NCVariable<double> gConc_fine;
      NCVariable<double> gExtScalarFlux_fine;
      NCVariable<double> gHStress_fine;

      new_dw->getModifiable(gMass_fine,          lb->gMassLabel,         dwi,finePatch);
      new_dw->getModifiable(gVolume_fine,        lb->gVolumeLabel,       dwi,finePatch);
      new_dw->getModifiable(gVelocity_fine,      lb->gVelocityLabel,     dwi,finePatch);
      new_dw->getModifiable(gTemperature_fine,   lb->gTemperatureLabel,  dwi,finePatch);
      new_dw->getModifiable(gExternalforce_fine, lb->gExternalForceLabel,dwi,finePatch);

      // loop over the coarse patches under the fine patches.
      for(int cp=0; cp<coarsePatches.size(); cp++){
        const Patch* coarsePatch = coarsePatches[cp];
        
        // get coarse level particle data
        constParticleVariable<Point>  pX_coarse;
        constParticleVariable<double> pMass_coarse;
        constParticleVariable<double> pVolume_coarse;
        constParticleVariable<double> pTemperature_coarse;
        constParticleVariable<Vector> pVelocity_coarse;
        constParticleVariable<Vector> pExternalforce_coarse;
        constParticleVariable<double> pConc_coarse;
        constParticleVariable<double> pExtScalarFlux_c;
        constParticleVariable<Matrix3> pStress_coarse;

        // coarseLow and coarseHigh cannot lie outside of the coarse patch
        IntVector cl = Max(cl_tmp, coarsePatch->getCellLowIndex());
        IntVector ch = Min(ch_tmp, coarsePatch->getCellHighIndex());
        
        ParticleSubset* pset=0;
        
        pset = old_dw->getParticleSubset(dwi, cl, ch, coarsePatch ,lb->pXLabel);
#if 0
        cout << "  coarseLevel: " << coarseLevel->getIndex() << endl;
        cout << " cl_tmp: "<< cl_tmp << " ch_tmp: " << ch_tmp << endl;
        cout << " cl:     " << cl    << " ch:     " << ch<< " fl: " << fl << " fh " << fh << endl;
        cout << "  " << *pset << endl;
#endif        
        old_dw->get(pX_coarse,             lb->pXLabel,                  pset);
        old_dw->get(pMass_coarse,          lb->pMassLabel,               pset);
        old_dw->get(pVolume_coarse,        lb->pVolumeLabel,             pset);
        old_dw->get(pVelocity_coarse,      lb->pVelocityLabel,           pset);
        old_dw->get(pTemperature_coarse,   lb->pTemperatureLabel,        pset);
        new_dw->get(pExternalforce_coarse, lb->pExtForceLabel_preReloc,  pset);

        for (ParticleSubset::iterator iter  = pset->begin();
             iter != pset->end(); iter++){
          particleIndex idx = *iter;

          // Get the node indices that surround the fine patch cell
          vector<IntVector> ni;
          vector<double> S;

          interpolator->findCellAndWeights_CFI(pX_coarse[idx],ni,S,zoi_fine);

          Vector pmom = pVelocity_coarse[idx]*pMass_coarse[idx];

          // Add each particle's contribution to the local mass & velocity 
          IntVector fineNode;

          for(int k = 0; k < (int) ni.size(); k++) {
            fineNode = ni[k];

            gMass_fine[fineNode]          += pMass_coarse[idx]          * S[k];
            gVelocity_fine[fineNode]      += pmom                       * S[k];
            gVolume_fine[fineNode]        += pVolume_coarse[idx]        * S[k];
            gExternalforce_fine[fineNode] += pExternalforce_coarse[idx] * S[k];
            gTemperature_fine[fineNode]   += pTemperature_coarse[idx] 
              * pMass_coarse[idx] * S[k];
          }
        }  // End of particle loop
      }  // loop over coarse patches
    }  // End loop over materials  
    delete interpolator;
  }  // End loop over fine patches
}

//______________________________________________________________________
//         G I M P
//  At the CFI fine patch nodes add contributions from the coarse level particles.
void AMRMPM::interpolateParticlesToGrid_CFI_GIMP(const ProcessorGroup*,
                                                 const PatchSubset* finePatches,
                                                 const MaterialSubset* ,
                                                 DataWarehouse* old_dw,
                                                 DataWarehouse* new_dw)
{
  const Level* fineLevel = getLevel(finePatches);
  const Level* coarseLevel = fineLevel->getCoarserLevel().get_rep();
  IntVector refineRatio(fineLevel->getRefinementRatio());
  
  for(int fp=0; fp<finePatches->size(); fp++){
    const Patch* finePatch = finePatches->get(fp);
    printTask(finePatches,finePatch,cout_doing,
                          "Doing AMRMPM::interpolateParticlesToGrid_CFI_GIMP");

    int numMatls = d_sharedState->getNumMPMMatls();
    ParticleInterpolator* interpolator =flags->d_interpolator->clone(finePatch);

    constNCVariable<Stencil7> zoi_fine;
    new_dw->get(zoi_fine, lb->gZOILabel, 0, finePatch, d_gn, 0 );

    // Determine extents for coarser level particle data
    // Linear Interpolation:  1 layer of coarse level cells
    // Gimp Interpolation:    2 layers
/*`==========TESTING==========*/
    IntVector nLayers(d_nPaddingCells_Coarse,
                      d_nPaddingCells_Coarse, 
                      d_nPaddingCells_Coarse);
    IntVector nPaddingCells = nLayers * (fineLevel->getRefinementRatio());
    //cout << " nPaddingCells " << nPaddingCells << "nLayers " << nLayers << endl;
/*===========TESTING==========`*/

    int nGhostCells = 0;
    bool returnExclusiveRange=false;
    IntVector cl_tmp, ch_tmp, fl, fh;

    getCoarseLevelRange(finePatch, coarseLevel, cl_tmp, ch_tmp, fl, fh, 
                        nPaddingCells, nGhostCells,returnExclusiveRange);
                        
    //  expand cl_tmp when a neighor patch exists.
    //  This patch owns the low nodes.  You need particles
    //  from the neighbor patch.
    cl_tmp -= finePatch->neighborsLow() * nLayers;

    // find the coarse patches under the fine patch.
    // You must add a single layer of padding cells.
    int padding = 1;
    Level::selectType coarsePatches;
    finePatch->getOtherLevelPatches55902(-1, coarsePatches, padding);

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      // get fine level nodal data
      NCVariable<double> gMass_fine;
      NCVariable<double> gVolume_fine;
      NCVariable<Vector> gVelocity_fine;
      NCVariable<Vector> gExternalforce_fine;
      NCVariable<double> gTemperature_fine;
      NCVariable<double> gConc_fine;
      NCVariable<double> gExtScalarFlux_fine;
      NCVariable<double> gHStress_fine;

      new_dw->getModifiable(gMass_fine,          lb->gMassLabel,         dwi,finePatch);
      new_dw->getModifiable(gVolume_fine,        lb->gVolumeLabel,       dwi,finePatch);
      new_dw->getModifiable(gVelocity_fine,      lb->gVelocityLabel,     dwi,finePatch);
      new_dw->getModifiable(gTemperature_fine,   lb->gTemperatureLabel,  dwi,finePatch);
      new_dw->getModifiable(gExternalforce_fine, lb->gExternalForceLabel,dwi,finePatch);

      // loop over the coarse patches under the fine patches.
      for(int cp=0; cp<coarsePatches.size(); cp++){
        const Patch* coarsePatch = coarsePatches[cp];
        
        // get coarse level particle data
        constParticleVariable<Point>  pX_coarse;
        constParticleVariable<double> pMass_coarse;
        constParticleVariable<double> pVolume_coarse;
        constParticleVariable<double> pTemperature_coarse;
        constParticleVariable<Vector> pVelocity_coarse;
        constParticleVariable<Vector> pExternalforce_coarse;
        constParticleVariable<double> pConc_coarse;
        constParticleVariable<double> pExtScalarFlux_c;
        constParticleVariable<Matrix3> pStress_coarse;

        // coarseLow and coarseHigh cannot lie outside of the coarse patch
        IntVector cl = Max(cl_tmp, coarsePatch->getCellLowIndex());
        IntVector ch = Min(ch_tmp, coarsePatch->getCellHighIndex());
        
        ParticleSubset* pset=0;
        
        pset = old_dw->getParticleSubset(dwi, cl, ch, coarsePatch ,lb->pXLabel);
#if 0
        cout << "  coarseLevel: " << coarseLevel->getIndex() << endl;
        cout << " cl_tmp: "<< cl_tmp << " ch_tmp: " << ch_tmp << endl;
        cout << " cl:     " << cl    << " ch:     " << ch<< " fl: " << fl << " fh " << fh << endl;
        cout << "  " << *pset << endl;
#endif        
        old_dw->get(pX_coarse,             lb->pXLabel,                  pset);
        old_dw->get(pMass_coarse,          lb->pMassLabel,               pset);
        old_dw->get(pVolume_coarse,        lb->pVolumeLabel,             pset);
        old_dw->get(pVelocity_coarse,      lb->pVelocityLabel,           pset);
        old_dw->get(pTemperature_coarse,   lb->pTemperatureLabel,        pset);
        new_dw->get(pExternalforce_coarse, lb->pExtForceLabel_preReloc,  pset);

        for (ParticleSubset::iterator iter  = pset->begin();
                                      iter != pset->end(); iter++){
          particleIndex idx = *iter;

          // Get the node indices that surround the fine patch cell
          vector<IntVector> ni;
          vector<double> S;

          interpolator->findCellAndWeights_CFI(pX_coarse[idx],ni,S,zoi_fine);

          Vector pmom = pVelocity_coarse[idx]*pMass_coarse[idx];

          // Add each particle's contribution to the local mass & velocity 
          IntVector fineNode;

          for(int k = 0; k < (int) ni.size(); k++) {
            fineNode = ni[k];

            gMass_fine[fineNode]          += pMass_coarse[idx]          * S[k];
            gVelocity_fine[fineNode]      += pmom                       * S[k];
            gVolume_fine[fineNode]        += pVolume_coarse[idx]        * S[k];
            gExternalforce_fine[fineNode] += pExternalforce_coarse[idx] * S[k];
            gTemperature_fine[fineNode]   += pTemperature_coarse[idx] 
                                           * pMass_coarse[idx] * S[k];
          }
        }  // End of particle loop
      }  // loop over coarse patches
    }  // End loop over materials  
    delete interpolator;
  }  // End loop over fine patches
}

//______________________________________________________________________
//  copy the fine level nodal data to the underlying coarse nodes at the CFI.
void AMRMPM::coarsenNodalData_CFI(const ProcessorGroup*,
                                  const PatchSubset* coarsePatches,
                                  const MaterialSubset* ,
                                  DataWarehouse* old_dw,
                                  DataWarehouse* new_dw,
                                  const coarsenFlag flag)
{
  Level::selectType CFI_coarsePatches;
  Level::selectType CFI_finePatches;
  
  coarseLevelCFI_Patches(coarsePatches, CFI_coarsePatches, CFI_finePatches);
  
  //__________________________________
  // From the coarse patch look up to the fine patches that have
  // coarse fine interfaces.
  const Level* coarseLevel = getLevel(coarsePatches);
  
  for (auto coarsePatch : CFI_coarsePatches) {

    string txt = "(zero)";
    if (flag == coarsenData){
      txt = "(coarsen)";
    }
    printTask(coarsePatch, cout_doing, "Doing AMRMPM::coarsenNodalData_CFI"+txt);
    
    int numMatls = d_sharedState->getNumMPMMatls();                  
    for(int m = 0; m < numMatls; m++){                               
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );    
      int dwi = mpm_matl->getDWIndex();
      
      // get coarse level data
      NCVariable<double> gMass_coarse;
      NCVariable<double> gVolume_coarse;
      NCVariable<Vector> gVelocity_coarse;
      NCVariable<Vector> gVelocityStar_coarse;
      NCVariable<Vector> gAcceleration_coarse;
      NCVariable<Vector> gExternalforce_coarse;
      NCVariable<double> gTemperature_coarse;
      NCVariable<double> gConcentration_coarse;
      NCVariable<double> gExtScalarFlux_coarse;

      new_dw->getModifiable(gMass_coarse,           lb->gMassLabel,          dwi, coarsePatch);
      new_dw->getModifiable(gVolume_coarse,         lb->gVolumeLabel,        dwi, coarsePatch);
      new_dw->getModifiable(gVelocity_coarse,       lb->gVelocityLabel,      dwi, coarsePatch);
      new_dw->getModifiable(gTemperature_coarse,    lb->gTemperatureLabel,   dwi, coarsePatch);
      new_dw->getModifiable(gExternalforce_coarse,  lb->gExternalForceLabel, dwi, coarsePatch);
      if(flag == zeroData){
        new_dw->getModifiable(gVelocityStar_coarse, lb->gVelocityStarLabel,  dwi, coarsePatch);
        new_dw->getModifiable(gAcceleration_coarse, lb->gAccelerationLabel,  dwi, coarsePatch);
      }

      //__________________________________
      // Iterate over coarse/fine interface faces
      ASSERT(coarseLevel->hasFinerLevel());
      const Level* fineLevel = coarseLevel->getFinerLevel().get_rep();

      Level::selectType finePatches;
      coarsePatch->getFineLevelPatches(finePatches);

      // loop over all the fine level patches
      for (auto finePatch : CFI_finePatches) {

        // get fine level data
        constNCVariable<double> gMass_fine;
        constNCVariable<double> gVolume_fine;
        constNCVariable<Vector> gVelocity_fine;
        constNCVariable<double> gTemperature_fine;
        constNCVariable<Vector> gExternalforce_fine;
        constNCVariable<double> gConcentration_fine;
        constNCVariable<double> gExtScalarFlux_fine;

        if(flag == coarsenData){
          // use getRegion() instead of get().  They should be equivalent but 
          // get() throws assert on parallel runs.
          IntVector fl = finePatch->getNodeLowIndex();  
          IntVector fh = finePatch->getNodeHighIndex();
          new_dw->getRegion(gMass_fine,          lb->gMassLabel,          dwi, fineLevel,fl, fh);
          new_dw->getRegion(gVolume_fine,        lb->gVolumeLabel,        dwi, fineLevel,fl, fh);
          new_dw->getRegion(gVelocity_fine,      lb->gVelocityLabel,      dwi, fineLevel,fl, fh);
          new_dw->getRegion(gTemperature_fine,   lb->gTemperatureLabel,   dwi, fineLevel,fl, fh);
          new_dw->getRegion(gExternalforce_fine, lb->gExternalForceLabel, dwi, fineLevel,fl, fh);
        }

        vector<Patch::FaceType> cf;
        finePatch->getCoarseFaces(cf);

        // Iterate over coarse/fine interface faces
        for (auto patchFace : cf) {

          // determine the iterator on the coarse level.
          NodeIterator n_iter(IntVector(-8,-8,-8),IntVector(-9,-9,-9));
          bool isRight_CP_FP_pair;

          coarseLevel_CFI_NodeIterator(patchFace, coarsePatch,
                                       finePatch, fineLevel,
                                       n_iter, isRight_CP_FP_pair);

          // Is this the right coarse/fine patch pair
          if (isRight_CP_FP_pair){
            switch(flag){
            case coarsenData:
              for(; !n_iter.done(); n_iter++) {
                IntVector c_node = *n_iter;
                IntVector f_node = coarseLevel->mapNodeToFiner(c_node);

                // only overwrite coarse data if there is non-zero fine data
                if( gMass_fine[f_node] > 2 * d_SMALL_NUM_MPM ){
                  gMass_coarse[c_node]          = gMass_fine[f_node];
                  gVolume_coarse[c_node]        = gVolume_fine[f_node];
                  gVelocity_coarse[c_node]      = gVelocity_fine[f_node];
                  gTemperature_coarse[c_node]   = gTemperature_fine[f_node];
                  gExternalforce_coarse[c_node] = gExternalforce_fine[f_node];
                } // if mass
              } // end node iterator loop
              break;
            case zeroData:
              for(; !n_iter.done(); n_iter++) {
                IntVector c_node = *n_iter;
                IntVector f_node = coarseLevel->mapNodeToFiner(c_node);
                gMass_coarse[c_node]          = 0;
                gVolume_coarse[c_node]        = 0;
                gVelocity_coarse[c_node]      = Vector(0,0,0);
                gVelocityStar_coarse[c_node]  = Vector(0,0,0);
                gAcceleration_coarse[c_node]  = Vector(0,0,0);
                gTemperature_coarse[c_node]   = 0;
                gExternalforce_coarse[c_node] = Vector(0,0,0);
              } // end node iterator loop
              break;
            }
          }  //  isRight_CP_FP_pair
        }  //  end CFI face loop
      }  //  end fine Patch loop
    }  //  end matl loop
  }  // end coarse patch loop
}


//______________________________________________________________________
//  copy the fine level nodal data to the underlying coarse nodes at the CFI.
void AMRMPM::coarsenNodalData_CFI2(const ProcessorGroup*,
                                   const PatchSubset* coarsePatches,
                                   const MaterialSubset* ,
                                   DataWarehouse* old_dw,
                                   DataWarehouse* new_dw)
{
  Level::selectType CFI_coarsePatches;
  Level::selectType CFI_finePatches;
  coarseLevelCFI_Patches(coarsePatches, CFI_coarsePatches, CFI_finePatches );

  //__________________________________
  // From the coarse patch look up to the fine patches that have
  // coarse fine interfaces.
  const Level* coarseLevel = getLevel(coarsePatches);
  
  for (auto coarsePatch : CFI_coarsePatches) {

    printTask(coarsePatch,cout_doing,"Doing AMRMPM::coarsenNodalData_CFI2");
    
    int numMatls = d_sharedState->getNumMPMMatls();                  
    for(int m = 0; m < numMatls; m++){                               
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );    
      int dwi = mpm_matl->getDWIndex();
      
      // get coarse level data
      NCVariable<Vector> internalForce_coarse;                    
      new_dw->getModifiable(internalForce_coarse, lb->gInternalForceLabel, 
                            dwi,coarsePatch);
      NCVariable<double> gConcRate_coarse;                    

      //__________________________________
      // Iterate over coarse/fine interface faces
      ASSERT(coarseLevel->hasFinerLevel());
      const Level* fineLevel = coarseLevel->getFinerLevel().get_rep();

      // loop over all the fine level patches
      for (auto finePatch : CFI_finePatches) {

        // get fine level data
        constNCVariable<double> gMass_fine,gConcRate_fine;
        constNCVariable<Vector> internalForce_fine;

        // use getRegion() instead of get().  They should be equivalent but 
        // get() throws assert on parallel runs.
        IntVector fl = finePatch->getNodeLowIndex();
        IntVector fh = finePatch->getNodeHighIndex();
        new_dw->getRegion(gMass_fine,          lb->gMassLabel,         
                          dwi, fineLevel, fl, fh);
        new_dw->getRegion(internalForce_fine,  lb->gInternalForceLabel,
                          dwi, fineLevel, fl, fh);

        vector<Patch::FaceType> cf;
        finePatch->getCoarseFaces(cf);

        // Iterate over coarse/fine interface faces
        for (auto patchFace : cf) {

          // determine the iterator on the coarse level.
          NodeIterator n_iter(IntVector(-8,-8,-8),IntVector(-9,-9,-9));
          bool isRight_CP_FP_pair;

          coarseLevel_CFI_NodeIterator(patchFace, coarsePatch,
                                       finePatch, fineLevel,
                                       n_iter, isRight_CP_FP_pair);

          // Is this the right coarse/fine patch pair
          if (isRight_CP_FP_pair){
               
            for(; !n_iter.done(); n_iter++) {
              IntVector c_node = *n_iter;

              IntVector f_node = coarseLevel->mapNodeToFiner(c_node);
 
              // only overwrite coarse data if there is non-zero fine data
              if( gMass_fine[f_node] > 2 * d_SMALL_NUM_MPM ){
               
                internalForce_coarse[c_node] = internalForce_fine[f_node];

                 
/*`==========TESTING==========*/
#if 0
                  if( internalForce_coarse[c_node].length()  >1e-8){
                    ostringstream warn;
                    warn << "Too Big: " << c_node << " f_node " << f_node 
                         << "    L-"<< fineLevel->getIndex()
                         <<" InternalForce_fine   " << internalForce_fine[f_node] 
                         <<" InternalForce_coarse " << internalForce_coarse[c_node] << endl;
                     
                    throw InternalError(warn.str(), __FILE__, __LINE__);
                  } 
#endif
/*===========TESTING==========`*/
                  
              }
            }  //  node loop
          }  //  isRight_CP_FP_pair
        }  //  end CFI face loop
      }  //  end fine Patch loop
    }  //  end matl loop
  }  //  end coarse patch loop
}

//______________________________________________________________________
// Divide gVelocity and gTemperature by gMass
void AMRMPM::normalizeNodalVelTempConc(const ProcessorGroup*,
                                       const PatchSubset* patches,
                                       const MaterialSubset* ,
                                       DataWarehouse* ,
                                       DataWarehouse* new_dw)
{
  for(int p=0; p<patches->size(); p++){
    const Patch* patch = patches->get(p);
    printTask(patches,patch,cout_doing,"Doing AMRMPM::normalizeNodalVelTempConc");

    int numMatls = d_sharedState->getNumMPMMatls();

    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      // get  level nodal data
      constNCVariable<double> gMass;
      NCVariable<Vector> gVelocity;
      NCVariable<double> gTemperature;
      NCVariable<double> gHydroStress;
      
      new_dw->get(gMass,                  lb->gMassLabel,       dwi,patch,d_gn,0);
      new_dw->getModifiable(gVelocity,    lb->gVelocityLabel,   dwi,patch,d_gn,0);
      new_dw->getModifiable(gTemperature, lb->gTemperatureLabel,dwi,patch,d_gn,0);
      
      //__________________________________
      //  back out the nodal quantities
      for(NodeIterator iter=patch->getExtraNodeIterator();!iter.done();iter++){
        IntVector n = *iter;
        gVelocity[n]     /= gMass[n];
        gTemperature[n]  /= gMass[n];
      }
      
      // Apply boundary conditions to the temperature and velocity (if symmetry)
      MPMBoundCond bc;
      string interp_type = flags->d_interpolator_type;
      bc.setBoundaryCondition(patch,dwi,"Temperature",gTemperature,interp_type);
      bc.setBoundaryCondition(patch,dwi,"Symmetric",  gVelocity,   interp_type);
    }  // End loop over materials
  }  // End loop over fine patches
}

//______________________________________________________________________
//
void AMRMPM::computeStressTensor(const ProcessorGroup*,
                                 const PatchSubset* patches,
                                 const MaterialSubset* ,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw)
{
  printTask(patches, patches->get(0),cout_doing,
            "Doing AMRMPM::computeStressTensor");

  for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){

    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);

    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();

    cm->setWorld(UintahParallelComponent::d_myworld);
    cm->computeStressTensor(patches, mpm_matl, old_dw, new_dw);
  }
}

//______________________________________________________________________
//
void AMRMPM::updateErosionParameter(const ProcessorGroup*,
                                    const PatchSubset* patches,
                                    const MaterialSubset* ,
                                    DataWarehouse* old_dw,
                                    DataWarehouse* new_dw)
{
  for (int p = 0; p<patches->size(); p++) {
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,"Doing AMRMPM::updateErosionParameter");

    int numMPMMatls=d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMPMMatls; m++){

      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

      // Get the localization info
      ParticleVariable<int> isLocalized;
      new_dw->allocateTemporary(isLocalized, pset);
      ParticleSubset::iterator iter = pset->begin(); 
      for (; iter != pset->end(); iter++) isLocalized[*iter] = 0;
      mpm_matl->getConstitutiveModel()->getDamageParameter(patch, isLocalized,
                                                           dwi, old_dw,new_dw);
    }
  }
}
//______________________________________________________________________
//
void AMRMPM::computeInternalForce(const ProcessorGroup*,
                                  const PatchSubset* patches,
                                  const MaterialSubset* ,
                                  DataWarehouse* old_dw,
                                  DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,"Doing AMRMPM::computeInternalForce");

    Vector dx = patch->dCell();
    double oodx[3];
    oodx[0] = 1.0/dx.x();
    oodx[1] = 1.0/dx.y();
    oodx[2] = 1.0/dx.z();
    Matrix3 Id;
    Id.Identity();

    ParticleInterpolator* interpolator = flags->d_interpolator->clone(patch);

    
    string interp_type = flags->d_interpolator_type;

    int numMPMMatls = d_sharedState->getNumMPMMatls();

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      
      constParticleVariable<Point>   px;
      constParticleVariable<double>  pvol;
      constParticleVariable<double>  p_pressure;
      constParticleVariable<double>  p_q;
      constParticleVariable<Matrix3> pstress;
      constParticleVariable<Matrix3> psize;
      NCVariable<Vector>             internalforce;
      NCVariable<Matrix3>            gstress;
      constNCVariable<double>        gvolume;
      constParticleVariable<Matrix3> pDefGrad;

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch,
                                                       Ghost::AroundNodes, NGP,
                                                       lb->pXLabel);

      old_dw->get(px,      lb->pXLabel,                              pset);
      old_dw->get(pvol,    lb->pVolumeLabel,                         pset);
      old_dw->get(pstress, lb->pStressLabel,                         pset);
      new_dw->get(psize,   lb->pSizeLabel_preReloc,                  pset);
      old_dw->get(pDefGrad, lb->pDefGradLabel, pset);

      new_dw->get(gvolume, lb->gVolumeLabel, dwi, patch, Ghost::None, 0);
      new_dw->allocateAndPut(gstress,      lb->gStressForSavingLabel,dwi,patch);
      new_dw->allocateAndPut(internalforce,lb->gInternalForceLabel,  dwi,patch);
      gstress.initialize(Matrix3(0));
      internalforce.initialize(Vector(0,0,0));
      
      // load p_q
      if(flags->d_artificial_viscosity){
        old_dw->get(p_q,lb->p_qLabel, pset);
      }
      else {
        ParticleVariable<double>  p_q_create;
        new_dw->allocateTemporary(p_q_create,  pset);
        for(ParticleSubset::iterator it = pset->begin();it != pset->end();it++){
          p_q_create[*it]=0.0;
        }
        p_q = p_q_create; // reference created data
      }
      
/*`==========TESTING==========*/
      NCVariable<double> gSumS;
      new_dw->allocateAndPut(gSumS, gSumSLabel,  dwi, patch); 
      gSumS.initialize(0); 
/*===========TESTING==========`*/ 
      
      //__________________________________
      //  fine Patch     
      gstress.initialize(Matrix3(0));

      Matrix3 stressvol;
      Matrix3 stresspress;
      vector<IntVector> ni(interpolator->size());
      vector<double> S(interpolator->size());
      vector<Vector> d_S(interpolator->size());
    

      for (ParticleSubset::iterator iter  = pset->begin();
           iter != pset->end();  iter++){
        particleIndex idx = *iter;
  
        // Get the node indices that surround the cell
        int NN = interpolator->findCellAndWeightsAndShapeDerivatives(px[idx],ni,
                                   S, d_S, psize[idx],pDefGrad[idx]);

        stresspress = pstress[idx] + Id*(/*p_pressure*/-p_q[idx]);

        for (int k = 0; k < NN; k++){
          
          if(patch->containsNode(ni[k])){ 
            Vector div(d_S[k].x()*oodx[0],
                       d_S[k].y()*oodx[1],
                       d_S[k].z()*oodx[2]);
                       
            internalforce[ni[k]] -= (div * stresspress)  * pvol[idx];
            
            // cout << " CIF: ni: " << ni[k] << " div " << div << "\t internalForce " << internalforce[ni[k]] << endl;
            // cout << " div " << div[k] << " stressPress: " << stresspress  << endl;
            
            if( std::isinf( internalforce[ni[k]].length() ) || 
                std::isnan( internalforce[ni[k]].length() ) ){
              cout << "INF: " << ni[k] << " " << internalforce[ni[k]] 
                   << " div: " << div << " stressPress: " << stresspress 
                   << " pvol " << pvol[idx] << endl;
            }
/*`==========TESTING==========*/
            gSumS[ni[k]] +=S[k]; 
/*===========TESTING==========`*/
          }
        }
      }

      string interp_type = flags->d_interpolator_type;
      MPMBoundCond bc;
      bc.setBoundaryCondition(patch,dwi,"Symmetric",internalforce,interp_type);
    }  // End matl loop
    delete interpolator;
  }  // End patch loop
}

//______________________________________________________________________
//
void AMRMPM::computeInternalForce_CFI(const ProcessorGroup*,
                                      const PatchSubset* finePatches,
                                      const MaterialSubset* ,
                                      DataWarehouse* old_dw,
                                      DataWarehouse* new_dw)
{
  const Level* fineLevel = getLevel(finePatches);
  const Level* coarseLevel = fineLevel->getCoarserLevel().get_rep();
  IntVector refineRatio(fineLevel->getRefinementRatio());
  
  for(int p=0;p<finePatches->size();p++){
    const Patch* finePatch = finePatches->get(p);
    printTask(finePatches, finePatch,cout_doing,
              "Doing AMRMPM::computeInternalForce_CFI");

    ParticleInterpolator* interpolator =flags->d_interpolator->clone(finePatch);

    //__________________________________
    //          AT CFI
    if( fineLevel->hasCoarserLevel() &&  finePatch->hasCoarseFaces() ){

      // Determine extents for coarser level particle data
      // Linear Interpolation:  1 layer of coarse level cells
      // Gimp Interpolation:    2 layers
      /*`==========TESTING==========*/
      IntVector nLayers(d_nPaddingCells_Coarse, d_nPaddingCells_Coarse, d_nPaddingCells_Coarse );
      IntVector nPaddingCells = nLayers * (fineLevel->getRefinementRatio());
      //cout << " nPaddingCells " << nPaddingCells << "nLayers " << nLayers << endl;
      /*===========TESTING==========`*/

      int nGhostCells = 0;
      bool returnExclusiveRange=false;
      IntVector cl_tmp, ch_tmp, fl, fh;

      getCoarseLevelRange(finePatch, coarseLevel, cl_tmp, ch_tmp, fl, fh, 
                          nPaddingCells, nGhostCells,returnExclusiveRange);
                          
      //  expand cl_tmp when a neighor patch exists.
      //  This patch owns the low nodes.  You need particles
      //  from the neighbor patch.
      cl_tmp -= finePatch->neighborsLow() * nLayers;

      // find the coarse patches under the fine patch.  
      // You must add a single layer of padding cells.
      int padding = 1;
      Level::selectType coarsePatches;
      finePatch->getOtherLevelPatches55902(-1, coarsePatches, padding);
        
      Matrix3 Id;
      Id.Identity();
        
      constNCVariable<Stencil7> zoi_fine;
      new_dw->get(zoi_fine, lb->gZOILabel, 0, finePatch, Ghost::None, 0 );
  
      int numMPMMatls = d_sharedState->getNumMPMMatls();
      
      for(int m = 0; m < numMPMMatls; m++){
        MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
        int dwi = mpm_matl->getDWIndex();
        
        NCVariable<Vector> internalforce;
        new_dw->getModifiable(internalforce,lb->gInternalForceLabel,  
                              dwi, finePatch);

        /*`==========TESTING==========*/
        NCVariable<double> gSumS;
        new_dw->getModifiable(gSumS, gSumSLabel,  dwi, finePatch);
        /*===========TESTING==========`*/ 
        
        // loop over the coarse patches under the fine patches.
        for(int cp=0; cp<coarsePatches.size(); cp++){
          const Patch* coarsePatch = coarsePatches[cp];

          // get coarse level particle data                                                       
          ParticleSubset* pset_coarse;    
          constParticleVariable<Point> px_coarse;
          constParticleVariable<Matrix3> pstress_coarse;
          constParticleVariable<double>  pvol_coarse;
          constParticleVariable<double>  p_q_coarse;
          
          // coarseLow and coarseHigh cannot lie outside of the coarse patch
          IntVector cl = Max(cl_tmp, coarsePatch->getCellLowIndex());
          IntVector ch = Min(ch_tmp, coarsePatch->getCellHighIndex());

          pset_coarse = old_dw->getParticleSubset(dwi, cl, ch, coarsePatch,
                                                  lb->pXLabel);

#if 0
          cout << " fine patch : " << finePatch->getGridIndex() << endl;
          cout << " cl_tmp: "<< cl_tmp << " ch_tmp: " << ch_tmp << endl;
          cout << " cl:     " << cl    << " ch:     " << ch<< " fl: " << fl << " fh " << fh << endl;                                                     
          cout << "  " << *pset_coarse << endl;
#endif

          // coarse level data
          old_dw->get(px_coarse,       lb->pXLabel,       pset_coarse);
          old_dw->get(pvol_coarse,     lb->pVolumeLabel,  pset_coarse);
          old_dw->get(pstress_coarse,  lb->pStressLabel,  pset_coarse);
          
          // Artificial Viscosity
          if(flags->d_artificial_viscosity){
            old_dw->get(p_q_coarse,    lb->p_qLabel,      pset_coarse);
          }
          else {
            ParticleVariable<double>  p_q_create;
            new_dw->allocateTemporary(p_q_create,  pset_coarse);
            for(ParticleSubset::iterator it = pset_coarse->begin();
                it != pset_coarse->end();it++){
              p_q_create[*it]=0.0;
            }
            p_q_coarse = p_q_create; // reference created data
          }
          
          //__________________________________
          //  Iterate over the coarse level particles and 
          // add their contribution to the internal stress on the fine patch
          for (ParticleSubset::iterator iter = pset_coarse->begin(); 
               iter != pset_coarse->end();  iter++){
            particleIndex idx = *iter;

            vector<IntVector> ni;
            vector<double> S;
            vector<Vector> div;
            interpolator->findCellAndWeightsAndShapeDerivatives_CFI(
              px_coarse[idx], ni, S, div, zoi_fine );

            Matrix3 stresspress =  pstress_coarse[idx] + Id*(-p_q_coarse[idx]);

            IntVector fineNode;
            for(int k = 0; k < (int)ni.size(); k++) {   
              fineNode = ni[k];

              if( finePatch->containsNode( fineNode ) ){
                gSumS[fineNode] +=S[k];

                Vector Increment ( (div[k] * stresspress)  * pvol_coarse[idx] );
                //Vector Before = internalforce[fineNode];
                //Vector After  = Before - Increment;

                internalforce[fineNode] -=  Increment;


                //  cout << " CIF_CFI: ni: " << ni[k] << " div " << div[k] << "\t internalForce " << internalforce[fineNode] << endl;
                //  cout << "    before " << Before << " After " << After << " Increment " << Increment << endl;
                //  cout << "    div " << div[k] << " stressPress: " << stresspress << " pvol_coarse " << pvol_coarse[idx] << endl;


                /*`==========TESTING==========*/
                if(std::isinf( internalforce[fineNode].length() ) ||  std::isnan( internalforce[fineNode].length() )){
                  cout << "INF: " << fineNode << " " << internalforce[fineNode] 
                       << " div[k]:"<< div[k] << " stressPress: " << stresspress
                       << " pvol " << pvol_coarse[idx] << endl;
                }
#if 0             
                if( internalforce[fineNode].length()  >1e-10){
                  cout << "CIF_CFI: " << fineNode
                       << "    L-"<< getLevel(finePatches)->getIndex()
                       <<" InternalForce " << internalforce[fineNode] << " div[k]: " << div[k] << " stressPress: " << stresspress 
                       << " pvol " << pvol_coarse[idx] << endl;
                  cout << "          Before: " << Before << " Increment " << Increment << endl;
                }
#endif
                /*===========TESTING==========`*/
              }  // contains node
            }  // node loop          
          }  // pset loop
        }  // coarse Patch loop
        
        //__________________________________
        //  Set boundary conditions 
        string interp_type = flags->d_interpolator_type;
        MPMBoundCond bc;
        bc.setBoundaryCondition( finePatch,dwi,"Symmetric",internalforce,
                                 interp_type); 
              
      }  // End matl loop 
    }  // patch has CFI faces
    delete interpolator;
  }  // End fine patch loop
}

//______________________________________________________________________
//
void AMRMPM::computeAndIntegrateAcceleration(const ProcessorGroup*,
                                             const PatchSubset* patches,
                                             const MaterialSubset*,
                                             DataWarehouse* old_dw,
                                             DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,
              "Doing AMRMPM::computeAndIntegrateAcceleration");

    Vector gravity = flags->d_gravity;
    
    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      // Get required variables for this patch
      constNCVariable<Vector> internalforce;
      constNCVariable<Vector> externalforce;
      constNCVariable<Vector> gvelocity;
      constNCVariable<double> gmass;

      delt_vartype delT;
      old_dw->get(delT, d_sharedState->get_delt_label(), getLevel(patches) );

      new_dw->get(internalforce, lb->gInternalForceLabel, dwi, patch, d_gn, 0);
      new_dw->get(externalforce, lb->gExternalForceLabel, dwi, patch, d_gn, 0);
      new_dw->get(gmass,         lb->gMassLabel,          dwi, patch, d_gn, 0);
      new_dw->get(gvelocity,     lb->gVelocityLabel,      dwi, patch, d_gn, 0);

      // Create variables for the results
      NCVariable<Vector> gvelocity_star;
      NCVariable<Vector> gacceleration;
      NCVariable<double> gConcStar,gConcRate;
      new_dw->allocateAndPut(gvelocity_star, lb->gVelocityStarLabel, dwi,patch);
      new_dw->allocateAndPut(gacceleration,  lb->gAccelerationLabel, dwi,patch);

      gacceleration.initialize(Vector(0.,0.,0.));
      double damp_coef = flags->d_artificialDampCoeff;
      gvelocity_star.initialize(Vector(0.,0.,0.));

      for(NodeIterator iter=patch->getExtraNodeIterator(); !iter.done();iter++){
        IntVector n = *iter;
        
        Vector acc(0,0,0);
        if (gmass[n] > flags->d_min_mass_for_acceleration){
          acc = (internalforce[n] + externalforce[n])/gmass[n];
          acc -= damp_coef * gvelocity[n];
        }
        gacceleration[n]  = acc +  gravity;
        gvelocity_star[n] = gvelocity[n] + gacceleration[n] * delT;
          
/*`==========TESTING==========*/
#ifdef DEBUG_ACC
        if( abs(gacceleration[n].length() - d_acc_ans.length()) > d_acc_tol ) {
          Vector diff = gacceleration[n] - d_acc_ans;
          cout << "    L-"<< getLevel(patches)->getIndex() << " node: "<< n << " gacceleration: " << gacceleration[n] 
               <<  " externalForce: " << externalforce[n]
               << " internalforce: "  << internalforce[n] 
               << " diff: " << diff
               << " gmass: " << gmass[n] 
               << " gravity: " << gravity << endl;
        }
#endif 
/*===========TESTING==========`*/
      }
    }  // matls
  }  // patches
}
//______________________________________________________________________
//
void AMRMPM::setGridBoundaryConditions(const ProcessorGroup*,
                                       const PatchSubset* patches,
                                       const MaterialSubset* ,
                                       DataWarehouse* old_dw,
                                       DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,"Doing AMRMPM::setGridBoundaryConditions");

    int numMPMMatls=d_sharedState->getNumMPMMatls();
    
    delt_vartype delT;            
    old_dw->get(delT, d_sharedState->get_delt_label(), getLevel(patches) );
    
    string interp_type = flags->d_interpolator_type;

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      
      NCVariable<Vector> gvelocity_star;
      NCVariable<Vector> gacceleration;
      constNCVariable<Vector> gvelocity;

      new_dw->getModifiable(gacceleration, lb->gAccelerationLabel,  dwi,patch);
      new_dw->getModifiable(gvelocity_star,lb->gVelocityStarLabel,  dwi,patch);
      new_dw->get(gvelocity,               lb->gVelocityLabel,      dwi, patch,
                  Ghost::None,0);
          
          
      //__________________________________
      // Apply grid boundary conditions to velocity_star and acceleration
      if( patch->hasBoundaryFaces() ){
        IntVector node(0,4,4);

        MPMBoundCond bc;
        bc.setBoundaryCondition(patch,dwi,"Velocity", gvelocity_star,interp_type);
        bc.setBoundaryCondition(patch,dwi,"Symmetric",gvelocity_star,interp_type);

        // Now recompute acceleration as the difference between the velocity
        // interpolated to the grid (no bcs applied) and the new velocity_star
        for(NodeIterator iter = patch->getExtraNodeIterator();
            !iter.done(); iter++){
          IntVector c = *iter;
          gacceleration[c] = (gvelocity_star[c] - gvelocity[c])/delT;
        }
      } 
      
      //__________________________________
      //
      if(!flags->d_doGridReset){
        NCVariable<Vector> displacement;
        constNCVariable<Vector> displacementOld;
        new_dw->allocateAndPut(displacement,lb->gDisplacementLabel,dwi,patch);
        old_dw->get(displacementOld,        lb->gDisplacementLabel,dwi,patch,
                    Ghost::None,0);
        for(NodeIterator iter = patch->getExtraNodeIterator();
            !iter.done();iter++){
          IntVector c = *iter;
          displacement[c] = displacementOld[c] + gvelocity_star[c] * delT;
        }
      }  // d_doGridReset

    } // matl loop
  }  // patch loop
}
//______________________________________________________________________

void AMRMPM::computeZoneOfInfluence(const ProcessorGroup*,
                                    const PatchSubset* patches,
                                    const MaterialSubset*,
                                    DataWarehouse* old_dw,
                                    DataWarehouse* new_dw)
{
  const Level* level = getLevel(patches);
  
  ASSERT(level->hasCoarserLevel() );

  //__________________________________
  //  Initialize the interior nodes
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    Vector dx = patch->dCell();
 
    printTask(patches, patch,cout_doing,"Doing AMRMPM::computeZoneOfInfluence");
    NCVariable<Stencil7> zoi;
    new_dw->allocateAndPut(zoi, lb->gZOILabel, 0, patch);

    for(NodeIterator iter = patch->getNodeIterator();!iter.done();iter++){
      IntVector c = *iter;
      zoi[c].p=-9876543210e99;
      zoi[c].w= dx.x();
      zoi[c].e= dx.x();
      zoi[c].s= dx.y();
      zoi[c].n= dx.y();
      zoi[c].b= dx.z();
      zoi[c].t= dx.z();
    }
  }
  
  // find the fine & coarse CFI patches
  Level::selectType CFI_coarsePatches;
  Level::selectType CFI_finePatches;
  
  coarseLevelCFI_Patches(patches,CFI_coarsePatches, CFI_finePatches  );
  
  //__________________________________
  // Set the ZOI on the current level.
  // Look up at the finer level patches
  // for coarse-fine interfaces
  for(int p=0;p<CFI_coarsePatches.size();p++){
    const Patch* coarsePatch = CFI_coarsePatches[p];
    
    NCVariable<Stencil7> zoi;
    new_dw->getModifiable(zoi, lb->gZOILabel, 0,coarsePatch);
  
      const Level* fineLevel = level->getFinerLevel().get_rep();
  
      
    for(int p=0;p<CFI_finePatches.size();p++){  
      const Patch* finePatch = CFI_finePatches[p];
    
        Vector fine_dx = finePatch->dCell();
 
        //__________________________________
        // Iterate over coarsefine interface faces
          vector<Patch::FaceType> cf;
          finePatch->getCoarseFaces(cf);
          
          vector<Patch::FaceType>::const_iterator iter;  
          for (iter  = cf.begin(); iter != cf.end(); ++iter){
            Patch::FaceType patchFace = *iter;

            // determine the iterator on the coarse level.
            NodeIterator n_iter(IntVector(-8,-8,-8),IntVector(-9,-9,-9));
            bool isRight_CP_FP_pair;
            
        coarseLevel_CFI_NodeIterator( patchFace,coarsePatch, finePatch, fineLevel,
                                          n_iter, isRight_CP_FP_pair);

            // The ZOI element is opposite
            // of the patch face
            int element = patchFace;
            if(patchFace == Patch::xminus || 
               patchFace == Patch::yminus || 
               patchFace == Patch::zminus){
              element += 1;  // e, n, t 
            }
            if(patchFace == Patch::xplus || 
               patchFace == Patch::yplus || 
               patchFace == Patch::zplus){
              element -= 1;   // w, s, b
            }
        IntVector dir = coarsePatch->getFaceAxes(patchFace); // face axes
        int p_dir = dir[0];                                  // normal direction
            
            // eject if this is not the right coarse/fine patch pair
            if (isRight_CP_FP_pair){
              
//         cout << "  A) Setting ZOI  " 
//              << " \t On L-" << level->getIndex() << " patch  " << coarsePatch->getID()
//              << ", beneath patch " << finePatch->getID() << ", face: "  << finePatch->getFaceName(patchFace) 
//              << ", isRight_CP_FP_pair: " << isRight_CP_FP_pair  << " n_iter: " << n_iter << endl;
              
              for(; !n_iter.done(); n_iter++) {
                IntVector c = *n_iter;
                zoi[c][element]=fine_dx[p_dir];
              }
            }
                         
          }  // patch face loop
      }  // finePatches loop
  }  // coarse patches loop


  //__________________________________
  // set the ZOI in cells in which there are overlaping coarse level nodes
  // look down for coarse level patches 

  Level::selectType coarsePatches;
  Level::selectType CFI_finePatches2;
  fineLevelCFI_Patches(patches,coarsePatches, CFI_finePatches2  );
  
  for(int p=0;p<CFI_finePatches2.size();p++){
    const Patch* finePatch = CFI_finePatches2[p];

    NCVariable<Stencil7> zoi_fine;
    new_dw->getModifiable(zoi_fine, lb->gZOILabel, 0,finePatch);

      //__________________________________
    // Iterate over coarse/fine interface faces
        vector<Patch::FaceType> cf;
        finePatch->getCoarseFaces(cf);

        vector<Patch::FaceType>::const_iterator iter;  
        for (iter  = cf.begin(); iter != cf.end(); ++iter){
          Patch::FaceType patchFace = *iter;
          bool setFace = false;
            
          for(int p=0;p<coarsePatches.size();p++){
            const Patch* coarsePatch = coarsePatches[p];
            Vector coarse_dx = coarsePatch->dCell();

            // determine the iterator on the coarse level.
            NodeIterator n_iter(IntVector(-8,-8,-8),IntVector(-9,-9,-9));
            bool isRight_CP_FP_pair;
            
            fineLevel_CFI_NodeIterator( patchFace,coarsePatch, finePatch,
                                        n_iter ,isRight_CP_FP_pair);

            // Is this the right coarse/fine patch pair
            if (isRight_CP_FP_pair){
          int element   = patchFace;
          IntVector dir = finePatch->getFaceAxes(patchFace); // face axes
          int p_dir     = dir[0];                           // normal dir
              setFace = true; 
                   
//          cout << "  C) Setting ZOI  "                                                                 
//               << " \t On L-" << level->getIndex() << " patch  " << finePatch->getID()                 
//               << "  coarsePatch " << coarsePatch->getID()                                             
//               << "   CFI face: "  << finePatch->getFaceName(patchFace)                                
//               << " isRight_CP_FP_pair: " << isRight_CP_FP_pair  << " n_iter: " << n_iter << endl;                 

              for(; !n_iter.done(); n_iter++) {
                IntVector c = *n_iter;
                zoi_fine[c][element]=coarse_dx[p_dir];
              }
            }
          }  // coarsePatches loop
          
      //__________________________________
          // bulletproofing
          if( !setFace ){ 
            ostringstream warn;
            warn << "\n ERROR: computeZoneOfInfluence:Fine Level: Did not find node iterator! "
                 << "\n coarse: L-" << level->getIndex()
                 << "\n coarePatches size: " << coarsePatches.size()
                 << "\n fine patch:   " << *finePatch
                 << "\n fine patch face: " << finePatch->getFaceName(patchFace);
            throw ProblemSetupException(warn.str(), __FILE__, __LINE__);
          }
        }  // face interator
  }  // patch loop
}

//______________________________________________________________________
//
void AMRMPM::interpolateToParticlesAndUpdate(const ProcessorGroup*,
                                             const PatchSubset* patches,
                                             const MaterialSubset* ,
                                             DataWarehouse* old_dw,
                                             DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,
              "Doing AMRMPM::interpolateToParticlesAndUpdate");

    ParticleInterpolator* interpolator = flags->d_interpolator->clone(patch);
    vector<IntVector> ni(interpolator->size());
    vector<double> S(interpolator->size());
    vector<Vector> d_S(interpolator->size());
    //Vector dx = patch->dCell();

    // Performs the interpolation from the cell vertices of the grid
    // acceleration and velocity to the particles to update their
    // velocity and position respectively
 
    // DON'T MOVE THESE!!!
    double thermal_energy = 0.0;
    double totalmass = 0;
    Vector CMX(0.0,0.0,0.0);
    Vector totalMom(0.0,0.0,0.0);
    double ke=0;

    int numMPMMatls=d_sharedState->getNumMPMMatls();
    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label(), getLevel(patches) );

    //Carry forward NC_CCweight (put outside of matl loop, only need for matl 0)
    constNCVariable<double> NC_CCweight;
    NCVariable<double> NC_CCweight_new;
    Ghost::GhostType  gnone = Ghost::None;
    old_dw->get(NC_CCweight,       lb->NC_CCweightLabel,  0, patch, gnone, 0);
    new_dw->allocateAndPut(NC_CCweight_new, lb->NC_CCweightLabel,0,patch);
    NC_CCweight_new.copyData(NC_CCweight);

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      // Get the arrays of particle values to be changed
      constParticleVariable<Point> px;
      ParticleVariable<Point> pxnew,pxx;
      constParticleVariable<Vector> pvelocity;
      constParticleVariable<Matrix3> psize;
      ParticleVariable<Vector> pvelocitynew;
      ParticleVariable<Matrix3> psizeNew;
      constParticleVariable<double> pmass,pTemperature;
      ParticleVariable<double> pmassNew,pvolume,pTempNew;
      constParticleVariable<long64> pids;
      ParticleVariable<long64> pids_new;
      constParticleVariable<Vector> pdisp;
      ParticleVariable<Vector> pdispnew;
      ParticleVariable<double> pTempPreNew;
      constParticleVariable<Matrix3> pFOld;

      // Get the arrays of grid data on which the new particle values depend
      constNCVariable<Vector> gvelocity_star, gacceleration;
      constNCVariable<double> gTemperatureRate;
      constNCVariable<double> dTdt, frictionTempRate;
      double Cp = mpm_matl->getSpecificHeat();

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

      old_dw->get(px,           lb->pXLabel,                         pset);
      old_dw->get(pdisp,        lb->pDispLabel,                      pset);
      old_dw->get(pmass,        lb->pMassLabel,                      pset);
      old_dw->get(pvelocity,    lb->pVelocityLabel,                  pset);
      old_dw->get(pTemperature, lb->pTemperatureLabel,               pset);
      old_dw->get(pFOld,        lb->pDefGradLabel,        pset);
      new_dw->get(psize,        lb->pSizeLabel_preReloc,             pset);

      new_dw->allocateAndPut(pvelocitynew, lb->pVelocityLabel_preReloc,   pset);
      new_dw->allocateAndPut(pxnew,        lb->pXLabel_preReloc,          pset);
      new_dw->allocateAndPut(pxx,          lb->pXXLabel,                  pset);
      new_dw->allocateAndPut(pdispnew,     lb->pDispLabel_preReloc,       pset);
      new_dw->allocateAndPut(pmassNew,     lb->pMassLabel_preReloc,       pset);
      new_dw->allocateAndPut(pTempNew,     lb->pTemperatureLabel_preReloc,pset);
      new_dw->allocateAndPut(pTempPreNew, lb->pTempPreviousLabel_preReloc,pset);


      ParticleSubset* delset = scinew ParticleSubset(0, dwi, patch);

      //Carry forward ParticleID and pSize
      old_dw->get(pids,                lb->pParticleIDLabel,          pset);
      new_dw->allocateAndPut(pids_new, lb->pParticleIDLabel_preReloc, pset);
      pids_new.copyData(pids);

      ParticleVariable<int> isLocalized;
      new_dw->allocateAndPut(isLocalized, lb->pLocalizedMPMLabel_preReloc,pset);
      ParticleSubset::iterator iter = pset->begin();
      for (; iter != pset->end(); iter++){
        isLocalized[*iter] = 0;
      }

      new_dw->get(gvelocity_star,  lb->gVelocityStarLabel,   dwi,patch,d_gac,NGP);
      new_dw->get(gacceleration,   lb->gAccelerationLabel,   dwi,patch,d_gac,NGP);
      new_dw->get(gTemperatureRate,lb->gTemperatureRateLabel,dwi,patch,d_gac,NGP);
      new_dw->get(frictionTempRate,lb->frictionalWorkLabel,  dwi,patch,d_gac,NGP);

      if(flags->d_with_ice){
        new_dw->get(dTdt,          lb->dTdt_NCLabel,         dwi,patch,d_gac,NGP);
      }
      else{
        NCVariable<double> dTdt_create,massBurnFrac_create;
        new_dw->allocateTemporary(dTdt_create,                   patch,d_gac,NGP);
        dTdt_create.initialize(0.);
        dTdt = dTdt_create;                         // reference created data
      }

      // Loop over particles
      for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++){
        particleIndex idx = *iter;

        // Get the node indices that surround the cell
        int NN = interpolator->findCellAndWeights(px[idx],ni,S,psize[idx],
                                                               pFOld[idx]);

        Vector vel(0.0,0.0,0.0);
        Vector acc(0.0,0.0,0.0);
        double fricTempRate = 0.0;
        double tempRate = 0.0;

        // Accumulate the contribution from vertices on this level
        for(int k = 0; k < NN; k++) {
          IntVector node = ni[k];
          vel      += gvelocity_star[node]  * S[k];
          acc      += gacceleration[node]   * S[k];

          fricTempRate = frictionTempRate[node]*flags->d_addFrictionWork;
          tempRate += (gTemperatureRate[node] + dTdt[node] +
                       fricTempRate)   * S[k];
        }

        // Update the particle's position and velocity
        pxnew[idx]           = px[idx]    + vel*delT;
        pdispnew[idx]        = pdisp[idx] + vel*delT;
        pvelocitynew[idx]    = pvelocity[idx]    + acc*delT;

        // pxx is only useful if we're not in normal grid resetting mode.
        pxx[idx]             = px[idx]    + pdispnew[idx];
        pTempNew[idx]        = pTemperature[idx] + tempRate*delT;
        pTempPreNew[idx]     = pTemperature[idx]; // for thermal stress
        pmassNew[idx]        = pmass[idx];

/*`==========TESTING==========*/
#ifdef DEBUG_VEL
        Vector diff = ( pvelocitynew[idx] - d_vel_ans );
        if( abs(diff.length() ) > d_vel_tol ) {
          cout << "    L-"<< getLevel(patches)->getIndex() << " px: "<< pxnew[idx] << " pvelocitynew: " << pvelocitynew[idx] <<  " pvelocity " << pvelocity[idx]
               << " diff " << diff << endl;
        }
#endif
#ifdef DEBUG_ACC
#endif
/*===========TESTING==========`*/

        totalmass  += pmass[idx];
        thermal_energy += pTemperature[idx] * pmass[idx] * Cp;
        ke += .5*pmass[idx]*pvelocitynew[idx].length2();
        CMX         = CMX + (pxnew[idx]*pmass[idx]).asVector();
        totalMom   += pvelocitynew[idx]*pmass[idx];
      }

#if 0 // Until Todd is ready for this, leave inactive
      // Delete particles that have left the domain
      // This is only needed if extra cells are being used.
      // Also delete particles whose mass is too small (due to combustion)
      // For particles whose new velocity exceeds a maximum set in the input
      // file, set their velocity back to the velocity that it came into
      // this step with
      for(ParticleSubset::iterator iter  = pset->begin();
          iter != pset->end(); iter++){
        particleIndex idx = *iter;
        if ((pmassNew[idx] <= flags->d_min_part_mass) || pTempNew[idx] < 0. ||
            (pLocalized[idx]==-999)){
          delset->addParticle(idx);
//        cout << "Material = " << m << " Deleted Particle = " << pids_new[idx] 
//             << " xold = " << px[idx] << " xnew = " << pxnew[idx]
//             << " vold = " << pvelocity[idx] << " vnew = "<< pvelocitynew[idx]
//             << " massold = " << pmass[idx] << " massnew = " << pmassNew[idx]
//             << " tempold = " << pTemperature[idx] 
//             << " tempnew = " << pTempNew[idx]
//             << " pLocalized = " << pLocalized[idx]
//             << " volnew = " << pvolume[idx] << endl;
        }
        if(pvelocitynew[idx].length() > flags->d_max_vel){
          if(pvelocitynew[idx].length() >= pvelocity[idx].length()){
            pvelocitynew[idx]=(pvelocitynew[idx]/pvelocitynew[idx].length())*(flags->d_max_vel*.9);  
            cout<<endl<<"Warning: particle "<<pids[idx]<<" hit speed ceiling #1. Modifying particle velocity accordingly."<<endl;
            //pvelocitynew[idx]=pvelocity[idx];
          }
        }
      }
#endif

      new_dw->deleteParticles(delset);    

      new_dw->put(sum_vartype(totalmass),       lb->TotalMassLabel);
      new_dw->put(sum_vartype(ke),              lb->KineticEnergyLabel);
      new_dw->put(sum_vartype(thermal_energy),  lb->ThermalEnergyLabel);
      new_dw->put(sumvec_vartype(CMX),          lb->CenterOfMassPositionLabel);
      new_dw->put(sumvec_vartype(totalMom),     lb->TotalMomentumLabel);


#ifndef USE_DEBUG_TASK
      //__________________________________
      //  particle debugging label-- carry forward
      if (flags->d_with_color) {
        constParticleVariable<double> pColor;
        ParticleVariable<double>pColor_new;
        old_dw->get(pColor, lb->pColorLabel, pset);
        new_dw->allocateAndPut(pColor_new, lb->pColorLabel_preReloc, pset);
        pColor_new.copyData(pColor);
      }    
#endif
      if(flags->d_refineParticles){
        constParticleVariable<int> pRefinedOld;
        ParticleVariable<int> pRefinedNew;
        old_dw->get(pRefinedOld,            lb->pRefinedLabel,          pset);
        new_dw->allocateAndPut(pRefinedNew, lb->pRefinedLabel_preReloc, pset);
        pRefinedNew.copyData(pRefinedOld);
      }
    }
    delete interpolator;
  }
}
//______________________________________________________________________
//
void AMRMPM::finalParticleUpdate(const ProcessorGroup*,
                                 const PatchSubset* patches,
                                 const MaterialSubset* ,
                                 DataWarehouse* old_dw,
                                 DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,
              "Doing finalParticleUpdate");

    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label(), getLevel(patches) );

    int numMPMMatls=d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      // Get the arrays of particle values to be changed
      constParticleVariable<double> pdTdt,pmassNew;
      ParticleVariable<double> pTempNew;

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
      ParticleSubset* delset = scinew ParticleSubset(0, dwi, patch);

      new_dw->get(pdTdt,        lb->pdTdtLabel,                      pset);
      new_dw->get(pmassNew,     lb->pMassLabel_preReloc,             pset);

      new_dw->getModifiable(pTempNew, lb->pTemperatureLabel_preReloc,pset);

      // Loop over particles
      for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++){
        particleIndex idx = *iter;
        pTempNew[idx] += pdTdt[idx]*delT;

        // Delete particles whose mass is too small (due to combustion),
        // whose pLocalized flag has been set to -999 or who have a negative temperature
        if ((pmassNew[idx] <= flags->d_min_part_mass) || pTempNew[idx] < 0.){
          delset->addParticle(idx);
        }

      } // particles
      new_dw->deleteParticles(delset);
    } // materials
  } // patches
}
//______________________________________________________________________
//
void AMRMPM::addParticles(const ProcessorGroup*,
                          const PatchSubset* patches,
                          const MaterialSubset* ,
                          DataWarehouse* old_dw,
                          DataWarehouse* new_dw)
{
    const Level* level = getLevel(patches);
  int numLevels = level->getGrid()->numLevels();
    int levelIndex = level->getIndex();
    bool hasCoarser=false;
    if(level->hasCoarserLevel()){
      hasCoarser=true;
    }

  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    Vector dx = patch->dCell();
    printTask(patches, patch,cout_doing, "Doing addParticles");
    int numMPMMatls=d_sharedState->getNumMPMMatls();

    //Carry forward CellNAPID
    constCCVariable<int> NAPID;
    CCVariable<int> NAPID_new;

    old_dw->get(NAPID,               lb->pCellNAPIDLabel,    0,patch,d_gn,0);
    new_dw->allocateAndPut(NAPID_new,lb->pCellNAPIDLabel,    0,patch);
    NAPID_new.copyData(NAPID);

    // Mark cells where particles are refined for grid refinement
    CCVariable<double> refineCell;
    new_dw->getModifiable(refineCell, lb->MPMRefineCellLabel, 0, patch);

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
      ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();

      ParticleVariable<Point> px;
      ParticleVariable<Matrix3> pF,pSize,pstress,pvelgrad,pscalefac;
      ParticleVariable<long64> pids;
      ParticleVariable<double> pvolume,pmass,ptemp,ptempP, pcolor;
      ParticleVariable<double> pESF,pD;
      ParticleVariable<Vector> pvelocity,pextforce,pdisp,pconcgrad,pArea;
      ParticleVariable<int> pref,ploc,plal,prefOld,pLoadCID,pSplitR1R2R3;
      new_dw->getModifiable(px,       lb->pXLabel_preReloc,            pset);
      new_dw->getModifiable(pids,     lb->pParticleIDLabel_preReloc,   pset);
      new_dw->getModifiable(pmass,    lb->pMassLabel_preReloc,         pset);
      new_dw->getModifiable(pSize,    lb->pSizeLabel_preReloc,         pset);
      new_dw->getModifiable(pdisp,    lb->pDispLabel_preReloc,         pset);
      new_dw->getModifiable(pstress,  lb->pStressLabel_preReloc,       pset);
      new_dw->getModifiable(pvolume,  lb->pVolumeLabel_preReloc,       pset);
      new_dw->getModifiable(pvelocity,lb->pVelocityLabel_preReloc,     pset);
      if(flags->d_computeScaleFactor){
        new_dw->getModifiable(pscalefac,lb->pScaleFactorLabel_preReloc,pset);
      }
      new_dw->getModifiable(pextforce,lb->pExtForceLabel_preReloc,     pset);
      new_dw->getModifiable(ptemp,    lb->pTemperatureLabel_preReloc,  pset);
      new_dw->getModifiable(ptempP,   lb->pTempPreviousLabel_preReloc, pset);
      new_dw->getModifiable(pref,     lb->pRefinedLabel_preReloc,      pset);
      new_dw->getModifiable(plal,     lb->pLastLevelLabel_preReloc,    pset);
      new_dw->getModifiable(ploc,     lb->pLocalizedMPMLabel_preReloc, pset);
      new_dw->getModifiable(pvelgrad, lb->pVelGradLabel_preReloc,      pset);
      new_dw->getModifiable(pF,  lb->pDefGradLabel_preReloc,pset);
      if (flags->d_with_color) {
        new_dw->getModifiable(pcolor,   lb->pColorLabel_preReloc,        pset);
      }
      if (flags->d_useLoadCurves) {
        new_dw->getModifiable(pLoadCID, lb->pLoadCurveIDLabel_preReloc,  pset);
      }

      // Body force acceleration
      ParticleVariable<double> pCoriolis;
      ParticleVariable<Vector> pBodyFAcc;
      new_dw->getModifiable(pCoriolis, lb->pCoriolisImportanceLabel_preReloc,  pset);
      new_dw->getModifiable(pBodyFAcc, lb->pBodyForceAccLabel_preReloc,        pset);

      
      new_dw->allocateTemporary(prefOld,       pset);
      new_dw->allocateTemporary(pSplitR1R2R3,  pset);

      int numNewPartNeeded=0;
      bool splitForStretch=false;
      bool splitForAny=false;
      // Put refinement criteria here
      const unsigned int origNParticles = pset->addParticles(0);
      for( unsigned int pp=0; pp<origNParticles; ++pp ){
        prefOld[pp] = pref[pp];
        // Conditions to refine particle based on physical state
        // TODO:  Check below, should be < or <= in first conditional
       bool splitCriteria=false;
       //__________________________________
       // Only set the refinement flags for certain materials
       for(int i = 0; i< (int)d_thresholdVars.size(); i++ ){
          thresholdVar data = d_thresholdVars[i];
          string name  = data.name;
          double thresholdValue = data.value;

          if(m==data.matl){
            pSplitR1R2R3[pp]=0;
            if(name=="stressNorm"){
               double stressNorm = pstress[pp].Norm();
               if(stressNorm > thresholdValue){
                 splitCriteria = true;
                 splitForAny = true;
               }
            }
            if(name=="stretchRatio"){
              // This is the same R-vector equation used in CPDI interpolator
              // The "size" is relative to the grid cell size at this point
//              Matrix3 dsize = pF[pp]*pSize[pp];
              Matrix3 dsize = pF[pp]*pSize[pp]*Matrix3(dx[0],0,0,
                                                       0,dx[1],0,
                                                       0,0,dx[2]);
              Vector R1(dsize(0,0), dsize(1,0), dsize(2,0));
              Vector R2(dsize(0,1), dsize(1,1), dsize(2,1));
              Vector R3(dsize(0,2), dsize(1,2), dsize(2,2));
              double R1L=R1.length2();
              double R2L=R2.length2();
              double R3L=R3.length2();
              double R1_R2_ratSq = R1L/R2L;
              double R1_R3_ratSq = R1L/R3L;
              double R2_R3_ratSq = R2L/R3L;
              double tVSq = thresholdValue*thresholdValue;
              double tV_invSq = 1.0/tVSq;
//              cout << "R1L = " << R1L << endl;
//              cout << "R2L = " << R2L << endl;
//              cout << "R3L = " << R3L << endl;
              if (R1_R2_ratSq > tVSq){
                pSplitR1R2R3[pp]=1;
              } else if (R1_R2_ratSq < tV_invSq) {
                pSplitR1R2R3[pp]=-1;
              } else if (R1_R3_ratSq > tVSq && d_ndim==3){
                pSplitR1R2R3[pp]=2;
              } else if (R1_R3_ratSq < tV_invSq && d_ndim==3){
                pSplitR1R2R3[pp]=-2;
              } else if (R2_R3_ratSq > tVSq && d_ndim==3){
                 pSplitR1R2R3[pp]=3;
              } else if (R2_R3_ratSq < tV_invSq && d_ndim==3){
                 pSplitR1R2R3[pp]=-3;
              } else {
                 pSplitR1R2R3[pp]=0;
              }
  
              if(pSplitR1R2R3[pp]){
                //cout << "pSplit = " << pSplitR1R2R3[pp] << endl;
                splitCriteria  = true;
                splitForStretch = true;
                splitForAny = true;
              }
           }
         } // if this matl is in the list
       } // loop over criteria

        if((pref[pp]< levelIndex && splitCriteria && numLevels > 1 ) ||
           (pref[pp]<=levelIndex && splitCriteria && numLevels == 1)){
          pref[pp]++;
          numNewPartNeeded++;
        }
        if(pref[pp]>prefOld[pp] || splitCriteria) {
          IntVector c = level->getCellIndex(px[pp]);
          if(patch->containsCell(c)){
            refineCell[c] = 3.0;  // Why did I use 3 here?  JG
          }
        }else{
          if(hasCoarser){  /* see comment below */
            IntVector c = level->getCellIndex(px[pp]);
            if(patch->containsCell(c)){
              refineCell[c] = -100.;
            }
          }
        }

        // Refine particle if it is too big relative to the cell size
        // of the level it is on.  Don't refine the grid.
        if(pref[pp]< levelIndex){
          splitForAny = true;
          pref[pp]++;
          numNewPartNeeded++;
        }
      }  // Loop over original particles

      int fourOrEight=pow(2,d_ndim);
      if(splitForStretch){
        fourOrEight=4;
      }
      double fourthOrEighth = 1./((double) fourOrEight);
      numNewPartNeeded*=fourOrEight;

      /*  This tomfoolery is in place to keep refined regions that contain
          particles refined.  If a patch with particles coarsens, the particles
          on that patch disappear when the fine patch is deleted.  This
          prevents the deletion of those patches.  Ideally, we'd allow
          coarsening and relocate the orphan particles, but I don't know how to
          do that.  JG */
      bool keep_patch_refined=false;
      IntVector low = patch->getCellLowIndex();
      IntVector high= patch->getCellHighIndex();
      IntVector middle = (low+high)/IntVector(2,2,2);

      for(CellIterator iter=patch->getCellIterator(); !iter.done();iter++){
        IntVector c = *iter;
        if(refineCell[c]<0.0){
          keep_patch_refined=true;
          refineCell[c]=0.0;
        }
      }
      if(keep_patch_refined==true){
        refineCell[middle]=-100.0;
      }
      /*  End tomfoolery */

      const unsigned int oldNumPar = pset->addParticles(numNewPartNeeded);

      ParticleVariable<Point> pxtmp;
      ParticleVariable<Matrix3> pFtmp,psizetmp,pstrstmp,pvgradtmp,pSFtmp;
      ParticleVariable<long64> pidstmp;
      ParticleVariable<double> pvoltmp, pmasstmp,ptemptmp,ptempPtmp,pESFtmp;
      ParticleVariable<double> pcolortmp;
      ParticleVariable<Vector> pveltmp,pextFtmp,pdisptmp,pareatmp;
      ParticleVariable<int> preftmp,ploctmp,plaltmp,pLoadCIDtmp;
      new_dw->allocateTemporary(pidstmp,  pset);
      new_dw->allocateTemporary(pxtmp,    pset);
      new_dw->allocateTemporary(pvoltmp,  pset);
      new_dw->allocateTemporary(pveltmp,  pset);
      if(flags->d_computeScaleFactor){
        new_dw->allocateTemporary(pSFtmp, pset);
      }
      new_dw->allocateTemporary(pextFtmp, pset);
      new_dw->allocateTemporary(ptemptmp, pset);
      new_dw->allocateTemporary(ptempPtmp,pset);
      new_dw->allocateTemporary(pFtmp,    pset);
      new_dw->allocateTemporary(psizetmp, pset);
      new_dw->allocateTemporary(pareatmp, pset);
      new_dw->allocateTemporary(pdisptmp, pset);
      new_dw->allocateTemporary(pstrstmp, pset);
      if (flags->d_with_color) {
        new_dw->allocateTemporary(pcolortmp,pset);
      }
      if (flags->d_useLoadCurves) {
        new_dw->allocateTemporary(pLoadCIDtmp,  pset);
      }
      new_dw->allocateTemporary(pmasstmp, pset);
      new_dw->allocateTemporary(preftmp,  pset);
      new_dw->allocateTemporary(plaltmp,  pset);
      //new_dw->allocateTemporary(ploctmp,  pset);
      new_dw->allocateTemporary(pvgradtmp,pset);
  
      // Body force acceleration
      ParticleVariable<double> pCoriolis_tmp;
      ParticleVariable<Vector> pBodyFAcc_tmp;
      new_dw->allocateTemporary(pCoriolis_tmp, pset);
      new_dw->allocateTemporary(pBodyFAcc_tmp, pset);

      // copy data from old variables for particle IDs and the position vector
      for( unsigned int pp=0; pp<oldNumPar; ++pp ){
        pidstmp[pp]  = pids[pp];
        pxtmp[pp]    = px[pp];
        pvoltmp[pp]  = pvolume[pp];
        pveltmp[pp]  = pvelocity[pp];
        pSFtmp[pp]   = pscalefac[pp];
        pextFtmp[pp] = pextforce[pp];
        ptemptmp[pp] = ptemp[pp];
        ptempPtmp[pp]= ptempP[pp];
        pFtmp[pp]    = pF[pp];
        psizetmp[pp] = pSize[pp];
        pdisptmp[pp] = pdisp[pp];
        pstrstmp[pp] = pstress[pp];
        if(flags->d_computeScaleFactor){
          pSFtmp[pp]   = pscalefac[pp];
        }
        if (flags->d_with_color) {
          pcolortmp[pp]= pcolor[pp];
        }
        if (flags->d_useLoadCurves) {
          pLoadCIDtmp[pp]= pLoadCID[pp];
        }
        pmasstmp[pp] = pmass[pp];
        preftmp[pp]  = pref[pp];
        plaltmp[pp]  = plal[pp];
        ploctmp[pp]  = ploc[pp];
        pvgradtmp[pp]= pvelgrad[pp];

        // Body force quantities
        pCoriolis_tmp[pp] = pCoriolis[pp];
        pBodyFAcc_tmp[pp] = pBodyFAcc[pp];
      }

      Vector dx = patch->dCell();
      int numRefPar=0;
      if(splitForAny){
       // Don't loop over particles unless at least one needs to be refined
      for( unsigned int idx=0; idx<oldNumPar; ++idx ){
       if(pref[idx]!=prefOld[idx]){  // do refinement!
          IntVector c_orig;
          patch->findCell(px[idx],c_orig);
          vector<Point> new_part_pos;

        // This dsize is now in terms of physical dimensions
        // The additional scaling by the grid cell size is needed
        // for determining new particle positions (below)
          Matrix3 dsize = (pF[idx]*pSize[idx]*Matrix3(dx[0],0,0,
                                                      0,dx[1],0,
                                                      0,0,dx[2]));

          // Find vectors to new particle locations, based on particle size and
          // deformation (patterned after CPDI interpolator code)
          Vector r[4];
        if(fourOrEight==8){
          r[0]=Vector(-dsize(0,0)-dsize(0,1)+dsize(0,2),
                      -dsize(1,0)-dsize(1,1)+dsize(1,2),
                      -dsize(2,0)-dsize(2,1)+dsize(2,2))*0.25;
          r[1]=Vector( dsize(0,0)-dsize(0,1)+dsize(0,2),
                       dsize(1,0)-dsize(1,1)+dsize(1,2),
                       dsize(2,0)-dsize(2,1)+dsize(2,2))*0.25;
          r[2]=Vector( dsize(0,0)+dsize(0,1)+dsize(0,2),
                       dsize(1,0)+dsize(1,1)+dsize(1,2),
                       dsize(2,0)+dsize(2,1)+dsize(2,2))*0.25;
          r[3]=Vector(-dsize(0,0)+dsize(0,1)+dsize(0,2),
                      -dsize(1,0)+dsize(1,1)+dsize(1,2),
                      -dsize(2,0)+dsize(2,1)+dsize(2,2))*0.25;

          new_part_pos.push_back(px[idx]+r[0]);
          new_part_pos.push_back(px[idx]+r[1]);
          new_part_pos.push_back(px[idx]+r[2]);
          new_part_pos.push_back(px[idx]+r[3]);
          new_part_pos.push_back(px[idx]-r[0]);
          new_part_pos.push_back(px[idx]-r[1]);
          new_part_pos.push_back(px[idx]-r[2]);
          new_part_pos.push_back(px[idx]-r[3]);
        } else if(fourOrEight==4){
          if(pSplitR1R2R3[idx]){
            // divide the particle in the direction of longest relative R-vector
            Vector R(0.,0.,0.);
            if(pSplitR1R2R3[idx]==1 || pSplitR1R2R3[idx]==2){
              //cout << "split in R1-direction!" << endl;
              R = Vector(dsize(0,0), dsize(1,0), dsize(2,0));
            } else if(pSplitR1R2R3[idx]==3 || pSplitR1R2R3[idx]==-1){
              //cout << "split in R2-direction!" << endl;
              R = Vector(dsize(0,1), dsize(1,1), dsize(2,1));
            } else if(pSplitR1R2R3[idx]==-2 || pSplitR1R2R3[idx]==-3){
              // Grab the third R-vector
              R = Vector(dsize(0,2), dsize(1,2), dsize(2,2));
              //cout << "split in R3-direction!" << endl;
            }
            new_part_pos.push_back(px[idx]-.375*R);
            new_part_pos.push_back(px[idx]-.125*R);
            new_part_pos.push_back(px[idx]+.125*R);
            new_part_pos.push_back(px[idx]+.375*R);
          } else {
            // divide the particle along x and y direction
            r[0]=Vector(-dsize(0,0)-dsize(0,1),
                        -dsize(1,0)-dsize(1,1),
                         0.0)*0.25;
            r[1]=Vector( dsize(0,0)-dsize(0,1),
                         dsize(1,0)-dsize(1,1),
                         0.0)*0.25;

            new_part_pos.push_back(px[idx]+r[0]);
            new_part_pos.push_back(px[idx]+r[1]);
            new_part_pos.push_back(px[idx]-r[0]);
            new_part_pos.push_back(px[idx]-r[1]);
          }
        }

//        cout << "OPP = " << px[idx] << endl;
        int comp=0;
        int last_index=-999;
        for(int i = 0;i<fourOrEight;i++){
//          cout << "NPP = " << new_part_pos[i] << endl;
            if(!level->containsPoint(new_part_pos[i])){
              Point anchor = level->getAnchor();
              Point orig = new_part_pos[i];
              new_part_pos[i]=Point(max(orig.x(),anchor.x()),
                                    max(orig.y(),anchor.y()),
                                    max(orig.z(),anchor.z()));
            }

            long64 cellID = ((long64)c_orig.x() << 16) |
              ((long64)c_orig.y() << 32) |
              ((long64)c_orig.z() << 48);

          int& myCellNAPID = NAPID_new[c_orig];
          int new_idx;
            if(i==0){
             new_idx=idx;
            } else {
             new_idx=oldNumPar+(fourOrEight-1)*numRefPar+i;
            }
//          cout << "new_idx = " << new_idx << endl;
          pidstmp[new_idx]    = (cellID | (long64) myCellNAPID);
          pxtmp[new_idx]      = new_part_pos[i];
          pvoltmp[new_idx]    = fourthOrEighth*pvolume[idx];
          pmasstmp[new_idx]   = fourthOrEighth*pmass[idx];
          pveltmp[new_idx]    = pvelocity[idx];
          if (flags->d_useLoadCurves) {
            pLoadCIDtmp[new_idx]  = pLoadCID[idx];
            }
          if(fourOrEight==8){
            if(flags->d_computeScaleFactor){
              pSFtmp[new_idx]     = 0.5*pscalefac[idx];
            }
            psizetmp[new_idx]   = 0.5*pSize[idx];
          } else if(fourOrEight==4){
           if(pSplitR1R2R3[idx]){
            // Divide psize in the direction of the biggest R-vector
            Matrix3 dSNew;
            if(pSplitR1R2R3[idx]==1 || pSplitR1R2R3[idx]==2){
              // Split across the first R-vector
              comp=0;
              dSNew = Matrix3(0.25*dsize(0,0), dsize(0,1), dsize(0,2),
                              0.25*dsize(1,0), dsize(1,1), dsize(1,2),
                              0.25*dsize(2,0), dsize(2,1), dsize(2,2));
            } else if(pSplitR1R2R3[idx]==3 || pSplitR1R2R3[idx]==-1){
              // Split across the second R-vector
              comp=1;
              dSNew = Matrix3(dsize(0,0), 0.25*dsize(0,1), dsize(0,2),
                              dsize(1,0), 0.25*dsize(1,1), dsize(1,2),
                              dsize(2,0), 0.25*dsize(2,1), dsize(2,2));
            } else if(pSplitR1R2R3[idx]==-2 || pSplitR1R2R3[idx]==-3){
              // Split across the third R-vector
              comp=2;
              dSNew = Matrix3(dsize(0,0), dsize(0,1), 0.25*dsize(0,2),
                              dsize(1,0), dsize(1,1), 0.25*dsize(1,2),
                              dsize(2,0), dsize(2,1), 0.25*dsize(2,2));
            }
            if(flags->d_computeScaleFactor){
              pSFtmp[new_idx]  = dSNew;
            }
            psizetmp[new_idx]= pF[idx].Inverse()*dSNew*Matrix3(1./dx[0],0.,0.,
                                                              0.,1./dx[1],0.,
                                                              0.,0.,1./dx[2]);
           } else {
              // Divide psize by two in both x and y directions
              Matrix3 ps=pscalefac[idx];
              Matrix3 tmp(0.5*ps(0,0), 0.5*ps(0,1), 0.0,
                          0.5*ps(1,0), 0.5*ps(1,1), 0.0,
                          0.0,         0.0,         ps(2,2));
              if(flags->d_computeScaleFactor){
               pSFtmp[new_idx]     = tmp;
              }
              ps = pSize[idx];
              tmp = Matrix3(0.5*ps(0,0), 0.5*ps(0,1), 0.0,
                            0.5*ps(1,0), 0.5*ps(1,1), 0.0,
                            0.0,         0.0,         ps(2,2));
              psizetmp[new_idx]   = tmp;
           }
          } // if fourOrEight==4
          pextFtmp[new_idx]   = pextforce[idx];
          pFtmp[new_idx]      = pF[idx];
          pdisptmp[new_idx]   = pdisp[idx];
          pstrstmp[new_idx]   = pstress[idx];
          if (flags->d_with_color) {
            pcolortmp[new_idx]  = pcolor[idx];
          }
          ptemptmp[new_idx]   = ptemp[idx];
          ptempPtmp[new_idx]  = ptempP[idx];
          preftmp[new_idx]    = pref[idx];
          plaltmp[new_idx]    = plal[idx];
          ploctmp[new_idx]    = ploc[idx];
          pvgradtmp[new_idx]  = pvelgrad[idx];
            NAPID_new[c_orig]++;
          last_index=new_idx;
          }
          numRefPar++;
       }  // if this particle flagged for refinement
      } // for old particles
      } // if any particles flagged for refinement

      cm->splitCMSpecificParticleData(patch, dwi, fourOrEight, prefOld, pref,
                                      oldNumPar, numNewPartNeeded,
                                      old_dw, new_dw);

      // put back temporary data
      new_dw->put(pidstmp,  lb->pParticleIDLabel_preReloc,           true);
      new_dw->put(pxtmp,    lb->pXLabel_preReloc,                    true);
      new_dw->put(pvoltmp,  lb->pVolumeLabel_preReloc,               true);
      new_dw->put(pveltmp,  lb->pVelocityLabel_preReloc,             true);
      if(flags->d_computeScaleFactor){
      new_dw->put(pSFtmp,   lb->pScaleFactorLabel_preReloc,          true);
      }
      new_dw->put(pextFtmp, lb->pExtForceLabel_preReloc,             true);
      new_dw->put(pmasstmp, lb->pMassLabel_preReloc,                 true);
      new_dw->put(ptemptmp, lb->pTemperatureLabel_preReloc,          true);
      new_dw->put(ptempPtmp,lb->pTempPreviousLabel_preReloc,         true);
      new_dw->put(psizetmp, lb->pSizeLabel_preReloc,                 true);
      new_dw->put(pdisptmp, lb->pDispLabel_preReloc,                 true);
      new_dw->put(pstrstmp, lb->pStressLabel_preReloc,               true);
      if (flags->d_with_color) {
        new_dw->put(pcolortmp,lb->pColorLabel_preReloc,              true);
      }
      if (flags->d_useLoadCurves) {
        new_dw->put(pLoadCIDtmp,lb->pLoadCurveIDLabel_preReloc,      true);
      }
      new_dw->put(pFtmp,    lb->pDefGradLabel_preReloc,   true);
      new_dw->put(preftmp,  lb->pRefinedLabel_preReloc,              true);
      new_dw->put(plaltmp,  lb->pLastLevelLabel_preReloc,            true);
      new_dw->put(ploctmp,  lb->pLocalizedMPMLabel_preReloc,         true);
      new_dw->put(pvgradtmp,lb->pVelGradLabel_preReloc,              true);

      // Body force terms
      new_dw->put(pBodyFAcc_tmp, lb->pBodyForceAccLabel_preReloc,       true);
      new_dw->put(pCoriolis_tmp, lb->pCoriolisImportanceLabel_preReloc, true);

      // put back temporary data
    }  // for matls
  }    // for patches
}
//______________________________________________________________________
//
void AMRMPM::reduceFlagsExtents(const ProcessorGroup*,
                                const PatchSubset* patches,
                                const MaterialSubset* ,
                                DataWarehouse* old_dw,
                                DataWarehouse* new_dw)
{

  // Currently doing for levels > 0
  const Level* level = getLevel(patches);
  int levelIndex = level->getIndex();
  IntVector RR_thisLevel = level->getRefinementRatio();
  int numLevels = level->getGrid()->numLevels();

  IntVector RR_RelToFinest = IntVector(1,1,1);
  if(level->hasFinerLevel()){
    RR_RelToFinest = RR_thisLevel*(numLevels-levelIndex-1);
  }

//  cout << "rFE levelIndex = " << levelIndex << endl;
//  cout << "RR_RelToFinest = " << RR_RelToFinest << endl;

  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing, "Doing reduceFlagsExtents");

    // Mark cells where particles are refined for grid refinement
    constCCVariable<double> refineCell;
    new_dw->get(refineCell, lb->MPMRefineCellLabel, 0, patch, d_gn, 0);

    int xmax,xmin,ymax,ymin,zmax,zmin;
    xmax = -999;   ymax = -999;   zmax = -999;
    xmin = 999999; ymin = 999999; zmin = 999999;
//    int print = 0;
    for(CellIterator iter=patch->getCellIterator(); !iter.done();iter++){
      IntVector c = *iter;
      if(refineCell[c]>0){
        xmax=max(xmax,c.x()); ymax=max(ymax,c.y()); zmax=max(zmax,c.z());
        xmin=min(xmin,c.x()); ymin=min(ymin,c.y()); zmin=min(zmin,c.z());
//        print = 1;
      }
    }

    xmax = xmax*RR_RelToFinest.x();
    ymax = ymax*RR_RelToFinest.y();
    zmax = zmax*RR_RelToFinest.z();
    xmin = xmin*RR_RelToFinest.x();
    ymin = ymin*RR_RelToFinest.y();
    zmin = zmin*RR_RelToFinest.z();

/*
  if (print==1){
  cout << "Xmax = " << xmax << endl;
  cout << "Ymax = " << ymax << endl;
  cout << "Zmax = " << zmax << endl;
  cout << "Xmin = " << xmin << endl;
  cout << "Ymin = " << ymin << endl;
  cout << "Zmin = " << zmin << endl;
  }
*/

    new_dw->put(max_vartype(xmax), RefineFlagXMaxLabel);
    new_dw->put(max_vartype(ymax), RefineFlagYMaxLabel);
    new_dw->put(max_vartype(zmax), RefineFlagZMaxLabel);
    new_dw->put(min_vartype(xmin), RefineFlagXMinLabel);
    new_dw->put(min_vartype(ymin), RefineFlagYMinLabel);
    new_dw->put(min_vartype(zmin), RefineFlagZMinLabel);
  }    // for patches
}

void AMRMPM::computeParticleScaleFactor(const ProcessorGroup*,
                                        const PatchSubset* patches,
                                        const MaterialSubset* ,
                                        DataWarehouse* old_dw,
                                        DataWarehouse* new_dw)
{
  // This task computes the particles initial physical size, to be used
  // in scaling particles for the deformed particle vis feature

  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing, "Doing computeParticleScaleFactor");

    int numMPMMatls=d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

      constParticleVariable<Matrix3> psize,pF;
      ParticleVariable<Matrix3> pScaleFactor;
      new_dw->get(psize,        lb->pSizeLabel_preReloc,                  pset);
      new_dw->get(pF,           lb->pDefGradLabel_preReloc,    pset);
      new_dw->allocateAndPut(pScaleFactor, lb->pScaleFactorLabel_preReloc,pset);

      if(dataArchiver->isOutputTimestep()){
        Vector dx = patch->dCell();
        for(ParticleSubset::iterator iter  = pset->begin();
            iter != pset->end(); iter++){
          particleIndex idx = *iter;
          pScaleFactor[idx] = (pF[idx]*psize[idx]*Matrix3(dx[0],0,0,
                                                          0,dx[1],0,
                                                          0,0,dx[2]));
        } // for particles
      } // isOutputTimestep
    } // matls
  } // patches

}

//______________________________________________________________________
//
void
AMRMPM::errorEstimate(const ProcessorGroup*,
                      const PatchSubset* patches,
                      const MaterialSubset* /*matls*/,
                      DataWarehouse*,
                      DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,"Doing AMRMPM::errorEstimate");

    constCCVariable<double> refineCell;
    CCVariable<int>      refineFlag;
    PerPatch<PatchFlagP> refinePatchFlag;
    new_dw->getModifiable(refineFlag, d_sharedState->get_refineFlag_label(),
                          0, patch);
    new_dw->get(refinePatchFlag, d_sharedState->get_refinePatchFlag_label(),
                0, patch);
    new_dw->get(refineCell,     lb->MPMRefineCellLabel, 0, patch, d_gn, 0);

    PatchFlag* refinePatch = refinePatchFlag.get().get_rep();

    IntVector low = patch->getCellLowIndex();
    IntVector high= patch->getCellHighIndex();
    IntVector middle = (low+high)/IntVector(2,2,2);

    for(CellIterator iter=patch->getCellIterator(); !iter.done();iter++){
      IntVector c = *iter;
      if(refineCell[c]>0.0 || refineFlag[c]==true){
        refineFlag[c] = 1;
        refinePatch->set();
      }else if(refineCell[c]<0.0 && ((int) refineCell[c])%100!=0){
        refineFlag[c] = 1;
        refinePatch->set();
      }else{
        refineFlag[c] = 0;
      }
    }

#if 0  // Alternate method of initializing refined regions.  Inactive, not
       // necessarily compatible with, say, defining levels in Grid section of
       // input.
    // loop over all the geometry objects
    for(int obj=0; obj<(int)d_refine_geom_objs.size(); obj++){
      GeometryPieceP piece = d_refine_geom_objs[obj]->getPiece();
      Vector dx = patch->dCell();
      
      int geom_level =  d_refine_geom_objs[obj]->getInitialData_int("level");
     
      // don't add refinement flags if the current level is greater than
      // the geometry level specification
      if(geom_level!=-1 && level->getIndex()>=geom_level)
        continue;

      for(CellIterator iter=patch->getExtraCellIterator(); !iter.done();iter++){
        IntVector c = *iter;
        Point  lower  = patch->nodePosition(c);
        Vector upperV = lower.asVector() + dx; 
        Point  upper  = upperV.asPoint();
        
        if(piece->inside(upper) && piece->inside(lower))
          refineFlag[c] = true;
        refinePatch->set();
      }
    }
#endif

#if 0
    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      
      // Loop over particles
      ParticleSubset* pset = new_dw->getParticleSubset(dwi, patch);
      
      constParticleVariable<Point> px;
      constParticleVariable<int> prefined;
      new_dw->get(px,       lb->pXLabel,       pset);
      new_dw->get(prefined, lb->pRefinedLabel, pset);
#if 0
      for(CellIterator iter=patch->getExtraCellIterator(); !iter.done();iter++){
        IntVector c = *iter;
        
        if(level->getIndex()==0 &&(c==IntVector(26,1,0) && step > 48)){
          refineFlag[c] = true;
          refinePatch->set();
        } else{
          refineFlag[c] = false;
        }
      }
#endif

#if 1

      for(ParticleSubset::iterator iter = pset->begin();
          iter!= pset->end();  iter++){
        if(prefined[*iter]==1){
          IntVector c = level->getCellIndex(px[*iter]);
          refineFlag[c] = true;
          cout << "refineFlag Cell = " << c << endl;
          refinePatch->set();
        }
      }
#endif
    }
#endif
  }

}
//______________________________________________________________________
//
void AMRMPM::refineGrid(const ProcessorGroup*,
                        const PatchSubset* patches,
                        const MaterialSubset* /*matls*/,
                        DataWarehouse*,
                        DataWarehouse* new_dw)
{
  // just create a particle subset if one doesn't exist
  for (int p = 0; p<patches->size(); p++) {
    const Patch* patch = patches->get(p);
    printTask(patches, patch,cout_doing,"Doing AMRMPM::refineGrid");

    CCVariable<int> cellNAPID;
    new_dw->allocateAndPut(cellNAPID, lb->pCellNAPIDLabel, 0, patch);
    cellNAPID.initialize(0);

    // First do NC_CCweight
    NCVariable<double> NC_CCweight;
    new_dw->allocateAndPut(NC_CCweight, lb->NC_CCweightLabel,  0, patch);
    //__________________________________
    // - Initialize NC_CCweight = 0.125
    // - Find the walls with symmetry BC and
    //   double NC_CCweight
    NC_CCweight.initialize(0.125);
    vector<Patch::FaceType>::const_iterator iter;
    vector<Patch::FaceType> bf;
    patch->getBoundaryFaces(bf);

    for (iter  = bf.begin(); iter != bf.end(); ++iter){
      Patch::FaceType face = *iter;
      int mat_id = 0;
      if (patch->haveBC(face,mat_id,"symmetry","Symmetric")) {

        for(CellIterator iter = patch->getFaceIterator(face,Patch::FaceNodes);
            !iter.done(); iter++) {
          NC_CCweight[*iter] = 2.0*NC_CCweight[*iter];
        }
      }
    }

    int numMPMMatls=d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      if (cout_doing.active()) {
        cout_doing <<"Doing refine on patch "
                   << patch->getID() << " material # = " << dwi << endl;
      }

      // this is a new patch, so create empty particle variables.
      if (!new_dw->haveParticleSubset(dwi, patch)) {
        ParticleSubset* pset = new_dw->createParticleSubset(0, dwi, patch);

        // Create arrays for the particle data
        ParticleVariable<Point>  px;
        ParticleVariable<double> pmass, pvolume, pTemperature;
        ParticleVariable<Vector> pvelocity, pexternalforce, pdisp,pConcGrad;
        ParticleVariable<Matrix3> psize;
        ParticleVariable<Vector>  parea;
        ParticleVariable<double> pTempPrev, pColor;
        ParticleVariable<int>    pLoadCurve,pLastLevel,pLocalized,pRefined;
        ParticleVariable<long64> pID;
        ParticleVariable<Matrix3> pdeform, pstress;
        //ParticleVariable<Matrix3> pVelGrad;
        
        new_dw->allocateAndPut(px,             lb->pXLabel,             pset);
        new_dw->allocateAndPut(pmass,          lb->pMassLabel,          pset);
        new_dw->allocateAndPut(pvolume,        lb->pVolumeLabel,        pset);
        new_dw->allocateAndPut(pvelocity,      lb->pVelocityLabel,      pset);
        new_dw->allocateAndPut(pTemperature,   lb->pTemperatureLabel,   pset);
        new_dw->allocateAndPut(pTempPrev,      lb->pTempPreviousLabel,  pset);
        new_dw->allocateAndPut(pexternalforce, lb->pExternalForceLabel, pset);
        new_dw->allocateAndPut(pID,            lb->pParticleIDLabel,    pset);
        new_dw->allocateAndPut(pdisp,          lb->pDispLabel,          pset);
        new_dw->allocateAndPut(pLastLevel,     lb->pLastLevelLabel,     pset);
        new_dw->allocateAndPut(pRefined,       lb->pRefinedLabel,       pset);
        //new_dw->allocateAndPut(pVelGrad,       lb->pVelGradLabel,       pset);
        new_dw->allocateAndPut(pvolume,        lb->pVolumeLabel,        pset);
        if (flags->d_useLoadCurves){
          new_dw->allocateAndPut(pLoadCurve,   lb->pLoadCurveIDLabel,   pset);
        }
        if (flags->d_with_color) {
          new_dw->allocateAndPut(pColor,       lb->pColorLabel,         pset);
        }
        new_dw->allocateAndPut(psize,          lb->pSizeLabel,          pset);

        // Init deformation gradient
        d_defGradComputer->initializeGradient(patch, mpm_matl, new_dw);

        mpm_matl->getConstitutiveModel()->initializeCMData(patch,
                                                           mpm_matl,new_dw);

        // Body force quantities
        ParticleVariable<Vector> pBodyFAcc;
        ParticleVariable<double> pCoriolis;
        new_dw->allocateAndPut(pBodyFAcc, lb->pBodyForceAccLabel,       pset);
        new_dw->allocateAndPut(pCoriolis, lb->pCoriolisImportanceLabel, pset);
      }
    }
  }
} // end refine()

//______________________________________________________________________
// Debugging Task that counts the number of particles in the domain.
void AMRMPM::scheduleCountParticles(const PatchSet* patches,
                                    SchedulerP& sched)
{
  printSchedule(patches,cout_doing,"AMRMPM::scheduleCountParticles");
  Task* t = scinew Task("AMRMPM::countParticles",this, 
                        &AMRMPM::countParticles);
  t->computes(lb->partCountLabel);
  sched->addTask(t, patches, d_sharedState->allMPMMaterials());
}
//______________________________________________________________________
//
void AMRMPM::countParticles(const ProcessorGroup*,
                            const PatchSubset* patches,
                            const MaterialSubset*,
                            DataWarehouse* old_dw,                           
                            DataWarehouse* new_dw)
{
  long int totalParticles=0;
  int numMPMMatls = d_sharedState->getNumMPMMatls();
  
//  const Level* level = getLevel(patches);
//  cout << "Level " << level->getIndex() << " has " << level->numPatches() << " patches" << endl;
  for (int p = 0; p<patches->size(); p++) {
    const Patch* patch = patches->get(p);
    
    printTask(patches,patch,cout_doing,"Doing AMRMPM::countParticles");
    
    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      
      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
      totalParticles += pset->end() - pset->begin();
    }
//    cout << "patch = " << patch->getID()
//         << ", numParticles = " << totalParticles << endl;
  }
  new_dw->put(sumlong_vartype(totalParticles), lb->partCountLabel);
}

//______________________________________________________________________
//  

//______________________________________________________________________
// This task colors the particles that are retrieved from the coarse level and
// used on the CFI.  This task mimics interpolateParticlesToGrid_CFI
void AMRMPM::scheduleDebug_CFI(SchedulerP& sched,
                               const PatchSet* patches,
                               const MaterialSet* matls)
{
  const Level* level = getLevel(patches);

  #define allPatches 0
  Task::MaterialDomainSpec  ND  = Task::NormalDomain;

  Task* t = scinew Task("AMRMPM::debug_CFI",
                        this,&AMRMPM::debug_CFI);
  printSchedule(patches,cout_doing,"AMRMPM::scheduleDebug_CFI");
  if(level->hasFinerLevel()){ 
    t->requires(Task::NewDW, lb->gZOILabel, allPatches, Task::FineLevel,d_one_matl, ND, d_gn, 0);
  }
  
  t->requires(Task::OldDW, lb->pXLabel,                  d_gn,0);
  t->requires(Task::OldDW, lb->pSizeLabel,               d_gn,0);
  t->requires(Task::OldDW, lb->pDefGradLabel, d_gn,0);
  
  t->computes(lb->pColorLabel_preReloc);

  sched->addTask(t, patches, matls);
}
//
//______________________________________________________________________
void AMRMPM::debug_CFI(const ProcessorGroup*,
                       const PatchSubset* patches,
                       const MaterialSubset* ,
                       DataWarehouse* old_dw,
                       DataWarehouse* new_dw)
{
  const Level* level = getLevel(patches);
  
  for(int cp=0; cp<patches->size(); cp++){
    const Patch* patch = patches->get(cp);

    printTask(patches,patch,cout_doing,"Doing AMRMPM::debug_CFI");
    
    
    //__________________________________
    //  Write p.color all levels all patches  
    ParticleSubset* pset=0;
    int dwi = 0;
    pset = old_dw->getParticleSubset(dwi, patch);
    
    constParticleVariable<Point>  px;
    constParticleVariable<Matrix3> psize;
    constParticleVariable<Matrix3> pDefGrad;
    ParticleVariable<double>  pColor;
    
    old_dw->get(px,                   lb->pXLabel,                  pset);
    new_dw->get(psize,                lb->pSizeLabel,               pset);
    old_dw->get(pDefGrad,  lb->pDefGradLabel, pset);
    new_dw->allocateAndPut(pColor,    lb->pColorLabel_preReloc,     pset);
    
    ParticleInterpolator* interpolatorCoarse = flags->d_interpolator->clone(patch);
    vector<IntVector> ni(interpolatorCoarse->size());
    vector<double> S(interpolatorCoarse->size());

    for (ParticleSubset::iterator iter = pset->begin();iter != pset->end(); iter++){
      particleIndex idx = *iter;
      pColor[idx] = 0;
      
      int NN = interpolatorCoarse->findCellAndWeights(px[idx],ni,S,psize[idx],
                                                     pDefGrad[idx]);
      
      for(int k = 0; k < NN; k++) {
        pColor[idx] += S[k];
      }
    }  

    //__________________________________
    //  Mark the particles that are accessed at the CFI.
    if(level->hasFinerLevel()){  

      // find the fine patches over the coarse patch.  Add a single layer of cells
      // so you will get the required patches when coarse patch and fine patch boundaries coincide.
      Level::selectType finePatches;
      patch->getOtherLevelPatches55902(1, finePatches, 1);

      const Level* fineLevel = level->getFinerLevel().get_rep();
      IntVector refineRatio(fineLevel->getRefinementRatio());

      for(int fp=0; fp<finePatches.size(); fp++){
        const Patch* finePatch = finePatches[fp];

        // Determine extents for coarser level particle data
        // Linear Interpolation:  1 layer of coarse level cells
        // Gimp Interpolation:    2 layers
        /*`==========TESTING==========*/
        IntVector nLayers(d_nPaddingCells_Coarse, d_nPaddingCells_Coarse, d_nPaddingCells_Coarse);
        IntVector nPaddingCells = nLayers * (refineRatio);
        //cout << " nPaddingCells " << nPaddingCells << "nLayers " << nLayers << endl;
        /*===========TESTING==========`*/

        int nGhostCells = 0;
        bool returnExclusiveRange=false;
        IntVector cl_tmp, ch_tmp, fl, fh;

        getCoarseLevelRange(finePatch, level, cl_tmp, ch_tmp, fl, fh, 
                            nPaddingCells, nGhostCells,returnExclusiveRange);
                            
        cl_tmp -= finePatch->neighborsLow() * nLayers;  //  expand cl_tmp when a neighor patch exists.
                                                        //  This patch owns the low nodes.  You need particles
                                                        //  from the neighbor patch.

        // coarseLow and coarseHigh cannot lie outside of the coarse patch
        IntVector cl = Max(cl_tmp, patch->getCellLowIndex());
        IntVector ch = Min(ch_tmp, patch->getCellHighIndex());

        ParticleSubset* pset2=0;
        pset2 = old_dw->getParticleSubset(dwi, cl, ch, patch,lb->pXLabel);
        
        constParticleVariable<Point>  px_CFI;
        constNCVariable<Stencil7> zoi;
        old_dw->get(px_CFI, lb->pXLabel,  pset2);
        new_dw->get(zoi,    lb->gZOILabel, 0, finePatch, Ghost::None, 0 );

        ParticleInterpolator* interpolatorFine = flags->d_interpolator->clone(finePatch);

        for (ParticleSubset::iterator iter = pset->begin();iter != pset->end(); iter++){
          particleIndex idx = *iter;
          
          for (ParticleSubset::iterator iter2 = pset2->begin();iter2 != pset2->end(); iter2++){
            particleIndex idx2 = *iter2;
            
            if( px[idx] == px_CFI[idx2] ){       
              pColor[idx] = 0;
              vector<IntVector> ni;
              vector<double> S;
              interpolatorFine->findCellAndWeights_CFI(px[idx],ni,S,zoi); 
              for(int k = 0; k < (int)ni.size(); k++) {
                pColor[idx] += S[k];
              }
            }
          }  // pset2 loop
        }  // pset loop
        delete interpolatorFine;
      }  // loop over fine patches
    }  //// hasFinerLevel
    delete interpolatorCoarse;
  }  // End loop over coarse patches
}

//______________________________________________________________________
//    This removes duplicate entries in the array
void AMRMPM::removeDuplicates( Level::selectType& array)
{
  int length = array.size();
  if ( length <= 1 ){
    return;
  }
  
  int newLength = 1;        // new length of modified array
  int i, j;
  
  for(i=1; i< length; i++){
    for(j=0; j< newLength; j++){
      if( array[i] == array[j] ){
        break;
      }
    }
    // if none of the values in array[0..j] == array[i],
    // then copy the current value to a new position in array

    if (j == newLength ){
      array[newLength++] = array[i];
    }
  }
  array.resize(newLength);
}


//______________________________________________________________________
//  Returns the fine and coarse level patches that have coarse fine interfaces
void AMRMPM::coarseLevelCFI_Patches(const PatchSubset* coarsePatches,
                                    Level::selectType& CFI_coarsePatches,
                                    Level::selectType& CFI_finePatches )
{
  const Level* coarseLevel = getLevel(coarsePatches);
  if( !coarseLevel->hasFinerLevel()) {
    return;
  }

  for(int p=0;p<coarsePatches->size();p++){
    const Patch* coarsePatch = coarsePatches->get(p);
    bool addMe = false;

    Level::selectType finePatches;
    coarsePatch->getFineLevelPatches(finePatches);
    // loop over all the coarse level patches

    for(int fp=0;fp<finePatches.size();fp++){  
      const Patch* finePatch = finePatches[fp];
      
      if(finePatch->hasCoarseFaces() ){
        addMe = true;
        CFI_finePatches.push_back( finePatch );
      }
    }
    
    if( addMe ){  // only add once
      CFI_coarsePatches.push_back( coarsePatch );
  }
}

  // remove duplicate patches
  removeDuplicates( CFI_coarsePatches );
  removeDuplicates( CFI_finePatches );

}

//______________________________________________________________________
//  Returns the fine patches that have a CFI and all of the underlying
//  coarse patches beneath those patches.  We don't know which of the coarse
//  patches are beneath the fine patch with the CFI.
// This takes in fine level patches
void AMRMPM::fineLevelCFI_Patches(const PatchSubset* finePatches,
                                  Level::selectType& coarsePatches,
                                  Level::selectType& CFI_finePatches )
{
  const Level* fineLevel = getLevel(finePatches);
      
  if( !fineLevel->hasCoarserLevel()) {
    return;
  }
  
  for(int p=0;p<finePatches->size();p++){
    const Patch* finePatch = finePatches->get(p);
    
    if(finePatch->hasCoarseFaces() ){
      CFI_finePatches.push_back( finePatch );
      
      // need to use the Node Based version of getOtherLevel
      finePatch->getOtherLevelPatchesNB(-1,coarsePatches,0);  
    } 
  }
  removeDuplicates( coarsePatches );
  removeDuplicates( CFI_finePatches );
}


#if 0  // May need to reactivate for GIMP
//______________________________________________________________________
//
void AMRMPM::scheduleInterpolateToParticlesAndUpdate_CFI(SchedulerP& sched,
                                                         const PatchSet* patches,
                                                         const MaterialSet* matls)

{
  const Level* level = getLevel(patches);
  
  if(level->hasFinerLevel()){

    printSchedule(patches,cout_doing,"AMRMPM::scheduleInterpolateToParticlesAndUpdate_CFI");

    Task* t=scinew Task("AMRMPM::interpolateToParticlesAndUpdate_CFI",
                        this, &AMRMPM::interpolateToParticlesAndUpdate_CFI);

    Ghost::GhostType  gn  = Ghost::None;
    Task::MaterialDomainSpec  ND  = Task::NormalDomain;
#define allPatches 0
#define allMatls 0
    t->requires(Task::OldDW, d_sharedState->get_delt_label() );
    
    t->requires(Task::OldDW, lb->pXLabel, gn);
    t->requires(Task::NewDW, lb->gVelocityStarLabel, allPatches, Task::FineLevel,allMatls,   ND, d_gn,0);
    t->requires(Task::NewDW, lb->gAccelerationLabel, allPatches, Task::FineLevel,allMatls,   ND, d_gn,0);
    t->requires(Task::NewDW, lb->gZOILabel,          allPatches, Task::FineLevel,d_one_matl, ND, d_gn,0);
    
    t->modifies(lb->pXLabel_preReloc);
    t->modifies(lb->pDispLabel_preReloc);
    t->modifies(lb->pVelocityLabel_preReloc);

    sched->addTask(t, patches, matls);
  }
}
#endif

#if 0  // May need to reactivate for GIMP
//______________________________________________________________________
//
void AMRMPM::interpolateToParticlesAndUpdate_CFI(const ProcessorGroup*,
                                                 const PatchSubset* coarsePatches,
                                                 const MaterialSubset* ,
                                                 DataWarehouse* old_dw,
                                                 DataWarehouse* new_dw)
{
  const Level* coarseLevel = getLevel(coarsePatches);
  const Level* fineLevel = coarseLevel->getFinerLevel().get_rep();
  
  delt_vartype delT;
  old_dw->get(delT, d_sharedState->get_delt_label(), coarseLevel );
  
  double move_particles=1.;
  if(!flags->d_doGridReset){
    move_particles=0.;
  }
  
  //__________________________________
  //Loop over the coarse level patches
  for(int p=0;p<coarsePatches->size();p++){
    const Patch* coarsePatch = coarsePatches->get(p);
    printTask(coarsePatches,coarsePatch,cout_doing,"AMRMPM::interpolateToParticlesAndUpdate_CFI");

    int numMatls = d_sharedState->getNumMPMMatls();
        
    Level::selectType finePatches;
    coarsePatch->getFineLevelPatches(finePatches);
    
    
    //__________________________________
    //  Fine patch loop
    for(int i=0;i<finePatches.size();i++){
      const Patch* finePatch = finePatches[i]; 
      
      if(finePatch->hasCoarseFaces()){

        ParticleInterpolator* interpolator = flags->d_interpolator->clone(finePatch);
        
        constNCVariable<Stencil7> zoi_fine;
        new_dw->get(zoi_fine, lb->gZOILabel, 0, finePatch, Ghost::None, 0 );

        for(int m = 0; m < numMatls; m++){
          MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
          int dwi = mpm_matl->getDWIndex();

          // get fine level grid data
          constNCVariable<double> gmass_fine;
          constNCVariable<Vector> gvelocity_star_fine;
          constNCVariable<Vector> gacceleration_fine;

          // use getRegion() instead of get().  They should be equivalent but 
          // get() throws assert on parallel runs.
          IntVector fl = finePatch->getNodeLowIndex();
          IntVector fh = finePatch->getNodeHighIndex();
          new_dw->getRegion(gvelocity_star_fine,  lb->gVelocityStarLabel, dwi, fineLevel,fl, fh);   
          new_dw->getRegion(gacceleration_fine,   lb->gAccelerationLabel, dwi, fineLevel,fl, fh); 
            
          
          // get coarse level particle data
          ParticleVariable<Point>  pxnew_coarse;
          ParticleVariable<Vector> pdispnew_coarse;
          ParticleVariable<Vector> pvelocitynew_coarse;
          constParticleVariable<Point>  pxold_coarse;
          
          ParticleSubset* pset=0;
          
/*`==========TESTING==========*/
          // get the particles for the entire coarse patch
          // Ideally you only need the particle subset in the cells
          // that surround the fine patch.  Currently, getModifiable doesn't
          // allow you to get a pset with a high/low index that does not match
          // the patch low high index  
          pset = old_dw->getParticleSubset(dwi, coarsePatch);
          //cout << *pset << endl; 
/*===========TESTING==========`*/
          old_dw->get(pxold_coarse,                  lb->pXLabel,                 pset);
          new_dw->getModifiable(pxnew_coarse,        lb->pXLabel_preReloc,        pset);
          new_dw->getModifiable(pdispnew_coarse,     lb->pDispLabel_preReloc,     pset);
          new_dw->getModifiable(pvelocitynew_coarse, lb->pVelocityLabel_preReloc, pset);


          for (ParticleSubset::iterator iter = pset->begin();
               iter != pset->end(); iter++){
            particleIndex idx = *iter;

            // Get the node indices that surround the fine patch cell
            vector<IntVector> ni;
            vector<double> S;
            
            interpolator->findCellAndWeights_CFI(pxold_coarse[idx],ni,S,zoi_fine);

            Vector acc(0.0, 0.0, 0.0); 
            Vector vel(0.0, 0.0, 0.0);

            // Add each nodes contribution to the particle's velocity & acceleration 
            IntVector fineNode;
            for(int k = 0; k < (int)ni.size(); k++) {
              
              fineNode = ni[k];
            
              vel  += gvelocity_star_fine[fineNode] * S[k];
              acc  += gacceleration_fine[fineNode]  * S[k];
/*`==========TESTING==========*/
#ifdef DEBUG_ACC 
              Vector diff = acc - d_acc_ans;
              if( abs(acc.length() - d_acc_ans.length() > d_acc_tol ) ) {
                const Level* fineLevel = coarseLevel->getFinerLevel().get_rep();
                cout << "    L-"<< fineLevel->getIndex() << " node: "<< fineNode << " gacceleration: " << gacceleration_fine[fineNode] 
                     << "  diff " << diff << endl;
              }
#endif 
/*===========TESTING==========`*/
            }
            
//            cout << " pvelocitynew_coarse  "<< idx << " "  << pvelocitynew_coarse[idx] << " p.x " << pxnew_coarse[idx] ;
            
            // Update the particle's position and velocity
            pxnew_coarse[idx]         += vel*delT;  
            pdispnew_coarse[idx]      += vel*delT;                 
            pvelocitynew_coarse[idx]  += acc*delT; 
            
          } // End of particle loop
        } // End loop over materials 
      
        delete interpolator;
      }  // if has coarse face
    }  // End loop over fine patches 
  }  // End loop over patches
}
#endif




