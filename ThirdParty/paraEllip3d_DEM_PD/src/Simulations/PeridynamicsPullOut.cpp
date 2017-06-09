#include <Simulations/PeridynamicsPullOut.h>
#include <Core/Util/Utility.h>

using namespace dem;
using util::combine;
void
PeridynamicsPullOut::execute(Assembly* assembly)
{
  std::ofstream progressInf;
  std::ofstream periProgInf;
  std::ofstream periProgInfHalf;

  REAL distX, distY, distZ;
  if (assembly->getMPIRank() == 0) {
    distX = util::getParam<REAL>("Xmax") -
            util::getParam<REAL>("Xmin");
    distY = util::getParam<REAL>("Ymax") -
            util::getParam<REAL>("Ymin");
    distZ = util::getParam<REAL>("Zmax") -
            util::getParam<REAL>("Zmin");
    REAL x1 = util::getParam<REAL>("Xmin");
    REAL x2 = util::getParam<REAL>("Xmax");
    REAL y1 = util::getParam<REAL>("Ymin");
    REAL y2 = util::getParam<REAL>("Ymax");
    REAL z1 = util::getParam<REAL>("Zmin");
    REAL z2 = util::getParam<REAL>("Zmax");
    assembly->setContainer(Box(x1, y1, z1, x2, y2, z2));
    assembly->setGrid(
      Box(x1, y1, z1, x2, y2, z2)); // compute grid assumed to be
                                    // the same as container,
                                    // change in scatterParticle()
                                    // if necessary.
    assembly->readParticles(
      Parameter::get().datafile["particleFile"]);
    assembly->readPeriDynamicsData(
      Parameter::get().datafile["periFile"]);
    assembly->openCompressProg(progressInf, "rigidInc_progress");
    assembly->openPeriProgress(periProgInf, "rigidInc_peri.dat");
    assembly->openPeriProgress(periProgInfHalf, "rigidInc_peri_half.dat");
    assembly->calcParticleVolume(); // volume and horizon size are related to
                                    // the mesh,
                                    // July 14, 2014
    assembly->calcHorizonSize();    // so these two should be calculated before
                                    // peri-points
    // that are within sand particles are deleted
    assembly
      ->removeInsidePeriParticles(); // delete those peri-points that are inside
                                     // sand particles
  }
  assembly->scatterDEMPeriParticle();
  assembly
    ->constructPeriMatrix(); // construct the Matrix members in periParticleVec
  assembly->constructNeighbor();

  // if(assembly->getMPIRank()==0){
  // assembly->printPeriDomain("peridomain_rank_0.dat");
  // assembly->printRecvPeriDomain("peridomain_recv_rank_0.dat");    // bondVec
  // is not
  // empty
  //}
  // if(assembly->getMPIRank()==1){
  // assembly->printPeriDomain("peridomain_rank_1.dat");
  // assembly->printRecvPeriDomain("peridomain_recv_rank_1.dat");    // bondVec
  // is empty
  //}

  auto startStep = util::getParam<std::size_t>("startStep");
  auto endStep = util::getParam<std::size_t>("endStep");
  auto startSnap = util::getParam<std::size_t>("startSnap");
  auto endSnap = util::getParam<std::size_t>("endSnap");
  std::size_t netStep = endStep - startStep + 1;
  std::size_t netSnap = endSnap - startSnap + 1;
  timeStep = util::getParam<REAL>("timeStep");

  // REAL time0, time1, time2, commuT, migraT, gatherT, totalT;
  iteration = startStep;
  std::size_t iterSnap = startSnap;
  std::string outputFolder(".");
  if (assembly->getMPIRank() == 0) {

    // Create the output writer in the master process
    // <outputFolder> rigidInc.pe3d </outputFolder>
    auto folderName =  dem::Parameter::get().datafile["outputFolder"];
    outputFolder = util::createOutputFolder(folderName);
    //std::cout << "Output folder = " << outputFolder << "\n";
    assembly->createOutputWriter(outputFolder, iterSnap-1);

    assembly->plotGrid();
    assembly->plotParticle();
    assembly->printPeriProgress(periProgInf, 0);
    assembly->printPeriProgressHalf(periProgInfHalf, 0);
  }
  //    if (assembly->getMPIRank() == 0)
  //      debugInf << std::setw(OWID) << "iter" << std::setw(OWID) << "commuT"
  //      << std::setw(OWID) << "migraT"
  //           << std::setw(OWID) << "totalT" << std::setw(OWID) << "overhead%"
  //           << std::endl;

  // Broadcast the output folder to all processes
  broadcast(assembly->getMPIWorld(), outputFolder, 0);

  assembly->commuParticle(); // the commuPeriParticle() have to be called after
                             // commuParticle(), since
  assembly->commuPeriParticle(); // rankX1... are calculated in commuParticle()
  assembly->constructRecvPeriMatrix(); // construct the Matrix members in
                                       // recvPeriParticle

  assembly
    ->findRecvPeriBonds(); // find the peri-bonds between periParticleVec and
                           // recvPeriParticle
  assembly->calcParticleKinv();
  for (auto& pt : assembly->getPeriParticleVec()) {
    pt->initial();
  }
  for (auto& pt : assembly->getRecvPeriParticleVec()) {
    pt->initial();
  }
  assembly->calcParticleStress();
  assembly->calcParticleAcceleration();
  assembly->releaseRecvParticle();
  //    releaseRecvPeriParticle();
  //    releasePeriBondVec();    // free the bondVec in periParticleVec and
  //    bondVec.clear() before constructNeighbor() again
  assembly->findBoundaryPeriParticles();
  assembly->clearPeriDEMBonds();
  assembly->findPeriDEMBonds();
  while (iteration <= endStep) {
    //      findBoundaryPeriParticles();    // boundary peri-points are
    //      determined by their initial positions, however they are still needed
    //      to be determined
    //      findFixedPeriParticles();        // every step since some
    //      peri-points can pass from one cpu to another
    // commuT = migraT = gatherT = totalT = 0; time0 = MPI_Wtime();
    //      commuParticle(); time2 = MPI_Wtime(); commuT = time2 - time0;

    // displacement control relies on constant time step, so do not call
    // calcTimeStep().
    // calcTimeStep(); // use values from last step, must call before findConact
    assembly->findContact();
    if (assembly->isBdryProcess())
      assembly->findBdryContact();

    assembly->clearContactForce();
    assembly->internalForce();
    if (assembly->isBdryProcess())
      assembly->boundaryForce();
    assembly->runFirstHalfStep();

    assembly->applyPeriBoundaryCondition();
    //      commuPeriParticle();
    //      constructRecvPeriMatrix();
    //      constructNeighbor();
    //      checkBondParticleAlive();
    assembly->findRecvPeriBonds();

    assembly->calcParticleStress();
    assembly->calcParticleAcceleration(); // peri-point acceleration is set zero
                                          // at the
                                          // beginning

    assembly
      ->findPeriDEMBonds(); // based on current states of DEM particles and
                            // peri-points
    assembly->applyCoupledForces();
    assembly->runSecondHalfStep();

    assembly->updateParticle();
    assembly->gatherBdryContact(); // must call before updateBoundary
    //      updateBoundary(sigmaConf, "triaxial");
    //      updateGrid();
    //      updatePeriGrid();

    if (iteration % (netStep / netSnap) == 0) {
      // time1 = MPI_Wtime();
      assembly->gatherParticle();
      assembly->gatherPeriParticle();
      assembly->gatherEnergy();
      // time2 = MPI_Wtime(); gatherT = time2 - time1;

      //    checkBondParticleAlive();
      //    findPeriDEMBonds();    // update peri-dem bonds

      if (assembly->getMPIRank() == 0) {
        assembly->updateFileNames(iterSnap);
        assembly->plotBoundary();
        assembly->plotGrid();
        assembly->plotParticle();
        assembly->printBdryContact();
        assembly->printBoundary();
        // printCompressProg(progressInf, distX, distY, distZ); // redundant
        assembly->printPeriProgress(periProgInf, iterSnap);
        assembly->printPeriProgressHalf(periProgInfHalf, iterSnap);
      }
      assembly->printContact(combine(".", "rigidInc_contact_", iterSnap, 3));
      ++iterSnap;
    }
    //      releaseRecvParticle(); // late release because printContact refers
    //      to received particles
    //      releaseRecvPeriParticle();    // release recvPeriParticleVec and
    //      also remove peri-bonds between recvPeriParticle
    //      releasePeriBondVec();
    // time1 = MPI_Wtime();
    //      migrateParticle();
    //      migratePeriParticle();
    // time2 = MPI_Wtime(); migraT = time2 - time1; totalT = time2 - time0;
    //      if (assembly->getMPIRank() == 0 && (iteration+1 ) % (netStep /
    //      netSnap) == 0) //
    //      ignore gather and print time at this step
    //    debugInf << std::setw(OWID) << iteration << std::setw(OWID) << commuT
    //    << std::setw(OWID) << migraT
    //         << std::setw(OWID) << totalT << std::setw(OWID) << (commuT +
    //         migraT)/totalT*100 << std::endl;

    if (assembly->getMPIRank() == 0 && iteration % 10 == 0)
      assembly->printCompressProg(progressInf, distX, distY, distZ);

    // no break condition, just through top/bottom displacement control
    ++iteration;
  }

  if (assembly->getMPIRank() == 0) {
    assembly->updateFileNames(iterSnap, ".end");
    assembly->plotParticle();
    assembly->printBdryContact();
    assembly->printBoundary();
    assembly->printCompressProg(progressInf, distX, distY, distZ);
    //      assembly->printPeriProgress(periProgInf, iterSnap);
  }
  if (assembly->getMPIRank() == 0) {
    assembly->closeProg(progressInf);
    assembly->closeProg(periProgInf);
    assembly->closeProg(periProgInfHalf);
  }
}
