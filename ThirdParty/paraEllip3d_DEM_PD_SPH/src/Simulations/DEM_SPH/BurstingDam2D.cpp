#include <Simulations/DEM_SPH/BurstingDam2D.h>
#include <SmoothParticleHydro/SPHParticleCreator.h>
#include <Core/Util/Utility.h>

using namespace dem;

void 
BurstingDam2D::execute(DiscreteElements* dem, sph::SmoothParticleHydro* sph) 
{     
  std::ofstream demProgressInf;
  std::ofstream sphProgressInf;

  REAL ghostWidth = 0.0;
  REAL bufferLength = 0.0;

  if (dem->getMPIRank() == 0) {
    auto boundaryFilename = util::getFilename("boundaryFilename");
    dem->readBoundary(boundaryFilename);

    auto particleFilename = util::getFilename("particleFilename");
    dem->readParticles(particleFilename);

    Box domain = dem->getSpatialDomain();
    DEMParticlePArray demParticles= dem->getAllDEMParticleVec();
    sph::SPHParticleCreator creator;
    sph::SPHParticlePArray sphParticles = 
      creator.generateSPHParticleDam<2>(domain, demParticles);
    sph->setAllSPHParticleVec(sphParticles);

    dem->openProgressOutputFile(demProgressInf, "deposit_progress");
    //sph->openSPHTecplot(sphTecplotInf, "sph_results.dat");

    ghostWidth = dem->getGradation().getPtclMaxRadius() * 2;
    bufferLength = dem->getPtclMaxZ(dem->getAllDEMParticleVec()) +
                   dem->getGradation().getPtclMaxRadius();
  }

  // Have to broadcast the ghostWidth and bufferLength to all processes
  // (**TODO** Think of a better way to do this)
  broadcast(dem->getMPIWorld(), ghostWidth, 0);
  broadcast(dem->getMPIWorld(), bufferLength, 0);

  // scatter particles only once; also updates patchGrid for the first time
  dem->scatterParticles(); 
  sph->scatterSPHParticle(dem->getSpatialDomain(), ghostWidth, bufferLength); 

  // sph parameters
  //auto gravAccel = util::getParam<REAL>("gravAccel");
  //auto gravScale = util::getParam<REAL>("gravScale");
  auto waterLength = util::getParam<REAL>("waterLength");
  auto nSPHPoint = util::getParam<std::size_t>("nSPHPoint");
  
  //REAL D = 5.0*gravAccel*gravScale*waterLength;
  REAL space_interval = waterLength/static_cast<REAL>(nSPHPoint-1);
  REAL smoothLength = 1.5*space_interval;
  REAL kernelSize = 3.0*smoothLength;
  //REAL sphCellSize = dem->getGradation().getPtclMaxRadius() + kernelSize;
  // REAL one_devide_h = 1.0/smoothLength;
  // REAL factor_kernel = 7.0/(478.0*Pi*smoothLength*smoothLength);
  // REAL factor_kernel_gradient = factor_kernel/smoothLength;
  // REAL qmin = 0.7593;	// this is fixed for quintic kernel function
  // REAL Wqmin = sph->kernelFunction(qmin);
  // REAL p1 = 4;	// for the Lennard-Jones boundary forces 
  // REAL p2 = 2;

  auto startStep = util::getParam<std::size_t>("startStep");
  auto endStep = util::getParam<std::size_t>("endStep");
  auto startSnap = util::getParam<std::size_t>("startSnap");
  auto endSnap = util::getParam<std::size_t>("endSnap");
  auto timeStep = util::getParam<REAL>("timeStep");
  auto timeAccrued = util::getParam<REAL>("timeAccrued");

  auto netStep = endStep - startStep + 1;
  auto netSnap = endSnap - startSnap + 1;
  REAL timeCount = 0;
  REAL timeIncr  = timeStep * netStep;
  REAL timeTotal = timeAccrued + timeStep * netStep;

  auto iteration = startStep;
  auto iterSnap = startSnap;
  std::string outputFolder(".");
  if (dem->getMPIRank() == 0) {

    // Create the output writer in the master process
    // <outputFolder> rigidInc.pe3d </outputFolder>
    auto folderName =  dem::InputParameter::get().datafile["outputFolder"];
    outputFolder = util::createOutputFolder(folderName);
    //std::cout << "Output folder = " << outputFolder << "\n";
    dem->createOutputWriter(outputFolder, iterSnap-1);
    sph->createOutputWriter(outputFolder, iterSnap-1);

    dem->writeBoundaryToFile();
    dem->writePatchGridToFile();
    dem->writeParticlesToFile(iterSnap);
    dem->printBoundaryContacts();
    //sph->printSPHTecplot(sphTecplotInf, iterSnap-1);
    sph->writeParticlesToFile(iterSnap);
    //sph->printSPHProgress(sphProgressInf, 0);
  }

  // Broadcast the output folder to all processes
  broadcast(dem->getMPIWorld(), outputFolder, 0);
  std::cerr << "Proc = " << dem->getMPIRank() << " outputFolder = " << outputFolder << "\n";

  dem->communicateGhostParticles();

  sph->commuSPHParticle(iteration, ghostWidth);

  // initialization SPH velocity based on equation(4.3) in 
  // http://www.artcompsci.org/vol_1/v1_web/node34.html
  sph->initializeSPHVelocity<2>(timeStep);	

  while (timeAccrued < timeTotal) { 

    bool toCheckTime = (iteration + 1) % (netStep / netSnap) == 0;

    // update position and density of SPH particles based on equation (4.1)
    sph->updateSPHLeapFrogPositionDensity(timeStep);

    sph->migrateSPHParticle(iteration);

    REAL commuT = 0, migraT = 0, gatherT = 0, totalT = 0;  
    REAL time1 = 0, time2 = 0, time3 = 0;  
    REAL time0 = MPI_Wtime();

    dem->communicateGhostParticles();
    sph->commuSPHParticle(iteration, ghostWidth);

    if (toCheckTime) {
      time1 = MPI_Wtime(); 
      commuT = time1 - time0;
    }

    // use values from last step, must call before findContact (which clears data)
    dem->calcTimeStep(); 
    dem->findContact(iteration);
    if (dem->isBoundaryProcess()) dem->findBoundaryContacts(iteration);

    dem->initializeForces();
    dem->internalForce(iteration);
    if (dem->isBoundaryProcess()) dem->boundaryForce(iteration);

    dem->dragForce();

    sph->updateParticleInteractions<2>(dem->getSpatialDomain(),
                                       bufferLength, ghostWidth,
                                       kernelSize, smoothLength);
                                       

    dem->updateParticles(iteration);   

    // update velocity of SPH particles based on equation (4.2)
    sph->updateSPHLeapFrogVelocity(timeStep);	

    // updatePatchBoxMaxZ();	// do not update patchGrid in DEM-SPH coupling model
    // updatePatchBoxMaxZ() is for deposition or explosion. If they go out of side walls, particles are discarded.
    // updatePatchBox() updates all six directions, thus side walls may "disappear" if particles go far out of side walls 
    // and cause some patchGrids to extrude out of side walls.

    timeCount += timeStep;
    timeAccrued += timeStep;
    if (timeCount >= timeIncr/netSnap) { 

      if (toCheckTime) {
        time2 = MPI_Wtime();
      }

      dem->gatherParticles();
      sph->gatherSPHParticle();
      dem->gatherBoundaryContacts();
      dem->gatherEnergy(); 
      
      if (toCheckTime) {
        time3 = MPI_Wtime(); 
        gatherT = time3 - time2;
      }

      if (dem->getMPIRank() == 0) {
        dem->writeBoundaryToFile();
        dem->writePatchGridToFile();
        dem->writeParticlesToFile(iterSnap);
        dem->printBoundaryContacts();
        sph->writeParticlesToFile(iterSnap);
        //sph->printSPHTecplot(sphTecplotInf, iterSnap);
        dem->appendToProgressOutputFile(demProgressInf);
      }
      dem->printContact(util::combine(outputFolder, "bursting_contact_", iterSnap, 3));
    
      timeCount = 0;
      ++iterSnap;
    }

    if (toCheckTime) {
      time1 = MPI_Wtime();
    }
    dem->migrateParticles(iteration);

    if (toCheckTime) {
      time2 = MPI_Wtime(); 
      migraT = time2 - time1; 
      totalT = time2 - time0;
    }

    if (dem->getMPIRank() == 0 && toCheckTime) {
      debugInf << std::setw(OWID) << iteration << std::setw(OWID) << commuT 
               << std::setw(OWID) << migraT 
               << std::setw(OWID) << totalT - commuT - migraT 
               << std::setw(OWID) << totalT 
               << std::setw(OWID) << (commuT + migraT)/totalT*100 << std::endl;
    }

    ++iteration;
  } 

  if (dem->getMPIRank() == 0) {
    dem->closeProgressOutputFile(demProgressInf);
  }
}

