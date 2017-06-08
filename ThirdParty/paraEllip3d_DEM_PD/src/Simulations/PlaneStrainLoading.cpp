#include <Simulations/PlaneStrainLoading.h>
#include <Core/Util/Utility.h>

using namespace dem;
using util::combine;
void
PlaneStrainLoading::execute(Assembly* assembly)
{
  std::ofstream progressInf;

  if (assembly->getMPIRank() == 0) {
    assembly->readBoundary(
      Parameter::get().datafile["boundaryFile"]);
    assembly->readParticles(
      Parameter::get().datafile["particleFile"]);
    assembly->openCompressProg(progressInf, "plnstrn_progress");
  }
  assembly->scatterParticle();

  auto startStep = util::getParam<std::size_t>("startStep");
  auto endStep = util::getParam<std::size_t>("endStep");
  auto startSnap = util::getParam<std::size_t>("startSnap");
  auto endSnap = util::getParam<std::size_t>("endSnap");
  std::size_t netStep = endStep - startStep + 1;
  std::size_t netSnap = endSnap - startSnap + 1;
  REAL sigmaConf = util::getParam<REAL>("sigmaConf");
  timeStep = util::getParam<REAL>("timeStep");

  REAL time0, time1, time2, commuT, migraT, gatherT, totalT;
  iteration = startStep;
  std::size_t iterSnap = startSnap;
  REAL distX, distY, distZ;
  std::string outputFolder(".");
  if (assembly->getMPIRank() == 0) {

    // Create the output writer in the master process
    // <outputFolder> plnstrn.pe3d </outputFolder>
    auto folderName =  dem::Parameter::get().datafile["outputFolder"];
    outputFolder = util::createOutputFolder(folderName);
    std::cout << "Output folder = " << outputFolder << "\n";
    assembly->createOutputWriter(outputFolder, iterSnap-1);

    assembly->plotBoundary();
    assembly->plotGrid();
    assembly->plotParticle();
    assembly->printBdryContact();
    assembly->printBoundary();
    assembly->getStartDimension(distX, distY, distZ);
  }
  if (assembly->getMPIRank() == 0)
    debugInf << std::setw(OWID) << "iter" << std::setw(OWID) << "commuT"
             << std::setw(OWID) << "migraT" << std::setw(OWID) << "totalT"
             << std::setw(OWID) << "overhead%" << std::endl;

  // Broadcast the output folder to all processes
  broadcast(assembly->getMPIWorld(), outputFolder, 0);

  while (iteration <= endStep) {
    commuT = migraT = gatherT = totalT = 0;
    time0 = MPI_Wtime();
    assembly->commuParticle();
    time2 = MPI_Wtime();
    commuT = time2 - time0;

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

    assembly->updateParticle();
    assembly->gatherBdryContact(); // must call before updateBoundary
    assembly->updateBoundary(sigmaConf, "plnstrn");
    assembly->updateGrid();

    if (iteration % (netStep / netSnap) == 0) {
      time1 = MPI_Wtime();
      assembly->gatherParticle();
      assembly->gatherEnergy();
      time2 = MPI_Wtime();
      gatherT = time2 - time1;

      if (assembly->getMPIRank() == 0) {
        assembly->updateFileNames(iterSnap);
        assembly->plotBoundary();
        assembly->plotGrid();
        assembly->plotParticle();
        assembly->printBdryContact();
        assembly->printBoundary();
        // assembly->printCompressProg(progressInf, distX, distY, distZ); //
        // redundant
      }
      assembly->printContact(combine(".", "plnstrn_contact_", iterSnap, 3));
      ++iterSnap;
    }

    assembly->releaseRecvParticle(); // late release because
                                     // assembly->printContact refers to
                                     // received particles
    time1 = MPI_Wtime();
    assembly->migrateParticle();
    time2 = MPI_Wtime();
    migraT = time2 - time1;
    totalT = time2 - time0;
    if (assembly->getMPIRank() == 0 &&
        (iteration + 1) % (netStep / netSnap) ==
          0) // ignore gather and assembly->print time at this step
      debugInf << std::setw(OWID) << iteration << std::setw(OWID) << commuT
               << std::setw(OWID) << migraT << std::setw(OWID) << totalT
               << std::setw(OWID) << (commuT + migraT) / totalT * 100
               << std::endl;

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
  }

  if (assembly->getMPIRank() == 0)
    assembly->closeProg(progressInf);
}
