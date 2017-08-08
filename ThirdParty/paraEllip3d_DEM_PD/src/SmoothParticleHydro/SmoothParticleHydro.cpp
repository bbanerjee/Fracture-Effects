#include <DiscreteElements/DEMParticle.h>
#include <SmoothParticleHydro/SmoothParticleHydro.h>

#include <Core/Const/const.h>
#include <Core/Math/IntVec.h>
#include <Core/Math/Matrix.h>
#include <Core/Util/Utility.h>
#include <InputOutput/OutputTecplot.h>
#include <InputOutput/OutputVTK.h>
#include <InputOutput/PeriParticleFileReader.h>
#include <chrono>

using namespace sph;

using Timer = std::chrono::steady_clock;
using Seconds = std::chrono::seconds;
using IntVec = dem::IntVec;
using Vec = dem::Vec;
using Matrix = dem::Matrix;
using Box = dem::Box;
using DEMParticlePArray = dem::DEMParticlePArray;
using OutputVTK = dem::OutputVTK<SPHParticlePArray>;
using OutputTecplot = dem::OutputTecplot<SPHParticlePArray>;

SmoothParticleHydro::SmoothParticleHydro()
{
}

SmoothParticleHydro::~SmoothParticleHydro()
{
  d_allSPHParticleVec.clear();
  d_sphParticleVec.clear();
}

/*
  // here the free particles and boundary particles will all stored in the same
vector while sph ghost particles will be stored in dem particles
  void SmoothParticleHydro::generateSPHParticleMiddleLayers3D(){  // this is for
drainage middleLayers

        if(mpiRank!=0) return;  // make only primary cpu generate particles

  int numLayers = util::getParam<int>("numLayers"];
   REAL L = allContainer.getMaxCorner().x()-allContainer.getMinCorner().x();  //
based on x direction
  REAL gamma = util::getParam<REAL>("gamma"];
  REAL P0 = util::getParam<REAL>("P0"];
  REAL gravAccel = util::getParam<REAL>("gravAccel"];
  REAL gravScale = util::getParam<REAL>("gravScale"];
  REAL sphInitialDensity = util::getParam<REAL>("SPHInitialDensity"];

    spaceInterval = util::getParam<REAL>("spaceInterval"];
  REAL SPHmass = sphInitialDensity*spaceInterval*spaceInterval*spaceInterval;

  // get the dimensions of the sph domain
      Vec vmin = allContainer.getMinCorner();
      Vec vmax = allContainer.getMaxCorner();
      REAL xmin = vmin.x();
      REAL ymin = vmin.y();
      REAL zmin = vmin.z();
      REAL xmax = vmax.x();
      REAL ymax = vmax.y();
      REAL zmax = vmax.z();

  REAL waterZmin = util::getParam<REAL>("waterZmin"];
  REAL waterZmax = util::getParam<REAL>("waterZmax"];

  // create and store PeriParticle objects into periParticleVec
  int isGhost = 0;  // is Ghost particle
  REAL radius_a, radius_b, radius_c;
    dem::Vec pt_position;
  dem::Vec localCoords;  // local position of ghost point in dem particle
  dem::Vec tmp_xyz, tmp_local;
  REAL small_value = 0.01*spaceInterval;
    for(REAL tmp_y=ymin-spaceInterval*numLayers;
tmp_y<ymax+spaceInterval*numLayers; tmp_y=tmp_y+spaceInterval){
  for(REAL tmp_x=xmin-spaceInterval*numLayers;
tmp_x<xmax+spaceInterval*numLayers; tmp_x=tmp_x+spaceInterval){
      for(REAL tmp_z=zmin-spaceInterval*numLayers;
tmp_z<zmax+spaceInterval*numLayers; tmp_z=tmp_z+spaceInterval){
    isGhost = 0;
    tmp_xyz = dem::Vec(tmp_x, tmp_y, tmp_z);
        for(std::vector<Particle*>::iterator pt=allParticleVec.begin();
pt!=allParticleVec.end(); pt++){
        radius_a = (*pt)->getA(); radius_b = (*pt)->getB(); radius_c =
(*pt)->getC();
        pt_position = (*pt)->currentPosition();
        tmp_local = (*pt)->globalToLocal(tmp_xyz-pt_position);
        if(
tmp_local.x()*tmp_local.x()/(radius_a*radius_a)+tmp_local.y()*tmp_local.y()/(radius_b*radius_b)+tmp_local.z()*tmp_local.z()/(radius_c*radius_c)
<= 1 ){
          isGhost = 1;  // is Ghost particle
          localCoords = (*pt)->globalToLocal( dem::Vec(tmp_x, tmp_y,
tmp_z)-pt_position );
          sph::SPHParticle* tmp_pt = new sph::SPHParticle(SPHmass,
sphInitialDensity, tmp_x, tmp_y, tmp_z, localCoords, 2);  // 2 is ghost SPH
particle
          (*pt)->SPHGhostParticleVec.push_back(tmp_pt);   // at current, the
scatterSPHParticle is not valide for ghost particles. July 15, 2015

          break;  // if this sph point is ghost for dem particle 1, then it
cannot be ghost for any others,
        // should avoid the initial overlap of the different dem particles
        }
        } // end dem particle

    if(isGhost==0){  // free/boundary particles
        if(tmp_x<=xmin-spaceInterval+small_value ||
tmp_x>=xmax+spaceInterval-small_value
        || tmp_y<=ymin-spaceInterval+small_value ||
tmp_y>=ymax+spaceInterval-small_value
        || tmp_z<=zmin-spaceInterval+small_value
        //|| tmp_z>=zmax+spaceInterval-small_value
        ){  // boundary sph particles, no top boundary
          sph::SPHParticle* tmp_pt = new sph::SPHParticle(SPHmass,
sphInitialDensity, tmp_x, tmp_y, tmp_z, 3);  // 3 is boundary SPH particle
      d_allSPHParticleVec.push_back(tmp_pt);
        }
        else if(tmp_z>=waterZmin && tmp_z<=waterZmax){  // free sph particles
          sph::SPHParticle* tmp_pt = new sph::SPHParticle(SPHmass,
sphInitialDensity*pow(1+sphInitialDensity*gravAccel*gravScale*(waterZmax-tmp_z)/P0,
1.0/gamma), tmp_x, tmp_y, tmp_z, 1);  // 1 is free SPH particle
//      sph::SPHParticle* tmp_pt = new sph::SPHParticle(SPHmass,
sphInitialDensity, tmp_x, 0, tmp_z, 1);  // 1 is free SPH particle
              d_allSPHParticleVec.push_back(tmp_pt);
        }
    }
      }
  }
    }

  for(std::vector<sph::SPHParticle*>::iterator pt=d_allSPHParticleVec.begin();
pt!=d_allSPHParticleVec.end(); pt++){
      (*pt)->initialize();
  }

  } // generateSPHParticleNoBottom3D

  void SmoothParticleHydro::findSPHParticleInRectangle(const Rectangle
&container,
        const std::vector<sph::SPHParticle*> &inputParticle,
        std::vector<sph::SPHParticle*> &foundParticle) {
    foundParticle.reserve(inputParticle.size());
    Vec  v1 = container.getMinCorner();
    Vec  v2 = container.getMaxCorner();
    REAL x1 = v1.x();
    REAL y1 = v1.y();
    REAL z1 = v1.z();
    REAL x2 = v2.x();
    REAL y2 = v2.y();
    REAL z2 = v2.z();
    for (std::size_t pt = 0; pt < inputParticle.size(); ++pt) {
      Vec center = inputParticle[pt]->getCurrPosition();
      // it is critical to use EPS
      if (center.x() - x1 >= -EPS && center.x() - x2 < -EPS &&
    center.y() - y1 >= -EPS && center.y() - y2 < -EPS &&
    center.z() - z1 >= -EPS && center.z() - z2 < -EPS)
  foundParticle.push_back(inputParticle[pt]);
    }
    std::vector<sph::SPHParticle*>(foundParticle).swap(foundParticle);
  }

  void SmoothParticleHydro::removeParticleOutRectangle() {
    Vec  v1 = container.getMinCorner();
    Vec  v2 = container.getMaxCorner();
    REAL x1 = v1.x();
    REAL y1 = v1.y();
    REAL z1 = v1.z();
    REAL x2 = v2.x();
    REAL y2 = v2.y();
    REAL z2 = v2.z();

    std::vector<Particle*>::iterator itr;
    Vec center;
    //std::size_t flag = 0;

    for (itr = particleVec.begin(); itr != particleVec.end(); ) {
      center=(*itr)->currentPosition();
      // it is critical to use EPS
      if ( !(center.x() - x1 >= -EPS && center.x() - x2 < -EPS &&
       center.y() - y1 >= -EPS && center.y() - y2 < -EPS &&
       center.z() - z1 >= -EPS && center.z() - z2 < -EPS) )
  {
    //debugInf << "iter=" << std::setw(8) << iteration << " rank=" <<
std::setw(2) << mpiRank
    //<< " removed=" << std::setw(3) << (*itr)->getId();
    //flag = 1;
    // this is important to free the memory of these sph ghost particles
    for(std::vector<sph::SPHParticle*>::iterator
st=(*itr)->SPHGhostParticleVec.begin(); st!=(*itr)->SPHGhostParticleVec.end();
st++){
       delete (*st);
    }
    (*itr)->SPHGhostParticleVec.clear();

    delete (*itr); // release memory
    itr = particleVec.erase(itr);
  }
      else
  ++itr;
    }
    //  if (flag == 1) {
    //  debugInf << " now " << particleVec.size() << ": ";
    //  for (std::vector<Particle*>::const_iterator it = particleVec.begin(); it
!= particleVec.end(); ++it)
    //  debugInf << std::setw(3) << (*it)->getId();
    //  debugInf << std::endl;
    //  }

  }

  void SmoothParticleHydro::removeSPHParticleOutRectangle() {
    Vec  v1 = container.getMinCorner();
    Vec  v2 = container.getMaxCorner();
    REAL x1 = v1.x();
    REAL y1 = v1.y();
    REAL z1 = v1.z();
    REAL x2 = v2.x();
    REAL y2 = v2.y();
    REAL z2 = v2.z();

    std::vector<sph::SPHParticle*>::iterator itr;
    Vec center;
    //std::size_t flag = 0;

    // this for loop may be replaced by for_each, remove, erase and swap (shrink
to fit)
    // see item 14 in "effective STL"
    for (itr = d_sphParticleVec.begin(); itr != d_sphParticleVec.end(); ) {
      center=(*itr)->getCurrPosition();
      // it is critical to use EPS
      if ( !(center.x() - x1 >= -EPS && center.x() - x2 < -EPS &&
       center.y() - y1 >= -EPS && center.y() - y2 < -EPS &&
       center.z() - z1 >= -EPS && center.z() - z2 < -EPS) )
  {
    //  debugInf << "iter=" << std::setw(8) << iteration << " rank=" <<
std::setw(2) << mpiRank
    //  << " removed=" << std::setw(3) << (*itr)->getId();
    //  flag = 1;
    delete (*itr); // release memory
    itr = d_sphParticleVec.erase(itr);
  }
      else
  ++itr;
    }
    //  if (flag == 1) {
    //  debugInf << " now " << particleVec.size() << ": ";
    //  for (std::vector<Particle*>::const_iterator it = particleVec.begin(); it
!= particleVec.end(); ++it)
    //  debugInf << std::setw(3) << (*it)->getId();
    //  debugInf << std::endl;
    //  }

  }

  // this is to scatter the dem and sph particle
  // two point: (1) the sph ghost particles will be partitioned with dem
particles together. There is no explicite partition for sph ghost particles.
  //        After the partition of the dem particles, the sph ghost particles
will be paritioned as a class member of dem particle.
  //        Before partition, SPHParticle.demParticle points to NULL; after
receive, SPHParticle.demParticle can point to their dem particle. July 27, 2015
  //            (2) block partitions for dem domain and sph domain have to be
the same,
  //        otherwise some dem particles and their nearby sph particles will
belong to different block
  //        (grid is expanded to include the boundary sph particles)
  //    (3) this partition method here is not very suitable for the free surface
flow problem, such as bursting dam problem
  //        since the total domain is divided, while there are lots of voids in
the domain. (partition only free sph particles will be better)
  //        But our goal is to simulate the porous media in triaxial, insotropic
or SHPB simulations, particles are filled in the container.
  void SmoothParticleHydro::scatterDEMSPHParticle() {
    // partition particles and send to each process
    if (mpiRank == 0) { // process 0
      int numLayers = util::getParam<int>("numLayers"];
      setGrid(Rectangle(allContainer.getMinCorner().x() -
spaceInterval*numLayers,
      allContainer.getMinCorner().y() - spaceInterval*numLayers,
      allContainer.getMinCorner().z() - spaceInterval*numLayers,
      allContainer.getMaxCorner().x() + spaceInterval*numLayers,
      allContainer.getMaxCorner().y() + spaceInterval*numLayers,
      allContainer.getMaxCorner().z() + spaceInterval*numLayers ));

      Vec v1 = grid.getMinCorner();
      Vec v2 = grid.getMaxCorner();
      Vec vspan = v2 - v1;

      boost::mpi::request *reqs = new boost::mpi::request [mpiSize - 1];
      std::vector<Particle*> tmpParticleVec;
      for (int iRank = mpiSize - 1; iRank >= 0; --iRank) {
  tmpParticleVec.clear(); // do not release memory!
  int ndim = 3;
  int coords[3];
  MPI_Cart_coords(cartComm, iRank, ndim, coords);
  Rectangle container(v1.x() + vspan.x() / mpiProcX * coords[0],
          v1.y() + vspan.y() / mpiProcY * coords[1],
          v1.z() + vspan.z() / mpiProcZ * coords[2],
          v1.x() + vspan.x() / mpiProcX * (coords[0] + 1),
          v1.y() + vspan.y() / mpiProcY * (coords[1] + 1),
          v1.z() + vspan.z() / mpiProcZ * (coords[2] + 1));
  findParticleInRectangle(container, allParticleVec, tmpParticleVec);
  if (iRank != 0)
    reqs[iRank - 1] = boostWorld.isend(iRank, mpiTag, tmpParticleVec); //
non-blocking send
    // before send, the SPHParticle.demParticle == NULL, since NULL is assigned
when SPHParticle is created
  if (iRank == 0) {
    particleVec.resize(tmpParticleVec.size());
    for (int i = 0; i < particleVec.size(); ++i){
      // default synthesized copy constructor
      particleVec[i] = new Particle(*tmpParticleVec[i]);  // at this point,
particleVec and allParticleVec are pointing to the same SPHGhoastParticle
      particleVec[i]->SPHGhostParticleVec.clear();  // at this point,
particleVec is pointing to nothing
      for(std::vector<sph::SPHParticle*>::iterator
st=tmpParticleVec[i]->SPHGhostParticleVec.begin();
st!=tmpParticleVec[i]->SPHGhostParticleVec.end(); st++){
      sph::SPHParticle* tmp_sph = new sph::SPHParticle(**st);  // create a new
SPHGhost particle, which is the same as the one in allParticleVec
      particleVec[i]->SPHGhostParticleVec.push_back(tmp_sph);  // now
particleVec points to the new SPHGhostParticle
      }
    }
  } // now particleVec do not share memeory with allParticleVec
      }
      boost::mpi::wait_all(reqs, reqs + mpiSize - 1); // for non-blocking send
      delete [] reqs;

    } else { // other processes except 0
      boostWorld.recv(0, mpiTag, particleVec);
    }

    // this is important to pointer the demParticle in SPHParticle to the
correct dem particle
    for(std::vector<Particle*>::iterator it=particleVec.begin();
it!=particleVec.end(); it++){
  (*it)->setDemParticleInSPHParticle();  // set SPHParticle.demParticle to (*it)
    }

    // content of allParticleVec may need to be printed, so do not clear it.
    //if (mpiRank == 0) releaseGatheredParticle();


    ///////////////////////////////////////////////////////////////////////////////
    // partition SPH particles (free and boundary) and send to each process
    if (mpiRank == 0) { // process 0
      int numLayers = util::getParam<int>("numLayers"];
      setGrid(Rectangle(allContainer.getMinCorner().x() -
spaceInterval*numLayers,
      allContainer.getMinCorner().y() - spaceInterval*numLayers,
      allContainer.getMinCorner().z() - spaceInterval*numLayers,
      allContainer.getMaxCorner().x() + spaceInterval*numLayers,
      allContainer.getMaxCorner().y() + spaceInterval*numLayers,
      allContainer.getMaxCorner().z() + spaceInterval*numLayers ));

      Vec v1 = grid.getMinCorner();
      Vec v2 = grid.getMaxCorner();
      Vec vspan = v2 - v1;

      boost::mpi::request *reqs = new boost::mpi::request [mpiSize - 1];
      std::vector<sph::SPHParticle*> tmpSPHParticleVec;
      for (int iRank = mpiSize - 1; iRank >= 0; --iRank) {
  tmpSPHParticleVec.clear(); // do not release memory!
  int ndim = 3;
  int coords[3];
  MPI_Cart_coords(cartComm, iRank, ndim, coords);
  Rectangle container(v1.x() + vspan.x() / mpiProcX * coords[0],
          v1.y() + vspan.y() / mpiProcY * coords[1],
          v1.z() + vspan.z() / mpiProcZ * coords[2],
          v1.x() + vspan.x() / mpiProcX * (coords[0] + 1),
          v1.y() + vspan.y() / mpiProcY * (coords[1] + 1),
          v1.z() + vspan.z() / mpiProcZ * (coords[2] + 1));
  findSPHParticleInRectangle(container, d_allSPHParticleVec, tmpSPHParticleVec);
  if (iRank != 0)
    reqs[iRank - 1] = boostWorld.isend(iRank, mpiTag, tmpSPHParticleVec); //
non-blocking send
  if (iRank == 0) {
    d_sphParticleVec.resize(tmpSPHParticleVec.size());
    for (int i = 0; i < d_sphParticleVec.size(); ++i)
      d_sphParticleVec[i] = new sph::SPHParticle(*tmpSPHParticleVec[i]); //
default synthesized copy constructor
  } // now particleVec do not share memeory with allParticleVec
      }
      boost::mpi::wait_all(reqs, reqs + mpiSize - 1); // for non-blocking send
      delete [] reqs;

    } else { // other processes except 0
      boostWorld.recv(0, mpiTag, d_sphParticleVec);
    }


    // broadcast necessary info
    broadcast(boostWorld, gradation, 0);
    broadcast(boostWorld, boundaryVec, 0);
    broadcast(boostWorld, allContainer, 0);
    broadcast(boostWorld, grid, 0);
  } // scatterDEMSPHParticle


  void SmoothParticleHydro::scatterDEMSPHParticleCopyDEM() {
    // partition particles and send to each process
    if (mpiRank == 0) { // process 0
      int numLayers = util::getParam<int>("numLayers"];
      setGrid(Rectangle(allContainer.getMinCorner().x() -
spaceInterval*numLayers,
      allContainer.getMinCorner().y() - spaceInterval*numLayers,
      allContainer.getMinCorner().z() - spaceInterval*numLayers,
      allContainer.getMaxCorner().x() + spaceInterval*numLayers,
      allContainer.getMaxCorner().y() + spaceInterval*numLayers,
      allContainer.getMaxCorner().z() + spaceInterval*numLayers ));

      Vec v1 = grid.getMinCorner();
      Vec v2 = grid.getMaxCorner();
      Vec vspan = v2 - v1;

      boost::mpi::request *reqs = new boost::mpi::request [mpiSize - 1];
      std::vector<Particle*> tmpParticleVec;
      for (int iRank = mpiSize - 1; iRank >= 0; --iRank) {
  tmpParticleVec.clear(); // do not release memory!
  int ndim = 3;
  int coords[3];
  MPI_Cart_coords(cartComm, iRank, ndim, coords);
  Rectangle container(v1.x(),
          v1.y(),
          v1.z(),
          v2.x(),
          v2.y(),
          v2.z() );
  findParticleInRectangle(container, allParticleVec, tmpParticleVec);
  if (iRank != 0)
    reqs[iRank - 1] = boostWorld.isend(iRank, mpiTag, tmpParticleVec); //
non-blocking send
    // before send, the SPHParticle.demParticle == NULL, since NULL is assigned
when SPHParticle is created
  if (iRank == 0) {
    particleVec.resize(tmpParticleVec.size());
    for (int i = 0; i < particleVec.size(); ++i){
      // default synthesized copy constructor
      particleVec[i] = new Particle(*tmpParticleVec[i]);  // at this point,
particleVec and allParticleVec are pointing to the same SPHGhoastParticle
      particleVec[i]->SPHGhostParticleVec.clear();  // at this point,
particleVec is pointing to nothing
      for(std::vector<sph::SPHParticle*>::iterator
st=tmpParticleVec[i]->SPHGhostParticleVec.begin();
st!=tmpParticleVec[i]->SPHGhostParticleVec.end(); st++){
      sph::SPHParticle* tmp_sph = new sph::SPHParticle(**st);  // create a new
SPHGhost particle, which is the same as the one in allParticleVec
      particleVec[i]->SPHGhostParticleVec.push_back(tmp_sph);  // now
particleVec points to the new SPHGhostParticle
      }
    }
  } // now particleVec do not share memeory with allParticleVec
      }
      boost::mpi::wait_all(reqs, reqs + mpiSize - 1); // for non-blocking send
      delete [] reqs;

    } else { // other processes except 0
      boostWorld.recv(0, mpiTag, particleVec);
    }

    // this is important to pointer the demParticle in SPHParticle to the
correct dem particle
    for(std::vector<Particle*>::iterator it=particleVec.begin();
it!=particleVec.end(); it++){
  (*it)->setDemParticleInSPHParticle();  // set SPHParticle.demParticle to (*it)
    }

    // content of allParticleVec may need to be printed, so do not clear it.
    //if (mpiRank == 0) releaseGatheredParticle();


    ///////////////////////////////////////////////////////////////////////////////
    // partition SPH particles (free and boundary) and send to each process
    if (mpiRank == 0) { // process 0
      int numLayers = util::getParam<int>("numLayers"];
      setGrid(Rectangle(allContainer.getMinCorner().x() -
spaceInterval*numLayers,
      allContainer.getMinCorner().y() - spaceInterval*numLayers,
      allContainer.getMinCorner().z() - spaceInterval*numLayers,
      allContainer.getMaxCorner().x() + spaceInterval*numLayers,
      allContainer.getMaxCorner().y() + spaceInterval*numLayers,
      allContainer.getMaxCorner().z() + spaceInterval*numLayers ));

      Vec v1 = grid.getMinCorner();
      Vec v2 = grid.getMaxCorner();
      Vec vspan = v2 - v1;

      boost::mpi::request *reqs = new boost::mpi::request [mpiSize - 1];
      std::vector<sph::SPHParticle*> tmpSPHParticleVec;
      for (int iRank = mpiSize - 1; iRank >= 0; --iRank) {
  tmpSPHParticleVec.clear(); // do not release memory!
  int ndim = 3;
  int coords[3];
  MPI_Cart_coords(cartComm, iRank, ndim, coords);
  Rectangle container(v1.x() + vspan.x() / mpiProcX * coords[0],
          v1.y() + vspan.y() / mpiProcY * coords[1],
          v1.z() + vspan.z() / mpiProcZ * coords[2],
          v1.x() + vspan.x() / mpiProcX * (coords[0] + 1),
          v1.y() + vspan.y() / mpiProcY * (coords[1] + 1),
          v1.z() + vspan.z() / mpiProcZ * (coords[2] + 1));
  findSPHParticleInRectangle(container, d_allSPHParticleVec, tmpSPHParticleVec);
  if (iRank != 0)
    reqs[iRank - 1] = boostWorld.isend(iRank, mpiTag, tmpSPHParticleVec); //
non-blocking send
  if (iRank == 0) {
    d_sphParticleVec.resize(tmpSPHParticleVec.size());
    for (int i = 0; i < d_sphParticleVec.size(); ++i)
      d_sphParticleVec[i] = new sph::SPHParticle(*tmpSPHParticleVec[i]); //
default synthesized copy constructor
  } // now particleVec do not share memeory with allParticleVec
      }
      boost::mpi::wait_all(reqs, reqs + mpiSize - 1); // for non-blocking send
      delete [] reqs;

    } else { // other processes except 0
      boostWorld.recv(0, mpiTag, d_sphParticleVec);
    }


    // broadcast necessary info
    broadcast(boostWorld, gradation, 0);
    broadcast(boostWorld, boundaryVec, 0);
    broadcast(boostWorld, allContainer, 0);
    broadcast(boostWorld, grid, 0);
  } // scatterDEMSPHParticleCopyDEM

  void SmoothParticleHydro::commuParticle()   // the communication of sph ghost
particles are implemented here, July 27, 2015
  {
    // determine container of each process
    Vec v1 = grid.getMinCorner();
    Vec v2 = grid.getMaxCorner();
    Vec vspan = v2 - v1;
    container = Rectangle(v1.x() + vspan.x() / mpiProcX * mpiCoords[0],
        v1.y() + vspan.y() / mpiProcY * mpiCoords[1],
        v1.z() + vspan.z() / mpiProcZ * mpiCoords[2],
        v1.x() + vspan.x() / mpiProcX * (mpiCoords[0] + 1),
        v1.y() + vspan.y() / mpiProcY * (mpiCoords[1] + 1),
        v1.z() + vspan.z() / mpiProcZ * (mpiCoords[2] + 1));

    // if found, communicate with neighboring blocks
    std::vector<Particle*> particleX1, particleX2;
    std::vector<Particle*> particleY1, particleY2;
    std::vector<Particle*> particleZ1, particleZ2;
    std::vector<Particle*> particleX1Y1, particleX1Y2, particleX1Z1,
particleX1Z2;
    std::vector<Particle*> particleX2Y1, particleX2Y2, particleX2Z1,
particleX2Z2;
    std::vector<Particle*> particleY1Z1, particleY1Z2, particleY2Z1,
particleY2Z2;
    std::vector<Particle*> particleX1Y1Z1, particleX1Y1Z2, particleX1Y2Z1,
particleX1Y2Z2;
    std::vector<Particle*> particleX2Y1Z1, particleX2Y1Z2, particleX2Y2Z1,
particleX2Y2Z2;
    boost::mpi::request reqX1[2], reqX2[2];
    boost::mpi::request reqY1[2], reqY2[2];
    boost::mpi::request reqZ1[2], reqZ2[2];
    boost::mpi::request reqX1Y1[2], reqX1Y2[2], reqX1Z1[2], reqX1Z2[2];
    boost::mpi::request reqX2Y1[2], reqX2Y2[2], reqX2Z1[2], reqX2Z2[2];
    boost::mpi::request reqY1Z1[2], reqY1Z2[2], reqY2Z1[2], reqY2Z2[2];
    boost::mpi::request reqX1Y1Z1[2], reqX1Y1Z2[2], reqX1Y2Z1[2], reqX1Y2Z2[2];
    boost::mpi::request reqX2Y1Z1[2], reqX2Y1Z2[2], reqX2Y2Z1[2], reqX2Y2Z2[2];
    v1 = container.getMinCorner(); // redefine v1, v2 in terms of process
    v2 = container.getMaxCorner();
    //debugInf << "rank=" << mpiRank << ' ' << v1.x() << ' ' << v1.y() << ' ' <<
v1.z() << ' '  << v2.x() << ' ' << v2.y() << ' ' << v2.z() << std::endl;
    REAL cellSize = gradation.getPtclMaxRadius() * 2;
    // 6 surfaces
    if (rankX1 >= 0) { // surface x1
      Rectangle containerX1(v1.x(), v1.y(), v1.z(),
          v1.x() + cellSize, v2.y(), v2.z());
      findParticleInRectangle(containerX1, particleVec, particleX1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1.begin();
it!=particleX1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();  // now the SPHGhostParticles in
part of particleVec are pointing to NULL, needed to be back after send
      reqX1[0] = boostWorld.isend(rankX1, mpiTag,  particleX1);
      reqX1[1] = boostWorld.irecv(rankX1, mpiTag, rParticleX1);
    }
    if (rankX2 >= 0) { // surface x2
      Rectangle containerX2(v2.x() - cellSize, v1.y(), v1.z(),
          v2.x(), v2.y(), v2.z());
      findParticleInRectangle(containerX2, particleVec, particleX2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2.begin();
it!=particleX2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2[0] = boostWorld.isend(rankX2, mpiTag,  particleX2);
      reqX2[1] = boostWorld.irecv(rankX2, mpiTag, rParticleX2);
    }
    if (rankY1 >= 0) {  // surface y1
      Rectangle containerY1(v1.x(), v1.y(), v1.z(),
          v2.x(), v1.y() + cellSize, v2.z());
      findParticleInRectangle(containerY1, particleVec, particleY1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY1.begin();
it!=particleY1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY1[0] = boostWorld.isend(rankY1, mpiTag,  particleY1);
      reqY1[1] = boostWorld.irecv(rankY1, mpiTag, rParticleY1);
    }
    if (rankY2 >= 0) {  // surface y2
      Rectangle containerY2(v1.x(), v2.y() - cellSize, v1.z(),
          v2.x(), v2.y(), v2.z());
      findParticleInRectangle(containerY2, particleVec, particleY2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY2.begin();
it!=particleY2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY2[0] = boostWorld.isend(rankY2, mpiTag,  particleY2);
      reqY2[1] = boostWorld.irecv(rankY2, mpiTag, rParticleY2);
    }
    if (rankZ1 >= 0) {  // surface z1
      Rectangle containerZ1(v1.x(), v1.y(), v1.z(),
          v2.x(), v2.y(), v1.z() + cellSize);
      findParticleInRectangle(containerZ1, particleVec, particleZ1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleZ1.begin();
it!=particleZ1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqZ1[0] = boostWorld.isend(rankZ1, mpiTag,  particleZ1);
      reqZ1[1] = boostWorld.irecv(rankZ1, mpiTag, rParticleZ1);
    }
    if (rankZ2 >= 0) {  // surface z2
      Rectangle containerZ2(v1.x(), v1.y(), v2.z() - cellSize,
          v2.x(), v2.y(), v2.z());
      findParticleInRectangle(containerZ2, particleVec, particleZ2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleZ2.begin();
it!=particleZ2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqZ2[0] = boostWorld.isend(rankZ2, mpiTag,  particleZ2);
      reqZ2[1] = boostWorld.irecv(rankZ2, mpiTag, rParticleZ2);
    }
    // 12 edges
    if (rankX1Y1 >= 0) { // edge x1y1
      Rectangle containerX1Y1(v1.x(), v1.y(), v1.z(),
            v1.x() + cellSize, v1.y() + cellSize, v2.z());
      findParticleInRectangle(containerX1Y1, particleVec, particleX1Y1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y1.begin();
it!=particleX1Y1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y1[0] = boostWorld.isend(rankX1Y1, mpiTag,  particleX1Y1);
      reqX1Y1[1] = boostWorld.irecv(rankX1Y1, mpiTag, rParticleX1Y1);
    }
    if (rankX1Y2 >= 0) { // edge x1y2
      Rectangle containerX1Y2(v1.x(), v2.y() - cellSize, v1.z(),
            v1.x() + cellSize, v2.y(), v2.z());
      findParticleInRectangle(containerX1Y2, particleVec, particleX1Y2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y2.begin();
it!=particleX1Y2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y2[0] = boostWorld.isend(rankX1Y2, mpiTag,  particleX1Y2);
      reqX1Y2[1] = boostWorld.irecv(rankX1Y2, mpiTag, rParticleX1Y2);
    }
    if (rankX1Z1 >= 0) { // edge x1z1
      Rectangle containerX1Z1(v1.x(), v1.y(), v1.z(),
            v1.x() + cellSize, v2.y(), v1.z() + cellSize);
      findParticleInRectangle(containerX1Z1, particleVec, particleX1Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Z1.begin();
it!=particleX1Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Z1[0] = boostWorld.isend(rankX1Z1, mpiTag,  particleX1Z1);
      reqX1Z1[1] = boostWorld.irecv(rankX1Z1, mpiTag, rParticleX1Z1);
    }
    if (rankX1Z2 >= 0) { // edge x1z2
      Rectangle containerX1Z2(v1.x(), v1.y(), v2.z() - cellSize,
            v1.x() + cellSize, v2.y(), v2.z());
      findParticleInRectangle(containerX1Z2, particleVec, particleX1Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Z2.begin();
it!=particleX1Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Z2[0] = boostWorld.isend(rankX1Z2, mpiTag,  particleX1Z2);
      reqX1Z2[1] = boostWorld.irecv(rankX1Z2, mpiTag, rParticleX1Z2);
    }
    if (rankX2Y1 >= 0) { // edge x2y1
      Rectangle containerX2Y1(v2.x() - cellSize, v1.y(), v1.z(),
            v2.x(), v1.y() + cellSize, v2.z());
      findParticleInRectangle(containerX2Y1, particleVec, particleX2Y1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y1.begin();
it!=particleX2Y1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y1[0] = boostWorld.isend(rankX2Y1, mpiTag,  particleX2Y1);
      reqX2Y1[1] = boostWorld.irecv(rankX2Y1, mpiTag, rParticleX2Y1);
    }
    if (rankX2Y2 >= 0) { // edge x2y2
      Rectangle containerX2Y2(v2.x() - cellSize, v2.y() - cellSize, v1.z(),
            v2.x(), v2.y(), v2.z());
      findParticleInRectangle(containerX2Y2, particleVec, particleX2Y2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y2.begin();
it!=particleX2Y2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y2[0] = boostWorld.isend(rankX2Y2, mpiTag,  particleX2Y2);
      reqX2Y2[1] = boostWorld.irecv(rankX2Y2, mpiTag, rParticleX2Y2);
    }
    if (rankX2Z1 >= 0) { // edge x2z1
      Rectangle containerX2Z1(v2.x() - cellSize, v1.y(), v1.z(),
            v2.x(), v2.y(), v1.z() + cellSize);
      findParticleInRectangle(containerX2Z1, particleVec, particleX2Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Z1.begin();
it!=particleX2Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Z1[0] = boostWorld.isend(rankX2Z1, mpiTag,  particleX2Z1);
      reqX2Z1[1] = boostWorld.irecv(rankX2Z1, mpiTag, rParticleX2Z1);
    }
    if (rankX2Z2 >= 0) { // edge x2z2
      Rectangle containerX2Z2(v2.x() - cellSize, v1.y(), v2.z() - cellSize,
            v2.x(), v2.y(), v2.z());
      findParticleInRectangle(containerX2Z2, particleVec, particleX2Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Z2.begin();
it!=particleX2Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Z2[0] = boostWorld.isend(rankX2Z2, mpiTag,  particleX2Z2);
      reqX2Z2[1] = boostWorld.irecv(rankX2Z2, mpiTag, rParticleX2Z2);
    }
    if (rankY1Z1 >= 0) { // edge y1z1
      Rectangle containerY1Z1(v1.x(), v1.y(), v1.z(),
            v2.x(), v1.y() + cellSize, v1.z() + cellSize);
      findParticleInRectangle(containerY1Z1, particleVec, particleY1Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY1Z1.begin();
it!=particleY1Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY1Z1[0] = boostWorld.isend(rankY1Z1, mpiTag,  particleY1Z1);
      reqY1Z1[1] = boostWorld.irecv(rankY1Z1, mpiTag, rParticleY1Z1);
    }
    if (rankY1Z2 >= 0) { // edge y1z2
      Rectangle containerY1Z2(v1.x(), v1.y(), v2.z() - cellSize,
            v2.x(), v1.y() + cellSize, v2.z());
      findParticleInRectangle(containerY1Z2, particleVec, particleY1Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY1Z2.begin();
it!=particleY1Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY1Z2[0] = boostWorld.isend(rankY1Z2, mpiTag,  particleY1Z2);
      reqY1Z2[1] = boostWorld.irecv(rankY1Z2, mpiTag, rParticleY1Z2);
    }
    if (rankY2Z1 >= 0) { // edge y2z1
      Rectangle containerY2Z1(v1.x(), v2.y() - cellSize, v1.z(),
            v2.x(), v2.y(), v1.z() + cellSize);
      findParticleInRectangle(containerY2Z1, particleVec, particleY2Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY2Z1.begin();
it!=particleY2Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY2Z1[0] = boostWorld.isend(rankY2Z1, mpiTag,  particleY2Z1);
      reqY2Z1[1] = boostWorld.irecv(rankY2Z1, mpiTag, rParticleY2Z1);
    }
    if (rankY2Z2 >= 0) { // edge y2z2
      Rectangle containerY2Z2(v1.x(), v2.y() - cellSize, v2.z() - cellSize,
            v2.x(), v2.y(), v2.z());
      findParticleInRectangle(containerY2Z2, particleVec, particleY2Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY2Z2.begin();
it!=particleY2Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY2Z2[0] = boostWorld.isend(rankY2Z2, mpiTag,  particleY2Z2);
      reqY2Z2[1] = boostWorld.irecv(rankY2Z2, mpiTag, rParticleY2Z2);
    }
    // 8 vertices
    if (rankX1Y1Z1 >= 0) { // edge x1y1z1
      Rectangle containerX1Y1Z1(v1.x(), v1.y(), v1.z(),
        v1.x() + cellSize, v1.y() + cellSize, v1.z() + cellSize);
      findParticleInRectangle(containerX1Y1Z1, particleVec, particleX1Y1Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y1Z1.begin();
it!=particleX1Y1Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y1Z1[0] = boostWorld.isend(rankX1Y1Z1, mpiTag,  particleX1Y1Z1);
      reqX1Y1Z1[1] = boostWorld.irecv(rankX1Y1Z1, mpiTag, rParticleX1Y1Z1);
    }
    if (rankX1Y1Z2 >= 0) { // edge x1y1z2
      Rectangle containerX1Y1Z2(v1.x(), v1.y(), v2.z() - cellSize,
        v1.x() + cellSize, v1.y() + cellSize, v2.z());
      findParticleInRectangle(containerX1Y1Z2, particleVec, particleX1Y1Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y1Z2.begin();
it!=particleX1Y1Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y1Z2[0] = boostWorld.isend(rankX1Y1Z2, mpiTag,  particleX1Y1Z2);
      reqX1Y1Z2[1] = boostWorld.irecv(rankX1Y1Z2, mpiTag, rParticleX1Y1Z2);
    }
    if (rankX1Y2Z1 >= 0) { // edge x1y2z1
      Rectangle containerX1Y2Z1(v1.x(), v2.y() - cellSize, v1.z(),
        v1.x() + cellSize, v2.y(), v1.z() + cellSize);
      findParticleInRectangle(containerX1Y2Z1, particleVec, particleX1Y2Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y2Z1.begin();
it!=particleX1Y2Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y2Z1[0] = boostWorld.isend(rankX1Y2Z1, mpiTag,  particleX1Y2Z1);
      reqX1Y2Z1[1] = boostWorld.irecv(rankX1Y2Z1, mpiTag, rParticleX1Y2Z1);
    }
    if (rankX1Y2Z2 >= 0) { // edge x1y2z2
      Rectangle containerX1Y2Z2(v1.x(), v2.y() - cellSize, v2.z() - cellSize,
        v1.x() + cellSize, v2.y() + cellSize, v2.z());
      findParticleInRectangle(containerX1Y2Z2, particleVec, particleX1Y2Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y2Z2.begin();
it!=particleX1Y2Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y2Z2[0] = boostWorld.isend(rankX1Y2Z2, mpiTag,  particleX1Y2Z2);
      reqX1Y2Z2[1] = boostWorld.irecv(rankX1Y2Z2, mpiTag, rParticleX1Y2Z2);
    }
    if (rankX2Y1Z1 >= 0) { // edge x2y1z1
      Rectangle containerX2Y1Z1(v2.x() - cellSize, v1.y(), v1.z(),
        v2.x(), v1.y() + cellSize, v1.z() + cellSize);
      findParticleInRectangle(containerX2Y1Z1, particleVec, particleX2Y1Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y1Z1.begin();
it!=particleX2Y1Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y1Z1[0] = boostWorld.isend(rankX2Y1Z1, mpiTag,  particleX2Y1Z1);
      reqX2Y1Z1[1] = boostWorld.irecv(rankX2Y1Z1, mpiTag, rParticleX2Y1Z1);
    }
    if (rankX2Y1Z2 >= 0) { // edge x2y1z2
      Rectangle containerX2Y1Z2(v2.x() - cellSize, v1.y(), v2.z() - cellSize,
        v2.x(), v1.y() + cellSize, v2.z());
      findParticleInRectangle(containerX2Y1Z2, particleVec, particleX2Y1Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y1Z2.begin();
it!=particleX2Y1Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y1Z2[0] = boostWorld.isend(rankX2Y1Z2, mpiTag,  particleX2Y1Z2);
      reqX2Y1Z2[1] = boostWorld.irecv(rankX2Y1Z2, mpiTag, rParticleX2Y1Z2);
    }
    if (rankX2Y2Z1 >= 0) { // edge x2y2z1
      Rectangle containerX2Y2Z1(v2.x() - cellSize, v2.y() - cellSize, v1.z(),
        v2.x(), v2.y(), v1.z() + cellSize);
      findParticleInRectangle(containerX2Y2Z1, particleVec, particleX2Y2Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y2Z1.begin();
it!=particleX2Y2Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y2Z1[0] = boostWorld.isend(rankX2Y2Z1, mpiTag,  particleX2Y2Z1);
      reqX2Y2Z1[1] = boostWorld.irecv(rankX2Y2Z1, mpiTag, rParticleX2Y2Z1);
    }
    if (rankX2Y2Z2 >= 0) { // edge x2y2z2
      Rectangle containerX2Y2Z2(v2.x() - cellSize, v2.y() - cellSize, v2.z() -
cellSize,
        v2.x(), v2.y(), v2.z());
      findParticleInRectangle(containerX2Y2Z2, particleVec, particleX2Y2Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y2Z2.begin();
it!=particleX2Y2Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y2Z2[0] = boostWorld.isend(rankX2Y2Z2, mpiTag,  particleX2Y2Z2);
      reqX2Y2Z2[1] = boostWorld.irecv(rankX2Y2Z2, mpiTag, rParticleX2Y2Z2);
    }

    // 6 surfaces
    if (rankX1 >= 0) boost::mpi::wait_all(reqX1, reqX1 + 2);
    if (rankX2 >= 0) boost::mpi::wait_all(reqX2, reqX2 + 2);
    if (rankY1 >= 0) boost::mpi::wait_all(reqY1, reqY1 + 2);
    if (rankY2 >= 0) boost::mpi::wait_all(reqY2, reqY2 + 2);
    if (rankZ1 >= 0) boost::mpi::wait_all(reqZ1, reqZ1 + 2);
    if (rankZ2 >= 0) boost::mpi::wait_all(reqZ2, reqZ2 + 2);
    // 12 edges
    if (rankX1Y1 >= 0) boost::mpi::wait_all(reqX1Y1, reqX1Y1 + 2);
    if (rankX1Y2 >= 0) boost::mpi::wait_all(reqX1Y2, reqX1Y2 + 2);
    if (rankX1Z1 >= 0) boost::mpi::wait_all(reqX1Z1, reqX1Z1 + 2);
    if (rankX1Z2 >= 0) boost::mpi::wait_all(reqX1Z2, reqX1Z2 + 2);
    if (rankX2Y1 >= 0) boost::mpi::wait_all(reqX2Y1, reqX2Y1 + 2);
    if (rankX2Y2 >= 0) boost::mpi::wait_all(reqX2Y2, reqX2Y2 + 2);
    if (rankX2Z1 >= 0) boost::mpi::wait_all(reqX2Z1, reqX2Z1 + 2);
    if (rankX2Z2 >= 0) boost::mpi::wait_all(reqX2Z2, reqX2Z2 + 2);
    if (rankY1Z1 >= 0) boost::mpi::wait_all(reqY1Z1, reqY1Z1 + 2);
    if (rankY1Z2 >= 0) boost::mpi::wait_all(reqY1Z2, reqY1Z2 + 2);
    if (rankY2Z1 >= 0) boost::mpi::wait_all(reqY2Z1, reqY2Z1 + 2);
    if (rankY2Z2 >= 0) boost::mpi::wait_all(reqY2Z2, reqY2Z2 + 2);
    // 8 vertices
    if (rankX1Y1Z1 >= 0) boost::mpi::wait_all(reqX1Y1Z1, reqX1Y1Z1 + 2);
    if (rankX1Y1Z2 >= 0) boost::mpi::wait_all(reqX1Y1Z2, reqX1Y1Z2 + 2);
    if (rankX1Y2Z1 >= 0) boost::mpi::wait_all(reqX1Y2Z1, reqX1Y2Z1 + 2);
    if (rankX1Y2Z2 >= 0) boost::mpi::wait_all(reqX1Y2Z2, reqX1Y2Z2 + 2);
    if (rankX2Y1Z1 >= 0) boost::mpi::wait_all(reqX2Y1Z1, reqX2Y1Z1 + 2);
    if (rankX2Y1Z2 >= 0) boost::mpi::wait_all(reqX2Y1Z2, reqX2Y1Z2 + 2);
    if (rankX2Y2Z1 >= 0) boost::mpi::wait_all(reqX2Y2Z1, reqX2Y2Z1 + 2);
    if (rankX2Y2Z2 >= 0) boost::mpi::wait_all(reqX2Y2Z2, reqX2Y2Z2 + 2);

    // merge: particles inside container (at front) + particles from neighoring
blocks (at end)
    recvParticleVec.clear();
    // 6 surfaces
    if (rankX1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1.begin(), rParticleX1.end());
    if (rankX2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2.begin(), rParticleX2.end());
    if (rankY1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY1.begin(), rParticleY1.end());
    if (rankY2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY2.begin(), rParticleY2.end());
    if (rankZ1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleZ1.begin(), rParticleZ1.end());
    if (rankZ2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleZ2.begin(), rParticleZ2.end());
    // 12 edges
    if (rankX1Y1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y1.begin(), rParticleX1Y1.end());
    if (rankX1Y2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y2.begin(), rParticleX1Y2.end());
    if (rankX1Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Z1.begin(), rParticleX1Z1.end());
    if (rankX1Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Z2.begin(), rParticleX1Z2.end());
    if (rankX2Y1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y1.begin(), rParticleX2Y1.end());
    if (rankX2Y2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y2.begin(), rParticleX2Y2.end());
    if (rankX2Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Z1.begin(), rParticleX2Z1.end());
    if (rankX2Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Z2.begin(), rParticleX2Z2.end());
    if (rankY1Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY1Z1.begin(), rParticleY1Z1.end());
    if (rankY1Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY1Z2.begin(), rParticleY1Z2.end());
    if (rankY2Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY2Z1.begin(), rParticleY2Z1.end());
    if (rankY2Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY2Z2.begin(), rParticleY2Z2.end());
    // 8 vertices
    if (rankX1Y1Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y1Z1.begin(), rParticleX1Y1Z1.end());
    if (rankX1Y1Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y1Z2.begin(), rParticleX1Y1Z2.end());
    if (rankX1Y2Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y2Z1.begin(), rParticleX1Y2Z1.end());
    if (rankX1Y2Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y2Z2.begin(), rParticleX1Y2Z2.end());
    if (rankX2Y1Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y1Z1.begin(), rParticleX2Y1Z1.end());
    if (rankX2Y1Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y1Z2.begin(), rParticleX2Y1Z2.end());
    if (rankX2Y2Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y2Z1.begin(), rParticleX2Y2Z1.end());
    if (rankX2Y2Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y2Z2.begin(), rParticleX2Y2Z2.end());

    // after receive, set SPHParticle.demParticle
    for(std::vector<Particle*>::iterator it=recvParticleVec.begin();
it!=recvParticleVec.end(); it++){
  (*it)->setDemParticleInSPHParticle();
    }

    mergeParticleVec.clear();
    mergeParticleVec = particleVec; // duplicate pointers, pointing to the same
memory
    mergeParticleVec.insert(mergeParticleVec.end(), recvParticleVec.begin(),
recvParticleVec.end());

    // at this point, the SPHGhostParticles in part of particleVec are pointing
to NULL, needed to assign back
    // this must be done after the receive is complete

    // 6 surfaces
    if (rankX1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1.begin();
it!=particleX1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2.begin();
it!=particleX2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankY1 >= 0)
      for(std::vector<Particle*>::iterator it=particleY1.begin();
it!=particleY1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankY2 >= 0)
      for(std::vector<Particle*>::iterator it=particleY2.begin();
it!=particleY2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankZ1 >= 0)
      for(std::vector<Particle*>::iterator it=particleZ1.begin();
it!=particleZ1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankZ2 >= 0)
      for(std::vector<Particle*>::iterator it=particleZ2.begin();
it!=particleZ2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    // 12 edges
    if (rankX1Y1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1Y1.begin();
it!=particleX1Y1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX1Y2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1Y2.begin();
it!=particleX1Y2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX1Z1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1Z1.begin();
it!=particleX1Z1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX1Z2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1Z2.begin();
it!=particleX1Z2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2Y1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2Y1.begin();
it!=particleX2Y1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2Y2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2Y2.begin();
it!=particleX2Y2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2Z1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2Z1.begin();
it!=particleX2Z1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2Z2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2Z2.begin();
it!=particleX2Z2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankY1Z1 >= 0)
      for(std::vector<Particle*>::iterator it=particleY1Z1.begin();
it!=particleY1Z1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankY1Z2 >= 0)
      for(std::vector<Particle*>::iterator it=particleY1Z2.begin();
it!=particleY1Z2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankY2Z1 >= 0)
      for(std::vector<Particle*>::iterator it=particleY2Z1.begin();
it!=particleY2Z1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankY2Z2 >= 0)
      for(std::vector<Particle*>::iterator it=particleY2Z2.begin();
it!=particleY2Z2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    // 8 vertices
    if (rankX1Y1Z1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1Y1Z1.begin();
it!=particleX1Y1Z1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX1Y1Z2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1Y1Z2.begin();
it!=particleX1Y1Z2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX1Y2Z1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1Y2Z1.begin();
it!=particleX1Y2Z1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX1Y2Z2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX1Y2Z2.begin();
it!=particleX1Y2Z2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2Y1Z1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2Y1Z1.begin();
it!=particleX2Y1Z1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2Y1Z2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2Y1Z2.begin();
it!=particleX2Y1Z2.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2Y2Z1 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2Y2Z1.begin();
it!=particleX2Y2Z1.end(); it++)
  (*it)->setDemParticleInSPHParticle();
    if (rankX2Y2Z2 >= 0)
      for(std::vector<Particle*>::iterator it=particleX2Y2Z2.begin();
it!=particleX2Y2Z2.end(); it++)
  (*it)->setDemParticleInSPHParticle();


      // std::vector<Particle*> testParticleVec;
      // testParticleVec.insert(testParticleVec.end(), rParticleX1.begin(),
rParticleX1.end());
      // testParticleVec.insert(testParticleVec.end(), rParticleX2.begin(),
rParticleX2.end());
      // testParticleVec.insert(testParticleVec.end(), rParticleY1.begin(),
rParticleY1.end());
      // testParticleVec.insert(testParticleVec.end(), rParticleY2.begin(),
rParticleY2.end());
      // testParticleVec.insert(testParticleVec.end(), rParticleZ1.begin(),
rParticleZ1.end());
      // testParticleVec.insert(testParticleVec.end(), rParticleZ2.begin(),
rParticleZ2.end());
      // debugInf << "iter=" << std::setw(4) << iteration << " rank=" <<
std::setw(4) << mpiRank
      // << " ptclNum=" << std::setw(4) << particleVec.size()
      // << " surface="
      // << std::setw(4) << particleX1.size()  << std::setw(4) <<
particleX2.size()
      // << std::setw(4) << particleY1.size()  << std::setw(4) <<
particleY2.size()
      // << std::setw(4) << particleZ1.size()  << std::setw(4) <<
particleZ2.size()
      // << " recv="
      // << std::setw(4) << rParticleX1.size() << std::setw(4) <<
rParticleX2.size()
      // << std::setw(4) << rParticleY1.size() << std::setw(4) <<
rParticleY2.size()
      // << std::setw(4) << rParticleZ1.size() << std::setw(4) <<
rParticleZ2.size()
      // << " rNum="
      // << std::setw(4) << recvParticleVec.size() << ": ";

      // for (std::vector<Particle*>::const_iterator it =
testParticleVec.begin(); it != testParticleVec.end();++it)
      // debugInf << (*it)->getId() << ' ';
      // debugInf << std::endl;
      // testParticleVec.clear();
  }

  // the communication of ghost sph particles are not implemented, the
communication of ghost sph should be
  // corresponding to their dem particles. July 16, 2015
  void SmoothParticleHydro::commuSPHParticle()
  {
    // determine container of each process
    Vec v1 = grid.getMinCorner();
    Vec v2 = grid.getMaxCorner();
    Vec vspan = v2 - v1;
    container = Rectangle(v1.x() + vspan.x() / mpiProcX * mpiCoords[0],
        v1.y() + vspan.y() / mpiProcY * mpiCoords[1],
        v1.z() + vspan.z() / mpiProcZ * mpiCoords[2],
        v1.x() + vspan.x() / mpiProcX * (mpiCoords[0] + 1),
        v1.y() + vspan.y() / mpiProcY * (mpiCoords[1] + 1),
        v1.z() + vspan.z() / mpiProcZ * (mpiCoords[2] + 1));

    // if found, communicate with neighboring blocks
    std::vector<sph::SPHParticle*> sphParticleX1, sphParticleX2;
    std::vector<sph::SPHParticle*> sphParticleY1, sphParticleY2;
    std::vector<sph::SPHParticle*> sphParticleZ1, sphParticleZ2;
    std::vector<sph::SPHParticle*> sphParticleX1Y1, sphParticleX1Y2,
sphParticleX1Z1, sphParticleX1Z2;
    std::vector<sph::SPHParticle*> sphParticleX2Y1, sphParticleX2Y2,
sphParticleX2Z1, sphParticleX2Z2;
    std::vector<sph::SPHParticle*> sphParticleY1Z1, sphParticleY1Z2,
sphParticleY2Z1, sphParticleY2Z2;
    std::vector<sph::SPHParticle*> sphParticleX1Y1Z1, sphParticleX1Y1Z2,
sphParticleX1Y2Z1, sphParticleX1Y2Z2;
    std::vector<sph::SPHParticle*> sphParticleX2Y1Z1, sphParticleX2Y1Z2,
sphParticleX2Y2Z1, sphParticleX2Y2Z2;
    boost::mpi::request reqX1[2], reqX2[2];
    boost::mpi::request reqY1[2], reqY2[2];
    boost::mpi::request reqZ1[2], reqZ2[2];
    boost::mpi::request reqX1Y1[2], reqX1Y2[2], reqX1Z1[2], reqX1Z2[2];
    boost::mpi::request reqX2Y1[2], reqX2Y2[2], reqX2Z1[2], reqX2Z2[2];
    boost::mpi::request reqY1Z1[2], reqY1Z2[2], reqY2Z1[2], reqY2Z2[2];
    boost::mpi::request reqX1Y1Z1[2], reqX1Y1Z2[2], reqX1Y2Z1[2], reqX1Y2Z2[2];
    boost::mpi::request reqX2Y1Z1[2], reqX2Y1Z2[2], reqX2Y2Z1[2], reqX2Y2Z2[2];
    v1 = container.getMinCorner(); // redefine v1, v2 in terms of process
    v2 = container.getMaxCorner();
    //debugInf << "rank=" << mpiRank << ' ' << v1.x() << ' ' << v1.y() << ' ' <<
v1.z() << ' '  << v2.x() << ' ' << v2.y() << ' ' << v2.z() << std::endl;
    REAL cellSize = sphCellSize;
    // 6 surfaces
    if (rankX1 >= 0) { // surface x1
      Rectangle containerX1(v1.x(), v1.y(), v1.z(),
          v1.x() + cellSize, v2.y(), v2.z());
      findSPHParticleInRectangle(containerX1, d_sphParticleVec, sphParticleX1);
      reqX1[0] = boostWorld.isend(rankX1, mpiTag,  sphParticleX1);
      reqX1[1] = boostWorld.irecv(rankX1, mpiTag, rsphParticleX1);
    }
    if (rankX2 >= 0) { // surface x2
      Rectangle containerX2(v2.x() - cellSize, v1.y(), v1.z(),
          v2.x(), v2.y(), v2.z());
      findSPHParticleInRectangle(containerX2, d_sphParticleVec, sphParticleX2);
      reqX2[0] = boostWorld.isend(rankX2, mpiTag,  sphParticleX2);
      reqX2[1] = boostWorld.irecv(rankX2, mpiTag, rsphParticleX2);
    }
    if (rankY1 >= 0) {  // surface y1
      Rectangle containerY1(v1.x(), v1.y(), v1.z(),
          v2.x(), v1.y() + cellSize, v2.z());
      findSPHParticleInRectangle(containerY1, d_sphParticleVec, sphParticleY1);
      reqY1[0] = boostWorld.isend(rankY1, mpiTag,  sphParticleY1);
      reqY1[1] = boostWorld.irecv(rankY1, mpiTag, rsphParticleY1);
    }
    if (rankY2 >= 0) {  // surface y2
      Rectangle containerY2(v1.x(), v2.y() - cellSize, v1.z(),
          v2.x(), v2.y(), v2.z());
      findSPHParticleInRectangle(containerY2, d_sphParticleVec, sphParticleY2);
      reqY2[0] = boostWorld.isend(rankY2, mpiTag,  sphParticleY2);
      reqY2[1] = boostWorld.irecv(rankY2, mpiTag, rsphParticleY2);
    }
    if (rankZ1 >= 0) {  // surface z1
      Rectangle containerZ1(v1.x(), v1.y(), v1.z(),
          v2.x(), v2.y(), v1.z() + cellSize);
      findSPHParticleInRectangle(containerZ1, d_sphParticleVec, sphParticleZ1);
      reqZ1[0] = boostWorld.isend(rankZ1, mpiTag,  sphParticleZ1);
      reqZ1[1] = boostWorld.irecv(rankZ1, mpiTag, rsphParticleZ1);
    }
    if (rankZ2 >= 0) {  // surface z2
      Rectangle containerZ2(v1.x(), v1.y(), v2.z() - cellSize,
          v2.x(), v2.y(), v2.z());
      findSPHParticleInRectangle(containerZ2, d_sphParticleVec, sphParticleZ2);
      reqZ2[0] = boostWorld.isend(rankZ2, mpiTag,  sphParticleZ2);
      reqZ2[1] = boostWorld.irecv(rankZ2, mpiTag, rsphParticleZ2);
    }
    // 12 edges
    if (rankX1Y1 >= 0) { // edge x1y1
      Rectangle containerX1Y1(v1.x(), v1.y(), v1.z(),
            v1.x() + cellSize, v1.y() + cellSize, v2.z());
      findSPHParticleInRectangle(containerX1Y1, d_sphParticleVec,
sphParticleX1Y1);
      reqX1Y1[0] = boostWorld.isend(rankX1Y1, mpiTag,  sphParticleX1Y1);
      reqX1Y1[1] = boostWorld.irecv(rankX1Y1, mpiTag, rsphParticleX1Y1);
    }
    if (rankX1Y2 >= 0) { // edge x1y2
      Rectangle containerX1Y2(v1.x(), v2.y() - cellSize, v1.z(),
            v1.x() + cellSize, v2.y(), v2.z());
      findSPHParticleInRectangle(containerX1Y2, d_sphParticleVec,
sphParticleX1Y2);
      reqX1Y2[0] = boostWorld.isend(rankX1Y2, mpiTag,  sphParticleX1Y2);
      reqX1Y2[1] = boostWorld.irecv(rankX1Y2, mpiTag, rsphParticleX1Y2);
    }
    if (rankX1Z1 >= 0) { // edge x1z1
      Rectangle containerX1Z1(v1.x(), v1.y(), v1.z(),
            v1.x() + cellSize, v2.y(), v1.z() + cellSize);
      findSPHParticleInRectangle(containerX1Z1, d_sphParticleVec,
sphParticleX1Z1);
      reqX1Z1[0] = boostWorld.isend(rankX1Z1, mpiTag,  sphParticleX1Z1);
      reqX1Z1[1] = boostWorld.irecv(rankX1Z1, mpiTag, rsphParticleX1Z1);
    }
    if (rankX1Z2 >= 0) { // edge x1z2
      Rectangle containerX1Z2(v1.x(), v1.y(), v2.z() - cellSize,
            v1.x() + cellSize, v2.y(), v2.z());
      findSPHParticleInRectangle(containerX1Z2, d_sphParticleVec,
sphParticleX1Z2);
      reqX1Z2[0] = boostWorld.isend(rankX1Z2, mpiTag,  sphParticleX1Z2);
      reqX1Z2[1] = boostWorld.irecv(rankX1Z2, mpiTag, rsphParticleX1Z2);
    }
    if (rankX2Y1 >= 0) { // edge x2y1
      Rectangle containerX2Y1(v2.x() - cellSize, v1.y(), v1.z(),
            v2.x(), v1.y() + cellSize, v2.z());
      findSPHParticleInRectangle(containerX2Y1, d_sphParticleVec,
sphParticleX2Y1);
      reqX2Y1[0] = boostWorld.isend(rankX2Y1, mpiTag,  sphParticleX2Y1);
      reqX2Y1[1] = boostWorld.irecv(rankX2Y1, mpiTag, rsphParticleX2Y1);
    }
    if (rankX2Y2 >= 0) { // edge x2y2
      Rectangle containerX2Y2(v2.x() - cellSize, v2.y() - cellSize, v1.z(),
            v2.x(), v2.y(), v2.z());
      findSPHParticleInRectangle(containerX2Y2, d_sphParticleVec,
sphParticleX2Y2);
      reqX2Y2[0] = boostWorld.isend(rankX2Y2, mpiTag,  sphParticleX2Y2);
      reqX2Y2[1] = boostWorld.irecv(rankX2Y2, mpiTag, rsphParticleX2Y2);
    }
    if (rankX2Z1 >= 0) { // edge x2z1
      Rectangle containerX2Z1(v2.x() - cellSize, v1.y(), v1.z(),
            v2.x(), v2.y(), v1.z() + cellSize);
      findSPHParticleInRectangle(containerX2Z1, d_sphParticleVec,
sphParticleX2Z1);
      reqX2Z1[0] = boostWorld.isend(rankX2Z1, mpiTag,  sphParticleX2Z1);
      reqX2Z1[1] = boostWorld.irecv(rankX2Z1, mpiTag, rsphParticleX2Z1);
    }
    if (rankX2Z2 >= 0) { // edge x2z2
      Rectangle containerX2Z2(v2.x() - cellSize, v1.y(), v2.z() - cellSize,
            v2.x(), v2.y(), v2.z());
      findSPHParticleInRectangle(containerX2Z2, d_sphParticleVec,
sphParticleX2Z2);
      reqX2Z2[0] = boostWorld.isend(rankX2Z2, mpiTag,  sphParticleX2Z2);
      reqX2Z2[1] = boostWorld.irecv(rankX2Z2, mpiTag, rsphParticleX2Z2);
    }
    if (rankY1Z1 >= 0) { // edge y1z1
      Rectangle containerY1Z1(v1.x(), v1.y(), v1.z(),
            v2.x(), v1.y() + cellSize, v1.z() + cellSize);
      findSPHParticleInRectangle(containerY1Z1, d_sphParticleVec,
sphParticleY1Z1);
      reqY1Z1[0] = boostWorld.isend(rankY1Z1, mpiTag,  sphParticleY1Z1);
      reqY1Z1[1] = boostWorld.irecv(rankY1Z1, mpiTag, rsphParticleY1Z1);
    }
    if (rankY1Z2 >= 0) { // edge y1z2
      Rectangle containerY1Z2(v1.x(), v1.y(), v2.z() - cellSize,
            v2.x(), v1.y() + cellSize, v2.z());
      findSPHParticleInRectangle(containerY1Z2, d_sphParticleVec,
sphParticleY1Z2);
      reqY1Z2[0] = boostWorld.isend(rankY1Z2, mpiTag,  sphParticleY1Z2);
      reqY1Z2[1] = boostWorld.irecv(rankY1Z2, mpiTag, rsphParticleY1Z2);
    }
    if (rankY2Z1 >= 0) { // edge y2z1
      Rectangle containerY2Z1(v1.x(), v2.y() - cellSize, v1.z(),
            v2.x(), v2.y(), v1.z() + cellSize);
      findSPHParticleInRectangle(containerY2Z1, d_sphParticleVec,
sphParticleY2Z1);
      reqY2Z1[0] = boostWorld.isend(rankY2Z1, mpiTag,  sphParticleY2Z1);
      reqY2Z1[1] = boostWorld.irecv(rankY2Z1, mpiTag, rsphParticleY2Z1);
    }
    if (rankY2Z2 >= 0) { // edge y2z2
      Rectangle containerY2Z2(v1.x(), v2.y() - cellSize, v2.z() - cellSize,
            v2.x(), v2.y(), v2.z());
      findSPHParticleInRectangle(containerY2Z2, d_sphParticleVec,
sphParticleY2Z2);
      reqY2Z2[0] = boostWorld.isend(rankY2Z2, mpiTag,  sphParticleY2Z2);
      reqY2Z2[1] = boostWorld.irecv(rankY2Z2, mpiTag, rsphParticleY2Z2);
    }
    // 8 vertices
    if (rankX1Y1Z1 >= 0) { // edge x1y1z1
      Rectangle containerX1Y1Z1(v1.x(), v1.y(), v1.z(),
        v1.x() + cellSize, v1.y() + cellSize, v1.z() + cellSize);
      findSPHParticleInRectangle(containerX1Y1Z1, d_sphParticleVec,
sphParticleX1Y1Z1);
      reqX1Y1Z1[0] = boostWorld.isend(rankX1Y1Z1, mpiTag,  sphParticleX1Y1Z1);
      reqX1Y1Z1[1] = boostWorld.irecv(rankX1Y1Z1, mpiTag, rsphParticleX1Y1Z1);
    }
    if (rankX1Y1Z2 >= 0) { // edge x1y1z2
      Rectangle containerX1Y1Z2(v1.x(), v1.y(), v2.z() - cellSize,
        v1.x() + cellSize, v1.y() + cellSize, v2.z());
      findSPHParticleInRectangle(containerX1Y1Z2, d_sphParticleVec,
sphParticleX1Y1Z2);
      reqX1Y1Z2[0] = boostWorld.isend(rankX1Y1Z2, mpiTag,  sphParticleX1Y1Z2);
      reqX1Y1Z2[1] = boostWorld.irecv(rankX1Y1Z2, mpiTag, rsphParticleX1Y1Z2);
    }
    if (rankX1Y2Z1 >= 0) { // edge x1y2z1
      Rectangle containerX1Y2Z1(v1.x(), v2.y() - cellSize, v1.z(),
        v1.x() + cellSize, v2.y(), v1.z() + cellSize);
      findSPHParticleInRectangle(containerX1Y2Z1, d_sphParticleVec,
sphParticleX1Y2Z1);
      reqX1Y2Z1[0] = boostWorld.isend(rankX1Y2Z1, mpiTag,  sphParticleX1Y2Z1);
      reqX1Y2Z1[1] = boostWorld.irecv(rankX1Y2Z1, mpiTag, rsphParticleX1Y2Z1);
    }
    if (rankX1Y2Z2 >= 0) { // edge x1y2z2
      Rectangle containerX1Y2Z2(v1.x(), v2.y() - cellSize, v2.z() - cellSize,
        v1.x() + cellSize, v2.y() + cellSize, v2.z());
      findSPHParticleInRectangle(containerX1Y2Z2, d_sphParticleVec,
sphParticleX1Y2Z2);
      reqX1Y2Z2[0] = boostWorld.isend(rankX1Y2Z2, mpiTag,  sphParticleX1Y2Z2);
      reqX1Y2Z2[1] = boostWorld.irecv(rankX1Y2Z2, mpiTag, rsphParticleX1Y2Z2);
    }
    if (rankX2Y1Z1 >= 0) { // edge x2y1z1
      Rectangle containerX2Y1Z1(v2.x() - cellSize, v1.y(), v1.z(),
        v2.x(), v1.y() + cellSize, v1.z() + cellSize);
      findSPHParticleInRectangle(containerX2Y1Z1, d_sphParticleVec,
sphParticleX2Y1Z1);
      reqX2Y1Z1[0] = boostWorld.isend(rankX2Y1Z1, mpiTag,  sphParticleX2Y1Z1);
      reqX2Y1Z1[1] = boostWorld.irecv(rankX2Y1Z1, mpiTag, rsphParticleX2Y1Z1);
    }
    if (rankX2Y1Z2 >= 0) { // edge x2y1z2
      Rectangle containerX2Y1Z2(v2.x() - cellSize, v1.y(), v2.z() - cellSize,
        v2.x(), v1.y() + cellSize, v2.z());
      findSPHParticleInRectangle(containerX2Y1Z2, d_sphParticleVec,
sphParticleX2Y1Z2);
      reqX2Y1Z2[0] = boostWorld.isend(rankX2Y1Z2, mpiTag,  sphParticleX2Y1Z2);
      reqX2Y1Z2[1] = boostWorld.irecv(rankX2Y1Z2, mpiTag, rsphParticleX2Y1Z2);
    }
    if (rankX2Y2Z1 >= 0) { // edge x2y2z1
      Rectangle containerX2Y2Z1(v2.x() - cellSize, v2.y() - cellSize, v1.z(),
        v2.x(), v2.y(), v1.z() + cellSize);
      findSPHParticleInRectangle(containerX2Y2Z1, d_sphParticleVec,
sphParticleX2Y2Z1);
      reqX2Y2Z1[0] = boostWorld.isend(rankX2Y2Z1, mpiTag,  sphParticleX2Y2Z1);
      reqX2Y2Z1[1] = boostWorld.irecv(rankX2Y2Z1, mpiTag, rsphParticleX2Y2Z1);
    }
    if (rankX2Y2Z2 >= 0) { // edge x2y2z2
      Rectangle containerX2Y2Z2(v2.x() - cellSize, v2.y() - cellSize, v2.z() -
cellSize,
        v2.x(), v2.y(), v2.z());
      findSPHParticleInRectangle(containerX2Y2Z2, d_sphParticleVec,
sphParticleX2Y2Z2);
      reqX2Y2Z2[0] = boostWorld.isend(rankX2Y2Z2, mpiTag,  sphParticleX2Y2Z2);
      reqX2Y2Z2[1] = boostWorld.irecv(rankX2Y2Z2, mpiTag, rsphParticleX2Y2Z2);
    }

    // 6 surfaces
    if (rankX1 >= 0) boost::mpi::wait_all(reqX1, reqX1 + 2);
    if (rankX2 >= 0) boost::mpi::wait_all(reqX2, reqX2 + 2);
    if (rankY1 >= 0) boost::mpi::wait_all(reqY1, reqY1 + 2);
    if (rankY2 >= 0) boost::mpi::wait_all(reqY2, reqY2 + 2);
    if (rankZ1 >= 0) boost::mpi::wait_all(reqZ1, reqZ1 + 2);
    if (rankZ2 >= 0) boost::mpi::wait_all(reqZ2, reqZ2 + 2);
    // 12 edges
    if (rankX1Y1 >= 0) boost::mpi::wait_all(reqX1Y1, reqX1Y1 + 2);
    if (rankX1Y2 >= 0) boost::mpi::wait_all(reqX1Y2, reqX1Y2 + 2);
    if (rankX1Z1 >= 0) boost::mpi::wait_all(reqX1Z1, reqX1Z1 + 2);
    if (rankX1Z2 >= 0) boost::mpi::wait_all(reqX1Z2, reqX1Z2 + 2);
    if (rankX2Y1 >= 0) boost::mpi::wait_all(reqX2Y1, reqX2Y1 + 2);
    if (rankX2Y2 >= 0) boost::mpi::wait_all(reqX2Y2, reqX2Y2 + 2);
    if (rankX2Z1 >= 0) boost::mpi::wait_all(reqX2Z1, reqX2Z1 + 2);
    if (rankX2Z2 >= 0) boost::mpi::wait_all(reqX2Z2, reqX2Z2 + 2);
    if (rankY1Z1 >= 0) boost::mpi::wait_all(reqY1Z1, reqY1Z1 + 2);
    if (rankY1Z2 >= 0) boost::mpi::wait_all(reqY1Z2, reqY1Z2 + 2);
    if (rankY2Z1 >= 0) boost::mpi::wait_all(reqY2Z1, reqY2Z1 + 2);
    if (rankY2Z2 >= 0) boost::mpi::wait_all(reqY2Z2, reqY2Z2 + 2);
    // 8 vertices
    if (rankX1Y1Z1 >= 0) boost::mpi::wait_all(reqX1Y1Z1, reqX1Y1Z1 + 2);
    if (rankX1Y1Z2 >= 0) boost::mpi::wait_all(reqX1Y1Z2, reqX1Y1Z2 + 2);
    if (rankX1Y2Z1 >= 0) boost::mpi::wait_all(reqX1Y2Z1, reqX1Y2Z1 + 2);
    if (rankX1Y2Z2 >= 0) boost::mpi::wait_all(reqX1Y2Z2, reqX1Y2Z2 + 2);
    if (rankX2Y1Z1 >= 0) boost::mpi::wait_all(reqX2Y1Z1, reqX2Y1Z1 + 2);
    if (rankX2Y1Z2 >= 0) boost::mpi::wait_all(reqX2Y1Z2, reqX2Y1Z2 + 2);
    if (rankX2Y2Z1 >= 0) boost::mpi::wait_all(reqX2Y2Z1, reqX2Y2Z1 + 2);
    if (rankX2Y2Z2 >= 0) boost::mpi::wait_all(reqX2Y2Z2, reqX2Y2Z2 + 2);

    // merge: sphParticles inside container (at front) + sphParticles from
neighoring blocks (at end)
    d_recvSPHParticleVec.clear();
    // 6 surfaces
    if (rankX1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1.begin(), rsphParticleX1.end());
    if (rankX2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2.begin(), rsphParticleX2.end());
    if (rankY1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY1.begin(), rsphParticleY1.end());
    if (rankY2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY2.begin(), rsphParticleY2.end());
    if (rankZ1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleZ1.begin(), rsphParticleZ1.end());
    if (rankZ2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleZ2.begin(), rsphParticleZ2.end());
    // 12 edges
    if (rankX1Y1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y1.begin(), rsphParticleX1Y1.end());
    if (rankX1Y2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y2.begin(), rsphParticleX1Y2.end());
    if (rankX1Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Z1.begin(), rsphParticleX1Z1.end());
    if (rankX1Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Z2.begin(), rsphParticleX1Z2.end());
    if (rankX2Y1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y1.begin(), rsphParticleX2Y1.end());
    if (rankX2Y2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y2.begin(), rsphParticleX2Y2.end());
    if (rankX2Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Z1.begin(), rsphParticleX2Z1.end());
    if (rankX2Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Z2.begin(), rsphParticleX2Z2.end());
    if (rankY1Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY1Z1.begin(), rsphParticleY1Z1.end());
    if (rankY1Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY1Z2.begin(), rsphParticleY1Z2.end());
    if (rankY2Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY2Z1.begin(), rsphParticleY2Z1.end());
    if (rankY2Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY2Z2.begin(), rsphParticleY2Z2.end());
    // 8 vertices
    if (rankX1Y1Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y1Z1.begin(), rsphParticleX1Y1Z1.end());
    if (rankX1Y1Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y1Z2.begin(), rsphParticleX1Y1Z2.end());
    if (rankX1Y2Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y2Z1.begin(), rsphParticleX1Y2Z1.end());
    if (rankX1Y2Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y2Z2.begin(), rsphParticleX1Y2Z2.end());
    if (rankX2Y1Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y1Z1.begin(), rsphParticleX2Y1Z1.end());
    if (rankX2Y1Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y1Z2.begin(), rsphParticleX2Y1Z2.end());
    if (rankX2Y2Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y2Z1.begin(), rsphParticleX2Y2Z1.end());
    if (rankX2Y2Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y2Z2.begin(), rsphParticleX2Y2Z2.end());

    mergeSPHParticleVec.clear();
    mergeSPHParticleVec = d_sphParticleVec; // duplicate pointers, pointing to
the same memory
    mergeSPHParticleVec.insert(mergeSPHParticleVec.end(),
d_recvSPHParticleVec.begin(), d_recvSPHParticleVec.end());

    #ifdef 0
      std::vector<Particle*> testParticleVec;
      testParticleVec.insert(testParticleVec.end(), rParticleX1.begin(),
rParticleX1.end());
      testParticleVec.insert(testParticleVec.end(), rParticleX2.begin(),
rParticleX2.end());
      testParticleVec.insert(testParticleVec.end(), rParticleY1.begin(),
rParticleY1.end());
      testParticleVec.insert(testParticleVec.end(), rParticleY2.begin(),
rParticleY2.end());
      testParticleVec.insert(testParticleVec.end(), rParticleZ1.begin(),
rParticleZ1.end());
      testParticleVec.insert(testParticleVec.end(), rParticleZ2.begin(),
rParticleZ2.end());
      debugInf << "iter=" << std::setw(4) << iteration << " rank=" <<
std::setw(4) << mpiRank
      << " ptclNum=" << std::setw(4) << particleVec.size()
      << " surface="
      << std::setw(4) << particleX1.size()  << std::setw(4) << particleX2.size()
      << std::setw(4) << particleY1.size()  << std::setw(4) << particleY2.size()
      << std::setw(4) << particleZ1.size()  << std::setw(4) << particleZ2.size()
      << " recv="
      << std::setw(4) << rParticleX1.size() << std::setw(4) <<
rParticleX2.size()
      << std::setw(4) << rParticleY1.size() << std::setw(4) <<
rParticleY2.size()
      << std::setw(4) << rParticleZ1.size() << std::setw(4) <<
rParticleZ2.size()
      << " rNum="
      << std::setw(4) << recvParticleVec.size() << ": ";

      for (std::vector<Particle*>::const_iterator it = testParticleVec.begin();
it != testParticleVec.end();++it)
      debugInf << (*it)->getId() << ' ';
      debugInf << std::endl;
      testParticleVec.clear();
    #endif
  }


  void SmoothParticleHydro::releaseRecvParticle() {
    // release memory of received particles
    for (std::vector<Particle*>::iterator it = recvParticleVec.begin(); it !=
recvParticleVec.end(); ++it){
      for(std::vector<sph::SPHParticle*>::iterator
st=(*it)->SPHGhostParticleVec.begin(); st!=(*it)->SPHGhostParticleVec.end();
st++){
  delete (*st);  // release memory of these sph ghost particles
      }
      (*it)->SPHGhostParticleVec.clear();
      delete (*it);
    }
    recvParticleVec.clear();
    // 6 surfaces
    rParticleX1.clear();
    rParticleX2.clear();
    rParticleY1.clear();
    rParticleY2.clear();
    rParticleZ1.clear();
    rParticleZ2.clear();
    // 12 edges
    rParticleX1Y1.clear();
    rParticleX1Y2.clear();
    rParticleX1Z1.clear();
    rParticleX1Z2.clear();
    rParticleX2Y1.clear();
    rParticleX2Y2.clear();
    rParticleX2Z1.clear();
    rParticleX2Z2.clear();
    rParticleY1Z1.clear();
    rParticleY1Z2.clear();
    rParticleY2Z1.clear();
    rParticleY2Z2.clear();
    // 8 vertices
    rParticleX1Y1Z1.clear();
    rParticleX1Y1Z2.clear();
    rParticleX1Y2Z1.clear();
    rParticleX1Y2Z2.clear();
    rParticleX2Y1Z1.clear();
    rParticleX2Y1Z2.clear();
    rParticleX2Y2Z1.clear();
    rParticleX2Y2Z2.clear();
  }


  void SmoothParticleHydro::releaseRecvSPHParticle() {
    // release memory of received particles
    for (std::vector<sph::SPHParticle*>::iterator it =
d_recvSPHParticleVec.begin(); it != d_recvSPHParticleVec.end(); ++it)
      delete (*it);
    d_recvSPHParticleVec.clear();
    // 6 surfaces
    rsphParticleX1.clear();
    rsphParticleX2.clear();
    rsphParticleY1.clear();
    rsphParticleY2.clear();
    rsphParticleZ1.clear();
    rsphParticleZ2.clear();
    // 12 edges
    rsphParticleX1Y1.clear();
    rsphParticleX1Y2.clear();
    rsphParticleX1Z1.clear();
    rsphParticleX1Z2.clear();
    rsphParticleX2Y1.clear();
    rsphParticleX2Y2.clear();
    rsphParticleX2Z1.clear();
    rsphParticleX2Z2.clear();
    rsphParticleY1Z1.clear();
    rsphParticleY1Z2.clear();
    rsphParticleY2Z1.clear();
    rsphParticleY2Z2.clear();
    // 8 vertices
    rsphParticleX1Y1Z1.clear();
    rsphParticleX1Y1Z2.clear();
    rsphParticleX1Y2Z1.clear();
    rsphParticleX1Y2Z2.clear();
    rsphParticleX2Y1Z1.clear();
    rsphParticleX2Y1Z2.clear();
    rsphParticleX2Y2Z1.clear();
    rsphParticleX2Y2Z2.clear();
  }

  void SmoothParticleHydro::migrateParticle()
  {
    Vec vspan = grid.getMaxCorner() - grid.getMinCorner();
    REAL segX = vspan.x() / mpiProcX;
    REAL segY = vspan.y() / mpiProcY;
    REAL segZ = vspan.z() / mpiProcZ;
    Vec v1 = container.getMinCorner(); // v1, v2 in terms of process
    Vec v2 = container.getMaxCorner();

    // if a neighbor exists, transfer particles crossing the boundary in
between.
    std::vector<Particle*> particleX1, particleX2;
    std::vector<Particle*> particleY1, particleY2;
    std::vector<Particle*> particleZ1, particleZ2;
    std::vector<Particle*> particleX1Y1, particleX1Y2, particleX1Z1,
particleX1Z2;
    std::vector<Particle*> particleX2Y1, particleX2Y2, particleX2Z1,
particleX2Z2;
    std::vector<Particle*> particleY1Z1, particleY1Z2, particleY2Z1,
particleY2Z2;
    std::vector<Particle*> particleX1Y1Z1, particleX1Y1Z2, particleX1Y2Z1,
particleX1Y2Z2;
    std::vector<Particle*> particleX2Y1Z1, particleX2Y1Z2, particleX2Y2Z1,
particleX2Y2Z2;
    boost::mpi::request reqX1[2], reqX2[2];
    boost::mpi::request reqY1[2], reqY2[2];
    boost::mpi::request reqZ1[2], reqZ2[2];
    boost::mpi::request reqX1Y1[2], reqX1Y2[2], reqX1Z1[2], reqX1Z2[2];
    boost::mpi::request reqX2Y1[2], reqX2Y2[2], reqX2Z1[2], reqX2Z2[2];
    boost::mpi::request reqY1Z1[2], reqY1Z2[2], reqY2Z1[2], reqY2Z2[2];
    boost::mpi::request reqX1Y1Z1[2], reqX1Y1Z2[2], reqX1Y2Z1[2], reqX1Y2Z2[2];
    boost::mpi::request reqX2Y1Z1[2], reqX2Y1Z2[2], reqX2Y2Z1[2], reqX2Y2Z2[2];

    // 6 surfaces
    if (rankX1 >= 0) { // surface x1
      Rectangle containerX1(v1.x() - segX, v1.y(), v1.z(),
          v1.x(), v2.y(), v2.z());
      findParticleInRectangle(containerX1, particleVec, particleX1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1.begin();
it!=particleX1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();  // now the SPHGhostParticles in
part of particleVec are pointing to NULL, don't needed to be back in migrate
      reqX1[0] = boostWorld.isend(rankX1, mpiTag,  particleX1);
      reqX1[1] = boostWorld.irecv(rankX1, mpiTag, rParticleX1);
    }
    if (rankX2 >= 0) { // surface x2
      Rectangle containerX2(v2.x(), v1.y(), v1.z(),
          v2.x() + segX, v2.y(), v2.z());
      findParticleInRectangle(containerX2, particleVec, particleX2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2.begin();
it!=particleX2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2[0] = boostWorld.isend(rankX2, mpiTag,  particleX2);
      reqX2[1] = boostWorld.irecv(rankX2, mpiTag, rParticleX2);
    }
    if (rankY1 >= 0) {  // surface y1
      Rectangle containerY1(v1.x(), v1.y() - segY, v1.z(),
          v2.x(), v1.y(), v2.z());
      findParticleInRectangle(containerY1, particleVec, particleY1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY1.begin();
it!=particleY1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY1[0] = boostWorld.isend(rankY1, mpiTag,  particleY1);
      reqY1[1] = boostWorld.irecv(rankY1, mpiTag, rParticleY1);
    }
    if (rankY2 >= 0) {  // surface y2
      Rectangle containerY2(v1.x(), v2.y(), v1.z(),
          v2.x(), v2.y() + segY, v2.z());
      findParticleInRectangle(containerY2, particleVec, particleY2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY2.begin();
it!=particleY2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY2[0] = boostWorld.isend(rankY2, mpiTag,  particleY2);
      reqY2[1] = boostWorld.irecv(rankY2, mpiTag, rParticleY2);
    }
    if (rankZ1 >= 0) {  // surface z1
      Rectangle containerZ1(v1.x(), v1.y(), v1.z() - segZ,
          v2.x(), v2.y(), v1.z());
      findParticleInRectangle(containerZ1, particleVec, particleZ1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleZ1.begin();
it!=particleZ1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqZ1[0] = boostWorld.isend(rankZ1, mpiTag,  particleZ1);
      reqZ1[1] = boostWorld.irecv(rankZ1, mpiTag, rParticleZ1);
    }
    if (rankZ2 >= 0) {  // surface z2
      Rectangle containerZ2(v1.x(), v1.y(), v2.z(),
          v2.x(), v2.y(), v2.z() + segZ);
      findParticleInRectangle(containerZ2, particleVec, particleZ2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleZ2.begin();
it!=particleZ2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqZ2[0] = boostWorld.isend(rankZ2, mpiTag,  particleZ2);
      reqZ2[1] = boostWorld.irecv(rankZ2, mpiTag, rParticleZ2);
    }
    // 12 edges
    if (rankX1Y1 >= 0) { // edge x1y1
      Rectangle containerX1Y1(v1.x() - segX, v1.y() - segY, v1.z(),
            v1.x(), v1.y(), v2.z());
      findParticleInRectangle(containerX1Y1, particleVec, particleX1Y1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y1.begin();
it!=particleX1Y1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y1[0] = boostWorld.isend(rankX1Y1, mpiTag,  particleX1Y1);
      reqX1Y1[1] = boostWorld.irecv(rankX1Y1, mpiTag, rParticleX1Y1);
    }
    if (rankX1Y2 >= 0) { // edge x1y2
      Rectangle containerX1Y2(v1.x() - segX, v2.y(), v1.z(),
            v1.x(), v2.y() + segY, v2.z());
      findParticleInRectangle(containerX1Y2, particleVec, particleX1Y2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y2.begin();
it!=particleX1Y2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y2[0] = boostWorld.isend(rankX1Y2, mpiTag,  particleX1Y2);
      reqX1Y2[1] = boostWorld.irecv(rankX1Y2, mpiTag, rParticleX1Y2);
    }
    if (rankX1Z1 >= 0) { // edge x1z1
      Rectangle containerX1Z1(v1.x() - segX, v1.y(), v1.z() -segZ,
            v1.x(), v2.y(), v1.z());
      findParticleInRectangle(containerX1Z1, particleVec, particleX1Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Z1.begin();
it!=particleX1Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Z1[0] = boostWorld.isend(rankX1Z1, mpiTag,  particleX1Z1);
      reqX1Z1[1] = boostWorld.irecv(rankX1Z1, mpiTag, rParticleX1Z1);
    }
    if (rankX1Z2 >= 0) { // edge x1z2
      Rectangle containerX1Z2(v1.x() - segX, v1.y(), v2.z(),
            v1.x(), v2.y(), v2.z() + segZ);
      findParticleInRectangle(containerX1Z2, particleVec, particleX1Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Z2.begin();
it!=particleX1Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Z2[0] = boostWorld.isend(rankX1Z2, mpiTag,  particleX1Z2);
      reqX1Z2[1] = boostWorld.irecv(rankX1Z2, mpiTag, rParticleX1Z2);
    }
    if (rankX2Y1 >= 0) { // edge x2y1
      Rectangle containerX2Y1(v2.x(), v1.y() - segY, v1.z(),
            v2.x() + segX, v1.y(), v2.z());
      findParticleInRectangle(containerX2Y1, particleVec, particleX2Y1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y1.begin();
it!=particleX2Y1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y1[0] = boostWorld.isend(rankX2Y1, mpiTag,  particleX2Y1);
      reqX2Y1[1] = boostWorld.irecv(rankX2Y1, mpiTag, rParticleX2Y1);
    }
    if (rankX2Y2 >= 0) { // edge x2y2
      Rectangle containerX2Y2(v2.x(), v2.y(), v1.z(),
            v2.x() + segX, v2.y() + segY, v2.z());
      findParticleInRectangle(containerX2Y2, particleVec, particleX2Y2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y2.begin();
it!=particleX2Y2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y2[0] = boostWorld.isend(rankX2Y2, mpiTag,  particleX2Y2);
      reqX2Y2[1] = boostWorld.irecv(rankX2Y2, mpiTag, rParticleX2Y2);
    }
    if (rankX2Z1 >= 0) { // edge x2z1
      Rectangle containerX2Z1(v2.x(), v1.y(), v1.z() - segZ,
            v2.x() + segX, v2.y(), v1.z());
      findParticleInRectangle(containerX2Z1, particleVec, particleX2Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Z1.begin();
it!=particleX2Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Z1[0] = boostWorld.isend(rankX2Z1, mpiTag,  particleX2Z1);
      reqX2Z1[1] = boostWorld.irecv(rankX2Z1, mpiTag, rParticleX2Z1);
    }
    if (rankX2Z2 >= 0) { // edge x2z2
      Rectangle containerX2Z2(v2.x(), v1.y(), v2.z(),
            v2.x() + segX, v2.y(), v2.z() + segZ);
      findParticleInRectangle(containerX2Z2, particleVec, particleX2Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Z2.begin();
it!=particleX2Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Z2[0] = boostWorld.isend(rankX2Z2, mpiTag,  particleX2Z2);
      reqX2Z2[1] = boostWorld.irecv(rankX2Z2, mpiTag, rParticleX2Z2);
    }
    if (rankY1Z1 >= 0) { // edge y1z1
      Rectangle containerY1Z1(v1.x(), v1.y() - segY, v1.z() - segZ,
            v2.x(), v1.y(), v1.z());
      findParticleInRectangle(containerY1Z1, particleVec, particleY1Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY1Z1.begin();
it!=particleY1Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY1Z1[0] = boostWorld.isend(rankY1Z1, mpiTag,  particleY1Z1);
      reqY1Z1[1] = boostWorld.irecv(rankY1Z1, mpiTag, rParticleY1Z1);
    }
    if (rankY1Z2 >= 0) { // edge y1z2
      Rectangle containerY1Z2(v1.x(), v1.y() - segY, v2.z(),
            v2.x(), v1.y(), v2.z() + segZ);
      findParticleInRectangle(containerY1Z2, particleVec, particleY1Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY1Z2.begin();
it!=particleY1Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY1Z2[0] = boostWorld.isend(rankY1Z2, mpiTag,  particleY1Z2);
      reqY1Z2[1] = boostWorld.irecv(rankY1Z2, mpiTag, rParticleY1Z2);
    }
    if (rankY2Z1 >= 0) { // edge y2z1
      Rectangle containerY2Z1(v1.x(), v2.y(), v1.z() - segZ,
            v2.x(), v2.y() + segY, v1.z());
      findParticleInRectangle(containerY2Z1, particleVec, particleY2Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY2Z1.begin();
it!=particleY2Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY2Z1[0] = boostWorld.isend(rankY2Z1, mpiTag,  particleY2Z1);
      reqY2Z1[1] = boostWorld.irecv(rankY2Z1, mpiTag, rParticleY2Z1);
    }
    if (rankY2Z2 >= 0) { // edge y2z2
      Rectangle containerY2Z2(v1.x(), v2.y(), v2.z(),
            v2.x(), v2.y() + segY, v2.z() + segZ);
      findParticleInRectangle(containerY2Z2, particleVec, particleY2Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleY2Z2.begin();
it!=particleY2Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqY2Z2[0] = boostWorld.isend(rankY2Z2, mpiTag,  particleY2Z2);
      reqY2Z2[1] = boostWorld.irecv(rankY2Z2, mpiTag, rParticleY2Z2);
    }
    // 8 vertices
    if (rankX1Y1Z1 >= 0) { // edge x1y1z1
      Rectangle containerX1Y1Z1(v1.x() - segX, v1.y() - segY, v1.z() - segZ,
        v1.x(), v1.y(), v1.z());
      findParticleInRectangle(containerX1Y1Z1, particleVec, particleX1Y1Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y1Z1.begin();
it!=particleX1Y1Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y1Z1[0] = boostWorld.isend(rankX1Y1Z1, mpiTag,  particleX1Y1Z1);
      reqX1Y1Z1[1] = boostWorld.irecv(rankX1Y1Z1, mpiTag, rParticleX1Y1Z1);
    }
    if (rankX1Y1Z2 >= 0) { // edge x1y1z2
      Rectangle containerX1Y1Z2(v1.x() - segX, v1.y() - segY, v2.z(),
        v1.x(), v1.y(), v2.z() + segZ);
      findParticleInRectangle(containerX1Y1Z2, particleVec, particleX1Y1Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y1Z2.begin();
it!=particleX1Y1Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y1Z2[0] = boostWorld.isend(rankX1Y1Z2, mpiTag,  particleX1Y1Z2);
      reqX1Y1Z2[1] = boostWorld.irecv(rankX1Y1Z2, mpiTag, rParticleX1Y1Z2);
    }
    if (rankX1Y2Z1 >= 0) { // edge x1y2z1
      Rectangle containerX1Y2Z1(v1.x() - segX, v2.y(), v1.z() - segZ,
        v1.x(), v2.y() + segY, v1.z());
      findParticleInRectangle(containerX1Y2Z1, particleVec, particleX1Y2Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y2Z1.begin();
it!=particleX1Y2Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y2Z1[0] = boostWorld.isend(rankX1Y2Z1, mpiTag,  particleX1Y2Z1);
      reqX1Y2Z1[1] = boostWorld.irecv(rankX1Y2Z1, mpiTag, rParticleX1Y2Z1);
    }
    if (rankX1Y2Z2 >= 0) { // edge x1y2z2
      Rectangle containerX1Y2Z2(v1.x() - segX, v2.y(), v2.z(),
        v1.x(), v2.y() + segY, v2.z() + segZ);
      findParticleInRectangle(containerX1Y2Z2, particleVec, particleX1Y2Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX1Y2Z2.begin();
it!=particleX1Y2Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX1Y2Z2[0] = boostWorld.isend(rankX1Y2Z2, mpiTag,  particleX1Y2Z2);
      reqX1Y2Z2[1] = boostWorld.irecv(rankX1Y2Z2, mpiTag, rParticleX1Y2Z2);
    }
    if (rankX2Y1Z1 >= 0) { // edge x2y1z1
      Rectangle containerX2Y1Z1(v2.x(), v1.y() - segY, v1.z() - segZ,
        v2.x() + segX, v1.y(), v1.z());
      findParticleInRectangle(containerX2Y1Z1, particleVec, particleX2Y1Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y1Z1.begin();
it!=particleX2Y1Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y1Z1[0] = boostWorld.isend(rankX2Y1Z1, mpiTag,  particleX2Y1Z1);
      reqX2Y1Z1[1] = boostWorld.irecv(rankX2Y1Z1, mpiTag, rParticleX2Y1Z1);
    }
    if (rankX2Y1Z2 >= 0) { // edge x2y1z2
      Rectangle containerX2Y1Z2(v2.x(), v1.y() - segY, v2.z(),
        v2.x() + segX, v1.y(), v2.z() + segZ);
      findParticleInRectangle(containerX2Y1Z2, particleVec, particleX2Y1Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y1Z2.begin();
it!=particleX2Y1Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y1Z2[0] = boostWorld.isend(rankX2Y1Z2, mpiTag,  particleX2Y1Z2);
      reqX2Y1Z2[1] = boostWorld.irecv(rankX2Y1Z2, mpiTag, rParticleX2Y1Z2);
    }
    if (rankX2Y2Z1 >= 0) { // edge x2y2z1
      Rectangle containerX2Y2Z1(v2.x(), v2.y(), v1.z() - segZ,
        v2.x() + segX, v2.y() + segY, v1.z());
      findParticleInRectangle(containerX2Y2Z1, particleVec, particleX2Y2Z1);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y2Z1.begin();
it!=particleX2Y2Z1.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y2Z1[0] = boostWorld.isend(rankX2Y2Z1, mpiTag,  particleX2Y2Z1);
      reqX2Y2Z1[1] = boostWorld.irecv(rankX2Y2Z1, mpiTag, rParticleX2Y2Z1);
    }
    if (rankX2Y2Z2 >= 0) { // edge x2y2z2
      Rectangle containerX2Y2Z2(v2.x(), v2.y(), v2.z(),
        v2.x() + segX, v2.y() + segY, v2.z() + segZ);
      findParticleInRectangle(containerX2Y2Z2, particleVec, particleX2Y2Z2);
      // before send, SPHParticle.demParticle should be NULL
      for(std::vector<Particle*>::iterator it=particleX2Y2Z2.begin();
it!=particleX2Y2Z2.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();
      reqX2Y2Z2[0] = boostWorld.isend(rankX2Y2Z2, mpiTag,  particleX2Y2Z2);
      reqX2Y2Z2[1] = boostWorld.irecv(rankX2Y2Z2, mpiTag, rParticleX2Y2Z2);
    }
    // 6 surfaces
    if (rankX1 >= 0) boost::mpi::wait_all(reqX1, reqX1 + 2);
    if (rankX2 >= 0) boost::mpi::wait_all(reqX2, reqX2 + 2);
    if (rankY1 >= 0) boost::mpi::wait_all(reqY1, reqY1 + 2);
    if (rankY2 >= 0) boost::mpi::wait_all(reqY2, reqY2 + 2);
    if (rankZ1 >= 0) boost::mpi::wait_all(reqZ1, reqZ1 + 2);
    if (rankZ2 >= 0) boost::mpi::wait_all(reqZ2, reqZ2 + 2);
    // 12 edges
    if (rankX1Y1 >= 0) boost::mpi::wait_all(reqX1Y1, reqX1Y1 + 2);
    if (rankX1Y2 >= 0) boost::mpi::wait_all(reqX1Y2, reqX1Y2 + 2);
    if (rankX1Z1 >= 0) boost::mpi::wait_all(reqX1Z1, reqX1Z1 + 2);
    if (rankX1Z2 >= 0) boost::mpi::wait_all(reqX1Z2, reqX1Z2 + 2);
    if (rankX2Y1 >= 0) boost::mpi::wait_all(reqX2Y1, reqX2Y1 + 2);
    if (rankX2Y2 >= 0) boost::mpi::wait_all(reqX2Y2, reqX2Y2 + 2);
    if (rankX2Z1 >= 0) boost::mpi::wait_all(reqX2Z1, reqX2Z1 + 2);
    if (rankX2Z2 >= 0) boost::mpi::wait_all(reqX2Z2, reqX2Z2 + 2);
    if (rankY1Z1 >= 0) boost::mpi::wait_all(reqY1Z1, reqY1Z1 + 2);
    if (rankY1Z2 >= 0) boost::mpi::wait_all(reqY1Z2, reqY1Z2 + 2);
    if (rankY2Z1 >= 0) boost::mpi::wait_all(reqY2Z1, reqY2Z1 + 2);
    if (rankY2Z2 >= 0) boost::mpi::wait_all(reqY2Z2, reqY2Z2 + 2);
    // 8 vertices
    if (rankX1Y1Z1 >= 0) boost::mpi::wait_all(reqX1Y1Z1, reqX1Y1Z1 + 2);
    if (rankX1Y1Z2 >= 0) boost::mpi::wait_all(reqX1Y1Z2, reqX1Y1Z2 + 2);
    if (rankX1Y2Z1 >= 0) boost::mpi::wait_all(reqX1Y2Z1, reqX1Y2Z1 + 2);
    if (rankX1Y2Z2 >= 0) boost::mpi::wait_all(reqX1Y2Z2, reqX1Y2Z2 + 2);
    if (rankX2Y1Z1 >= 0) boost::mpi::wait_all(reqX2Y1Z1, reqX2Y1Z1 + 2);
    if (rankX2Y1Z2 >= 0) boost::mpi::wait_all(reqX2Y1Z2, reqX2Y1Z2 + 2);
    if (rankX2Y2Z1 >= 0) boost::mpi::wait_all(reqX2Y2Z1, reqX2Y2Z1 + 2);
    if (rankX2Y2Z2 >= 0) boost::mpi::wait_all(reqX2Y2Z2, reqX2Y2Z2 + 2);

    // do not need to assign SPHGhostParticle.demParticle back in particleX1...,
since these particles will be removed
    // delete outgoing particles
    removeParticleOutRectangle();

    // add incoming particles
    recvParticleVec.clear(); // new use of recvParticleVec
    // 6 surfaces
    if (rankX1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1.begin(), rParticleX1.end());
    if (rankX2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2.begin(), rParticleX2.end());
    if (rankY1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY1.begin(), rParticleY1.end());
    if (rankY2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY2.begin(), rParticleY2.end());
    if (rankZ1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleZ1.begin(), rParticleZ1.end());
    if (rankZ2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleZ2.begin(), rParticleZ2.end());
    // 12 edges
    if (rankX1Y1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y1.begin(), rParticleX1Y1.end());
    if (rankX1Y2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y2.begin(), rParticleX1Y2.end());
    if (rankX1Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Z1.begin(), rParticleX1Z1.end());
    if (rankX1Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Z2.begin(), rParticleX1Z2.end());
    if (rankX2Y1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y1.begin(), rParticleX2Y1.end());
    if (rankX2Y2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y2.begin(), rParticleX2Y2.end());
    if (rankX2Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Z1.begin(), rParticleX2Z1.end());
    if (rankX2Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Z2.begin(), rParticleX2Z2.end());
    if (rankY1Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY1Z1.begin(), rParticleY1Z1.end());
    if (rankY1Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY1Z2.begin(), rParticleY1Z2.end());
    if (rankY2Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY2Z1.begin(), rParticleY2Z1.end());
    if (rankY2Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleY2Z2.begin(), rParticleY2Z2.end());
    // 8 vertices
    if (rankX1Y1Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y1Z1.begin(), rParticleX1Y1Z1.end());
    if (rankX1Y1Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y1Z2.begin(), rParticleX1Y1Z2.end());
    if (rankX1Y2Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y2Z1.begin(), rParticleX1Y2Z1.end());
    if (rankX1Y2Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX1Y2Z2.begin(), rParticleX1Y2Z2.end());
    if (rankX2Y1Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y1Z1.begin(), rParticleX2Y1Z1.end());
    if (rankX2Y1Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y1Z2.begin(), rParticleX2Y1Z2.end());
    if (rankX2Y2Z1 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y2Z1.begin(), rParticleX2Y2Z1.end());
    if (rankX2Y2Z2 >= 0) recvParticleVec.insert(recvParticleVec.end(),
rParticleX2Y2Z2.begin(), rParticleX2Y2Z2.end());

    // after receive, set SPHParticle.demParticle
    for(std::vector<Particle*>::iterator it=recvParticleVec.begin();
it!=recvParticleVec.end(); it++){
  (*it)->setDemParticleInSPHParticle();
    }

    particleVec.insert(particleVec.end(), recvParticleVec.begin(),
recvParticleVec.end());

    #ifdef 0
      if (recvParticleVec.size() > 0) {
      debugInf << "iter=" << std::setw(8) << iteration << " rank=" <<
std::setw(2) << mpiRank
      << "   added=";
      for (std::vector<Particle*>::const_iterator it = recvParticleVec.begin();
it != recvParticleVec.end(); ++it)
      debugInf << std::setw(3) << (*it)->getId();
      debugInf << " now " << particleVec.size() << ": ";
      for (std::vector<Particle*>::const_iterator it = particleVec.begin(); it
!= particleVec.end(); ++it)
      debugInf << std::setw(3) << (*it)->getId();
      debugInf << std::endl;
      }
    #endif

    // do not release memory of received particles because they are part of and
managed by particleVec
    // 6 surfaces
    rParticleX1.clear();
    rParticleX2.clear();
    rParticleY1.clear();
    rParticleY2.clear();
    rParticleZ1.clear();
    rParticleZ2.clear();
    // 12 edges
    rParticleX1Y1.clear();
    rParticleX1Y2.clear();
    rParticleX1Z1.clear();
    rParticleX1Z2.clear();
    rParticleX2Y1.clear();
    rParticleX2Y2.clear();
    rParticleX2Z1.clear();
    rParticleX2Z2.clear();
    rParticleY1Z1.clear();
    rParticleY1Z2.clear();
    rParticleY2Z1.clear();
    rParticleY2Z2.clear();
    // 8 vertices
    rParticleX1Y1Z1.clear();
    rParticleX1Y1Z2.clear();
    rParticleX1Y2Z1.clear();
    rParticleX1Y2Z2.clear();
    rParticleX2Y1Z1.clear();
    rParticleX2Y1Z2.clear();
    rParticleX2Y2Z1.clear();
    rParticleX2Y2Z2.clear();

    recvParticleVec.clear();
  }


  void SmoothParticleHydro::migrateSPHParticle()
  {
    Vec vspan = grid.getMaxCorner() - grid.getMinCorner();
    REAL segX = vspan.x() / mpiProcX;
    REAL segY = vspan.y() / mpiProcY;
    REAL segZ = vspan.z() / mpiProcZ;
    Vec v1 = container.getMinCorner(); // v1, v2 in terms of process
    Vec v2 = container.getMaxCorner();

    // if a neighbor exists, transfer particles crossing the boundary in
between.
    std::vector<sph::SPHParticle*> sphParticleX1, sphParticleX2;
    std::vector<sph::SPHParticle*> sphParticleY1, sphParticleY2;
    std::vector<sph::SPHParticle*> sphParticleZ1, sphParticleZ2;
    std::vector<sph::SPHParticle*> sphParticleX1Y1, sphParticleX1Y2,
sphParticleX1Z1, sphParticleX1Z2;
    std::vector<sph::SPHParticle*> sphParticleX2Y1, sphParticleX2Y2,
sphParticleX2Z1, sphParticleX2Z2;
    std::vector<sph::SPHParticle*> sphParticleY1Z1, sphParticleY1Z2,
sphParticleY2Z1, sphParticleY2Z2;
    std::vector<sph::SPHParticle*> sphParticleX1Y1Z1, sphParticleX1Y1Z2,
sphParticleX1Y2Z1, sphParticleX1Y2Z2;
    std::vector<sph::SPHParticle*> sphParticleX2Y1Z1, sphParticleX2Y1Z2,
sphParticleX2Y2Z1, sphParticleX2Y2Z2;
    boost::mpi::request reqX1[2], reqX2[2];
    boost::mpi::request reqY1[2], reqY2[2];
    boost::mpi::request reqZ1[2], reqZ2[2];
    boost::mpi::request reqX1Y1[2], reqX1Y2[2], reqX1Z1[2], reqX1Z2[2];
    boost::mpi::request reqX2Y1[2], reqX2Y2[2], reqX2Z1[2], reqX2Z2[2];
    boost::mpi::request reqY1Z1[2], reqY1Z2[2], reqY2Z1[2], reqY2Z2[2];
    boost::mpi::request reqX1Y1Z1[2], reqX1Y1Z2[2], reqX1Y2Z1[2], reqX1Y2Z2[2];
    boost::mpi::request reqX2Y1Z1[2], reqX2Y1Z2[2], reqX2Y2Z1[2], reqX2Y2Z2[2];

    // 6 surfaces
    if (rankX1 >= 0) { // surface x1
      Rectangle containerX1(v1.x() - segX, v1.y(), v1.z(),
          v1.x(), v2.y(), v2.z());
      findSPHParticleInRectangle(containerX1, d_sphParticleVec, sphParticleX1);
      reqX1[0] = boostWorld.isend(rankX1, mpiTag,  sphParticleX1);
      reqX1[1] = boostWorld.irecv(rankX1, mpiTag, rsphParticleX1);
    }
    if (rankX2 >= 0) { // surface x2
      Rectangle containerX2(v2.x(), v1.y(), v1.z(),
          v2.x() + segX, v2.y(), v2.z());
      findSPHParticleInRectangle(containerX2, d_sphParticleVec, sphParticleX2);
      reqX2[0] = boostWorld.isend(rankX2, mpiTag,  sphParticleX2);
      reqX2[1] = boostWorld.irecv(rankX2, mpiTag, rsphParticleX2);
    }
    if (rankY1 >= 0) {  // surface y1
      Rectangle containerY1(v1.x(), v1.y() - segY, v1.z(),
          v2.x(), v1.y(), v2.z());
      findSPHParticleInRectangle(containerY1, d_sphParticleVec, sphParticleY1);
      reqY1[0] = boostWorld.isend(rankY1, mpiTag,  sphParticleY1);
      reqY1[1] = boostWorld.irecv(rankY1, mpiTag, rsphParticleY1);
    }
    if (rankY2 >= 0) {  // surface y2
      Rectangle containerY2(v1.x(), v2.y(), v1.z(),
          v2.x(), v2.y() + segY, v2.z());
      findSPHParticleInRectangle(containerY2, d_sphParticleVec, sphParticleY2);
      reqY2[0] = boostWorld.isend(rankY2, mpiTag,  sphParticleY2);
      reqY2[1] = boostWorld.irecv(rankY2, mpiTag, rsphParticleY2);
    }
    if (rankZ1 >= 0) {  // surface z1
      Rectangle containerZ1(v1.x(), v1.y(), v1.z() - segZ,
          v2.x(), v2.y(), v1.z());
      findSPHParticleInRectangle(containerZ1, d_sphParticleVec, sphParticleZ1);
      reqZ1[0] = boostWorld.isend(rankZ1, mpiTag,  sphParticleZ1);
      reqZ1[1] = boostWorld.irecv(rankZ1, mpiTag, rsphParticleZ1);
    }
    if (rankZ2 >= 0) {  // surface z2
      Rectangle containerZ2(v1.x(), v1.y(), v2.z(),
          v2.x(), v2.y(), v2.z() + segZ);
      findSPHParticleInRectangle(containerZ2, d_sphParticleVec, sphParticleZ2);
      reqZ2[0] = boostWorld.isend(rankZ2, mpiTag,  sphParticleZ2);
      reqZ2[1] = boostWorld.irecv(rankZ2, mpiTag, rsphParticleZ2);
    }
    // 12 edges
    if (rankX1Y1 >= 0) { // edge x1y1
      Rectangle containerX1Y1(v1.x() - segX, v1.y() - segY, v1.z(),
            v1.x(), v1.y(), v2.z());
      findSPHParticleInRectangle(containerX1Y1, d_sphParticleVec,
sphParticleX1Y1);
      reqX1Y1[0] = boostWorld.isend(rankX1Y1, mpiTag,  sphParticleX1Y1);
      reqX1Y1[1] = boostWorld.irecv(rankX1Y1, mpiTag, rsphParticleX1Y1);
    }
    if (rankX1Y2 >= 0) { // edge x1y2
      Rectangle containerX1Y2(v1.x() - segX, v2.y(), v1.z(),
            v1.x(), v2.y() + segY, v2.z());
      findSPHParticleInRectangle(containerX1Y2, d_sphParticleVec,
sphParticleX1Y2);
      reqX1Y2[0] = boostWorld.isend(rankX1Y2, mpiTag,  sphParticleX1Y2);
      reqX1Y2[1] = boostWorld.irecv(rankX1Y2, mpiTag, rsphParticleX1Y2);
    }
    if (rankX1Z1 >= 0) { // edge x1z1
      Rectangle containerX1Z1(v1.x() - segX, v1.y(), v1.z() -segZ,
            v1.x(), v2.y(), v1.z());
      findSPHParticleInRectangle(containerX1Z1, d_sphParticleVec,
sphParticleX1Z1);
      reqX1Z1[0] = boostWorld.isend(rankX1Z1, mpiTag,  sphParticleX1Z1);
      reqX1Z1[1] = boostWorld.irecv(rankX1Z1, mpiTag, rsphParticleX1Z1);
    }
    if (rankX1Z2 >= 0) { // edge x1z2
      Rectangle containerX1Z2(v1.x() - segX, v1.y(), v2.z(),
            v1.x(), v2.y(), v2.z() + segZ);
      findSPHParticleInRectangle(containerX1Z2, d_sphParticleVec,
sphParticleX1Z2);
      reqX1Z2[0] = boostWorld.isend(rankX1Z2, mpiTag,  sphParticleX1Z2);
      reqX1Z2[1] = boostWorld.irecv(rankX1Z2, mpiTag, rsphParticleX1Z2);
    }
    if (rankX2Y1 >= 0) { // edge x2y1
      Rectangle containerX2Y1(v2.x(), v1.y() - segY, v1.z(),
            v2.x() + segX, v1.y(), v2.z());
      findSPHParticleInRectangle(containerX2Y1, d_sphParticleVec,
sphParticleX2Y1);
      reqX2Y1[0] = boostWorld.isend(rankX2Y1, mpiTag,  sphParticleX2Y1);
      reqX2Y1[1] = boostWorld.irecv(rankX2Y1, mpiTag, rsphParticleX2Y1);
    }
    if (rankX2Y2 >= 0) { // edge x2y2
      Rectangle containerX2Y2(v2.x(), v2.y(), v1.z(),
            v2.x() + segX, v2.y() + segY, v2.z());
      findSPHParticleInRectangle(containerX2Y2, d_sphParticleVec,
sphParticleX2Y2);
      reqX2Y2[0] = boostWorld.isend(rankX2Y2, mpiTag,  sphParticleX2Y2);
      reqX2Y2[1] = boostWorld.irecv(rankX2Y2, mpiTag, rsphParticleX2Y2);
    }
    if (rankX2Z1 >= 0) { // edge x2z1
      Rectangle containerX2Z1(v2.x(), v1.y(), v1.z() - segZ,
            v2.x() + segX, v2.y(), v1.z());
      findSPHParticleInRectangle(containerX2Z1, d_sphParticleVec,
sphParticleX2Z1);
      reqX2Z1[0] = boostWorld.isend(rankX2Z1, mpiTag,  sphParticleX2Z1);
      reqX2Z1[1] = boostWorld.irecv(rankX2Z1, mpiTag, rsphParticleX2Z1);
    }
    if (rankX2Z2 >= 0) { // edge x2z2
      Rectangle containerX2Z2(v2.x(), v1.y(), v2.z(),
            v2.x() + segX, v2.y(), v2.z() + segZ);
      findSPHParticleInRectangle(containerX2Z2, d_sphParticleVec,
sphParticleX2Z2);
      reqX2Z2[0] = boostWorld.isend(rankX2Z2, mpiTag,  sphParticleX2Z2);
      reqX2Z2[1] = boostWorld.irecv(rankX2Z2, mpiTag, rsphParticleX2Z2);
    }
    if (rankY1Z1 >= 0) { // edge y1z1
      Rectangle containerY1Z1(v1.x(), v1.y() - segY, v1.z() - segZ,
            v2.x(), v1.y(), v1.z());
      findSPHParticleInRectangle(containerY1Z1, d_sphParticleVec,
sphParticleY1Z1);
      reqY1Z1[0] = boostWorld.isend(rankY1Z1, mpiTag,  sphParticleY1Z1);
      reqY1Z1[1] = boostWorld.irecv(rankY1Z1, mpiTag, rsphParticleY1Z1);
    }
    if (rankY1Z2 >= 0) { // edge y1z2
      Rectangle containerY1Z2(v1.x(), v1.y() - segY, v2.z(),
            v2.x(), v1.y(), v2.z() + segZ);
      findSPHParticleInRectangle(containerY1Z2, d_sphParticleVec,
sphParticleY1Z2);
      reqY1Z2[0] = boostWorld.isend(rankY1Z2, mpiTag,  sphParticleY1Z2);
      reqY1Z2[1] = boostWorld.irecv(rankY1Z2, mpiTag, rsphParticleY1Z2);
    }
    if (rankY2Z1 >= 0) { // edge y2z1
      Rectangle containerY2Z1(v1.x(), v2.y(), v1.z() - segZ,
            v2.x(), v2.y() + segY, v1.z());
      findSPHParticleInRectangle(containerY2Z1, d_sphParticleVec,
sphParticleY2Z1);
      reqY2Z1[0] = boostWorld.isend(rankY2Z1, mpiTag,  sphParticleY2Z1);
      reqY2Z1[1] = boostWorld.irecv(rankY2Z1, mpiTag, rsphParticleY2Z1);
    }
    if (rankY2Z2 >= 0) { // edge y2z2
      Rectangle containerY2Z2(v1.x(), v2.y(), v2.z(),
            v2.x(), v2.y() + segY, v2.z() + segZ);
      findSPHParticleInRectangle(containerY2Z2, d_sphParticleVec,
sphParticleY2Z2);
      reqY2Z2[0] = boostWorld.isend(rankY2Z2, mpiTag,  sphParticleY2Z2);
      reqY2Z2[1] = boostWorld.irecv(rankY2Z2, mpiTag, rsphParticleY2Z2);
    }
    // 8 vertices
    if (rankX1Y1Z1 >= 0) { // edge x1y1z1
      Rectangle containerX1Y1Z1(v1.x() - segX, v1.y() - segY, v1.z() - segZ,
        v1.x(), v1.y(), v1.z());
      findSPHParticleInRectangle(containerX1Y1Z1, d_sphParticleVec,
sphParticleX1Y1Z1);
      reqX1Y1Z1[0] = boostWorld.isend(rankX1Y1Z1, mpiTag,  sphParticleX1Y1Z1);
      reqX1Y1Z1[1] = boostWorld.irecv(rankX1Y1Z1, mpiTag, rsphParticleX1Y1Z1);
    }
    if (rankX1Y1Z2 >= 0) { // edge x1y1z2
      Rectangle containerX1Y1Z2(v1.x() - segX, v1.y() - segY, v2.z(),
        v1.x(), v1.y(), v2.z() + segZ);
      findSPHParticleInRectangle(containerX1Y1Z2, d_sphParticleVec,
sphParticleX1Y1Z2);
      reqX1Y1Z2[0] = boostWorld.isend(rankX1Y1Z2, mpiTag,  sphParticleX1Y1Z2);
      reqX1Y1Z2[1] = boostWorld.irecv(rankX1Y1Z2, mpiTag, rsphParticleX1Y1Z2);
    }
    if (rankX1Y2Z1 >= 0) { // edge x1y2z1
      Rectangle containerX1Y2Z1(v1.x() - segX, v2.y(), v1.z() - segZ,
        v1.x(), v2.y() + segY, v1.z());
      findSPHParticleInRectangle(containerX1Y2Z1, d_sphParticleVec,
sphParticleX1Y2Z1);
      reqX1Y2Z1[0] = boostWorld.isend(rankX1Y2Z1, mpiTag,  sphParticleX1Y2Z1);
      reqX1Y2Z1[1] = boostWorld.irecv(rankX1Y2Z1, mpiTag, rsphParticleX1Y2Z1);
    }
    if (rankX1Y2Z2 >= 0) { // edge x1y2z2
      Rectangle containerX1Y2Z2(v1.x() - segX, v2.y(), v2.z(),
        v1.x(), v2.y() + segY, v2.z() + segZ);
      findSPHParticleInRectangle(containerX1Y2Z2, d_sphParticleVec,
sphParticleX1Y2Z2);
      reqX1Y2Z2[0] = boostWorld.isend(rankX1Y2Z2, mpiTag,  sphParticleX1Y2Z2);
      reqX1Y2Z2[1] = boostWorld.irecv(rankX1Y2Z2, mpiTag, rsphParticleX1Y2Z2);
    }
    if (rankX2Y1Z1 >= 0) { // edge x2y1z1
      Rectangle containerX2Y1Z1(v2.x(), v1.y() - segY, v1.z() - segZ,
        v2.x() + segX, v1.y(), v1.z());
      findSPHParticleInRectangle(containerX2Y1Z1, d_sphParticleVec,
sphParticleX2Y1Z1);
      reqX2Y1Z1[0] = boostWorld.isend(rankX2Y1Z1, mpiTag,  sphParticleX2Y1Z1);
      reqX2Y1Z1[1] = boostWorld.irecv(rankX2Y1Z1, mpiTag, rsphParticleX2Y1Z1);
    }
    if (rankX2Y1Z2 >= 0) { // edge x2y1z2
      Rectangle containerX2Y1Z2(v2.x(), v1.y() - segY, v2.z(),
        v2.x() + segX, v1.y(), v2.z() + segZ);
      findSPHParticleInRectangle(containerX2Y1Z2, d_sphParticleVec,
sphParticleX2Y1Z2);
      reqX2Y1Z2[0] = boostWorld.isend(rankX2Y1Z2, mpiTag,  sphParticleX2Y1Z2);
      reqX2Y1Z2[1] = boostWorld.irecv(rankX2Y1Z2, mpiTag, rsphParticleX2Y1Z2);
    }
    if (rankX2Y2Z1 >= 0) { // edge x2y2z1
      Rectangle containerX2Y2Z1(v2.x(), v2.y(), v1.z() - segZ,
        v2.x() + segX, v2.y() + segY, v1.z());
      findSPHParticleInRectangle(containerX2Y2Z1, d_sphParticleVec,
sphParticleX2Y2Z1);
      reqX2Y2Z1[0] = boostWorld.isend(rankX2Y2Z1, mpiTag,  sphParticleX2Y2Z1);
      reqX2Y2Z1[1] = boostWorld.irecv(rankX2Y2Z1, mpiTag, rsphParticleX2Y2Z1);
    }
    if (rankX2Y2Z2 >= 0) { // edge x2y2z2
      Rectangle containerX2Y2Z2(v2.x(), v2.y(), v2.z(),
        v2.x() + segX, v2.y() + segY, v2.z() + segZ);
      findSPHParticleInRectangle(containerX2Y2Z2, d_sphParticleVec,
sphParticleX2Y2Z2);
      reqX2Y2Z2[0] = boostWorld.isend(rankX2Y2Z2, mpiTag,  sphParticleX2Y2Z2);
      reqX2Y2Z2[1] = boostWorld.irecv(rankX2Y2Z2, mpiTag, rsphParticleX2Y2Z2);
    }
    // 6 surfaces
    if (rankX1 >= 0) boost::mpi::wait_all(reqX1, reqX1 + 2);
    if (rankX2 >= 0) boost::mpi::wait_all(reqX2, reqX2 + 2);
    if (rankY1 >= 0) boost::mpi::wait_all(reqY1, reqY1 + 2);
    if (rankY2 >= 0) boost::mpi::wait_all(reqY2, reqY2 + 2);
    if (rankZ1 >= 0) boost::mpi::wait_all(reqZ1, reqZ1 + 2);
    if (rankZ2 >= 0) boost::mpi::wait_all(reqZ2, reqZ2 + 2);
    // 12 edges
    if (rankX1Y1 >= 0) boost::mpi::wait_all(reqX1Y1, reqX1Y1 + 2);
    if (rankX1Y2 >= 0) boost::mpi::wait_all(reqX1Y2, reqX1Y2 + 2);
    if (rankX1Z1 >= 0) boost::mpi::wait_all(reqX1Z1, reqX1Z1 + 2);
    if (rankX1Z2 >= 0) boost::mpi::wait_all(reqX1Z2, reqX1Z2 + 2);
    if (rankX2Y1 >= 0) boost::mpi::wait_all(reqX2Y1, reqX2Y1 + 2);
    if (rankX2Y2 >= 0) boost::mpi::wait_all(reqX2Y2, reqX2Y2 + 2);
    if (rankX2Z1 >= 0) boost::mpi::wait_all(reqX2Z1, reqX2Z1 + 2);
    if (rankX2Z2 >= 0) boost::mpi::wait_all(reqX2Z2, reqX2Z2 + 2);
    if (rankY1Z1 >= 0) boost::mpi::wait_all(reqY1Z1, reqY1Z1 + 2);
    if (rankY1Z2 >= 0) boost::mpi::wait_all(reqY1Z2, reqY1Z2 + 2);
    if (rankY2Z1 >= 0) boost::mpi::wait_all(reqY2Z1, reqY2Z1 + 2);
    if (rankY2Z2 >= 0) boost::mpi::wait_all(reqY2Z2, reqY2Z2 + 2);
    // 8 vertices
    if (rankX1Y1Z1 >= 0) boost::mpi::wait_all(reqX1Y1Z1, reqX1Y1Z1 + 2);
    if (rankX1Y1Z2 >= 0) boost::mpi::wait_all(reqX1Y1Z2, reqX1Y1Z2 + 2);
    if (rankX1Y2Z1 >= 0) boost::mpi::wait_all(reqX1Y2Z1, reqX1Y2Z1 + 2);
    if (rankX1Y2Z2 >= 0) boost::mpi::wait_all(reqX1Y2Z2, reqX1Y2Z2 + 2);
    if (rankX2Y1Z1 >= 0) boost::mpi::wait_all(reqX2Y1Z1, reqX2Y1Z1 + 2);
    if (rankX2Y1Z2 >= 0) boost::mpi::wait_all(reqX2Y1Z2, reqX2Y1Z2 + 2);
    if (rankX2Y2Z1 >= 0) boost::mpi::wait_all(reqX2Y2Z1, reqX2Y2Z1 + 2);
    if (rankX2Y2Z2 >= 0) boost::mpi::wait_all(reqX2Y2Z2, reqX2Y2Z2 + 2);

    // delete outgoing particles
    removeSPHParticleOutRectangle();

    // add incoming particles
    d_recvSPHParticleVec.clear(); // new use of recvParticleVec
    // 6 surfaces
    if (rankX1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1.begin(), rsphParticleX1.end());
    if (rankX2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2.begin(), rsphParticleX2.end());
    if (rankY1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY1.begin(), rsphParticleY1.end());
    if (rankY2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY2.begin(), rsphParticleY2.end());
    if (rankZ1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleZ1.begin(), rsphParticleZ1.end());
    if (rankZ2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleZ2.begin(), rsphParticleZ2.end());
    // 12 edges
    if (rankX1Y1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y1.begin(), rsphParticleX1Y1.end());
    if (rankX1Y2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y2.begin(), rsphParticleX1Y2.end());
    if (rankX1Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Z1.begin(), rsphParticleX1Z1.end());
    if (rankX1Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Z2.begin(), rsphParticleX1Z2.end());
    if (rankX2Y1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y1.begin(), rsphParticleX2Y1.end());
    if (rankX2Y2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y2.begin(), rsphParticleX2Y2.end());
    if (rankX2Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Z1.begin(), rsphParticleX2Z1.end());
    if (rankX2Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Z2.begin(), rsphParticleX2Z2.end());
    if (rankY1Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY1Z1.begin(), rsphParticleY1Z1.end());
    if (rankY1Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY1Z2.begin(), rsphParticleY1Z2.end());
    if (rankY2Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY2Z1.begin(), rsphParticleY2Z1.end());
    if (rankY2Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleY2Z2.begin(), rsphParticleY2Z2.end());
    // 8 vertices
    if (rankX1Y1Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y1Z1.begin(), rsphParticleX1Y1Z1.end());
    if (rankX1Y1Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y1Z2.begin(), rsphParticleX1Y1Z2.end());
    if (rankX1Y2Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y2Z1.begin(), rsphParticleX1Y2Z1.end());
    if (rankX1Y2Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX1Y2Z2.begin(), rsphParticleX1Y2Z2.end());
    if (rankX2Y1Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y1Z1.begin(), rsphParticleX2Y1Z1.end());
    if (rankX2Y1Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y1Z2.begin(), rsphParticleX2Y1Z2.end());
    if (rankX2Y2Z1 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y2Z1.begin(), rsphParticleX2Y2Z1.end());
    if (rankX2Y2Z2 >= 0) d_recvSPHParticleVec.insert(d_recvSPHParticleVec.end(),
rsphParticleX2Y2Z2.begin(), rsphParticleX2Y2Z2.end());

    d_sphParticleVec.insert(d_sphParticleVec.end(),
d_recvSPHParticleVec.begin(), d_recvSPHParticleVec.end());

    #ifdef 0
      if (recvParticleVec.size() > 0) {
      debugInf << "iter=" << std::setw(8) << iteration << " rank=" <<
std::setw(2) << mpiRank
      << "   added=";
      for (std::vector<Particle*>::const_iterator it = recvParticleVec.begin();
it != recvParticleVec.end(); ++it)
      debugInf << std::setw(3) << (*it)->getId();
      debugInf << " now " << particleVec.size() << ": ";
      for (std::vector<Particle*>::const_iterator it = particleVec.begin(); it
!= particleVec.end(); ++it)
      debugInf << std::setw(3) << (*it)->getId();
      debugInf << std::endl;
      }
    #endif

    // do not release memory of received particles because they are part of and
managed by particleVec
    // 6 surfaces
    rsphParticleX1.clear();
    rsphParticleX2.clear();
    rsphParticleY1.clear();
    rsphParticleY2.clear();
    rsphParticleZ1.clear();
    rsphParticleZ2.clear();
    // 12 edges
    rsphParticleX1Y1.clear();
    rsphParticleX1Y2.clear();
    rsphParticleX1Z1.clear();
    rsphParticleX1Z2.clear();
    rsphParticleX2Y1.clear();
    rsphParticleX2Y2.clear();
    rsphParticleX2Z1.clear();
    rsphParticleX2Z2.clear();
    rsphParticleY1Z1.clear();
    rsphParticleY1Z2.clear();
    rsphParticleY2Z1.clear();
    rsphParticleY2Z2.clear();
    // 8 vertices
    rsphParticleX1Y1Z1.clear();
    rsphParticleX1Y1Z2.clear();
    rsphParticleX1Y2Z1.clear();
    rsphParticleX1Y2Z2.clear();
    rsphParticleX2Y1Z1.clear();
    rsphParticleX2Y1Z2.clear();
    rsphParticleX2Y2Z1.clear();
    rsphParticleX2Y2Z2.clear();

    d_recvSPHParticleVec.clear();
  }

  void SmoothParticleHydro::gatherParticle() {
    // before send, SPHParticle.demParticle should be NULL
    for(std::vector<Particle*>::iterator it=particleVec.begin();
it!=particleVec.end(); it++)
  (*it)->setNULLDemParticleInSPHParticle();  // at this point,
SPHGhostParticle.demParticle is not pointing to particleVec

    // update allParticleVec: process 0 collects all updated particles from each
other process
    if (mpiRank != 0) {// each process except 0
      boostWorld.send(0, mpiTag, particleVec);
    }
    else { // process 0
      // allParticleVec is cleared before filling with new data
      releaseGatheredParticle();

      // duplicate particleVec so that it is not destroyed by allParticleVec in
next iteration,
      // otherwise it causes memory error.
      std::vector<Particle*> dupParticleVec(particleVec.size());
      for (std::size_t i = 0; i < dupParticleVec.size(); ++i){
  dupParticleVec[i] = new Particle(*particleVec[i]);  // at this point,
dupParticleVec and particleVec are pointint to the same SPHGhoastParticle
  dupParticleVec[i]->SPHGhostParticleVec.clear();    // at this point,
dupParticleVec is pointing to nothing
  for(std::vector<sph::SPHParticle*>::iterator
st=particleVec[i]->SPHGhostParticleVec.begin();
st!=particleVec[i]->SPHGhostParticleVec.end(); st++){
    sph::SPHParticle* tmp_sph = new sph::SPHParticle(**st);  // create a new
SPHGhost particle, which is the same as the one in particleVec
    dupParticleVec[i]->SPHGhostParticleVec.push_back(tmp_sph);  // now
dupParticleVec points to the new SPHGhostParticle
  }
      }

      // fill allParticleVec with dupParticleVec and received particles
      allParticleVec.insert(allParticleVec.end(), dupParticleVec.begin(),
dupParticleVec.end());

      std::vector<Particle*> tmpParticleVec;
      long gatherRam = 0;
      for (int iRank = 1; iRank < mpiSize; ++iRank) {

  tmpParticleVec.clear();// do not destroy particles!
  boostWorld.recv(iRank, mpiTag, tmpParticleVec);
  allParticleVec.insert(allParticleVec.end(), tmpParticleVec.begin(),
tmpParticleVec.end());
  gatherRam += tmpParticleVec.size();

      }
      //debugInf << "gather: particleNum = " << gatherRam <<  " particleRam = "
<< gatherRam * sizeof(Particle) << std::endl;
    }
    // after receive, set SPHParticle.demParticle
    for(std::vector<Particle*>::iterator it=particleVec.begin();
it!=particleVec.end(); it++){
  (*it)->setDemParticleInSPHParticle();
    }
  }


  void SmoothParticleHydro::releaseGatheredParticle() {
    // clear allParticleVec, avoid long time memory footprint.
    for (std::vector<Particle*>::iterator it = allParticleVec.begin(); it !=
allParticleVec.end(); ++it){
      for(std::vector<sph::SPHParticle*>::iterator
st=(*it)->SPHGhostParticleVec.begin(); st!=(*it)->SPHGhostParticleVec.end();
++st){
  delete (*st);  // this is important to free the memories of sph ghost
particles
      }
      (*it)->SPHGhostParticleVec.clear();
      std::vector<sph::SPHParticle*>().swap((*it)->SPHGhostParticleVec); //
actual memory release
      delete (*it);
    }
    allParticleVec.clear();
    std::vector<Particle*>().swap(allParticleVec); // actual memory release
  }


  void SmoothParticleHydro::gatherSPHParticle() {
    // update d_allSPHParticleVec: process 0 collects all updated particles from
each other process
    if (mpiRank != 0) {// each process except 0
      boostWorld.send(0, mpiTag, d_sphParticleVec);
    }
    else { // process 0
      // d_allSPHParticleVec is cleared before filling with new data
      releaseGatheredSPHParticle();

      // duplicate d_sphParticleVec so that it is not destroyed by
d_allSPHParticleVec in next iteration,
      // otherwise it causes memory error.
      std::vector<sph::SPHParticle*> dupSPHParticleVec(d_sphParticleVec.size());
      for (std::size_t i = 0; i < dupSPHParticleVec.size(); ++i)
  dupSPHParticleVec[i] = new sph::SPHParticle(*d_sphParticleVec[i]);

      // fill allParticleVec with dupParticleVec and received particles
      d_allSPHParticleVec.insert(d_allSPHParticleVec.end(),
dupSPHParticleVec.begin(), dupSPHParticleVec.end());

      std::vector<sph::SPHParticle*> tmpSPHParticleVec;
      long gatherRam = 0;
      for (int iRank = 1; iRank < mpiSize; ++iRank) {

  tmpSPHParticleVec.clear();// do not destroy particles!
  boostWorld.recv(iRank, mpiTag, tmpSPHParticleVec);
  d_allSPHParticleVec.insert(d_allSPHParticleVec.end(),
tmpSPHParticleVec.begin(), tmpSPHParticleVec.end());
  gatherRam += tmpSPHParticleVec.size();

      }
      //debugInf << "gather: particleNum = " << gatherRam <<  " particleRam = "
<< gatherRam * sizeof(Particle) << std::endl;
    }
  }

  void SmoothParticleHydro::releaseGatheredSPHParticle() {
    // clear allParticleVec, avoid long time memory footprint.
    for (std::vector<sph::SPHParticle*>::iterator it =
d_allSPHParticleVec.begin(); it != d_allSPHParticleVec.end(); ++it)
      delete (*it);
    d_allSPHParticleVec.clear();
    std::vector<sph::SPHParticle*>().swap(d_allSPHParticleVec); // actual memory
release
  }

  void SmoothParticleHydro::gatherBdryContact() {
    if (isBdryProcess()) {
      if (mpiRank != 0)
  boostWorld.send(0, mpiTag, boundaryVec);
    }

    if (mpiRank == 0) {
      mergeBoundaryVec.clear();
      std::vector<Boundary*>().swap(mergeBoundaryVec); // actual memory release
      mergeBoundaryVec = boundaryVec;

      std::vector<Boundary*> tmpBoundaryVec;
      for (std::size_t it = 0; it < bdryProcess.size(); ++it) {
  if (bdryProcess[it] != 0) {// not root process
    tmpBoundaryVec.clear();  // do not destroy particles!
    boostWorld.recv(bdryProcess[it], mpiTag, tmpBoundaryVec);
    // merge tmpBoundaryVec into mergeBoundaryVec
    assert(tmpBoundaryVec.size() == mergeBoundaryVec.size());
    for (std::size_t jt = 0; jt < tmpBoundaryVec.size(); ++jt)
      mergeBoundaryVec[jt]->getContactInfo().insert(   \
                mergeBoundaryVec[jt]->getContactInfo().end(), \
                tmpBoundaryVec[jt]->getContactInfo().begin(), \
                tmpBoundaryVec[jt]->getContactInfo().end() );
  }
      }

      // must update after collecting all boundary contact info
      for(std::vector<Boundary*>::iterator it = mergeBoundaryVec.begin(); it !=
mergeBoundaryVec.end(); ++it)
  (*it)->updateStatForce();
    }
  }


  void  SmoothParticleHydro::openSPHTecplot(std::ofstream &ofs, const char *str)
{
    ofs.open(str);
    if(!ofs) { debugInf << "stream error: openSPHTecplot" << std::endl;
exit(-1); }
    ofs.setf(std::ios::scientific, std::ios::floatfield);
    ofs.precision(OPREC);

    ofs << "Title = \"SPH Particle Information\"" << std::endl;
    ofs << "VARIABLES = \"x\", \"y\",\"z\" \"Ux\" \"Uy\" \"Uz\" \"Vx\" \"Vy\"
\"Vz\" \"Pressure\" \"a_x\" \"a_y\" \"a_z\" \"density_dot\" \"density\" "
  << std::endl;
  }

  void SmoothParticleHydro::printSPHTecplot(std::ofstream &ofs, int iframe) {
  ofs << "ZONE T =\" " << iframe << "-th Load Step\" "<< std::endl;
  // Output the coordinates and the array information
  for(std::vector<sph::SPHParticle*>::iterator pt = d_allSPHParticleVec.begin();
pt!= d_allSPHParticleVec.end(); pt++) {

//// not print the most right layer of SPH free particles, 2015/05/19
//if((*pt)->getInitPosition().getx()==25){
//continue;
//}
//      if((*pt)->getType()==3) continue;  // not print out boundary particles

      ofs << std::setw(20) << (*pt)->getCurrPosition().x()
    << std::setw(20) << (*pt)->getCurrPosition().y()
    << std::setw(20) << (*pt)->getCurrPosition().z()
    << std::setw(20) << (*pt)->getDisplacement().x()
    << std::setw(20) << (*pt)->getDisplacement().y()
    << std::setw(20) << (*pt)->getDisplacement().z()
    << std::setw(20) << (*pt)->getVelocity().x()
    << std::setw(20) << (*pt)->getVelocity().y()
    << std::setw(20) << (*pt)->getVelocity().z();

      (*pt)->calculateParticlePressure();
       ofs << std::setw(20) <<(*pt)->getParticlePressure();

      ofs << std::setw(20) << (*pt)->getVelocityDot().x()
    << std::setw(20) << (*pt)->getVelocityDot().y()
    << std::setw(20) << (*pt)->getVelocityDot().z()
    << std::setw(20) << (*pt)->getDensityDot()
    << std::setw(20) << (*pt)->getParticleDensity() << std::endl;

  }
  }
  void SmoothParticleHydro::printSPHParticle(const char *str) const{
      std::ofstream ofs(str);
      if(!ofs) { debugInf << "stream error: printSPHParticle" << std::endl;
exit(-1); }
      ofs.setf(std::ios::scientific, std::ios::floatfield);
      ofs.precision(OPREC);

        ofs << "Title = \"SPH Particle Information\"" << std::endl;
        ofs << "VARIABLES = \"x\", \"y\",\"z\" \"Ux\" \"Uy\" \"Uz\" \"Vx\"
\"Vy\" \"Vz\" \"Pressure\" \"a_x\" \"a_y\" \"a_z\" \"density_dot\" \"density\" "
      << std::endl;

  // Output the coordinates and the array information
  for(std::vector<sph::SPHParticle*>::const_iterator pt =
d_allSPHParticleVec.begin(); pt!= d_allSPHParticleVec.end(); pt++) {

//// not print the most right layer of SPH free particles, 2015/05/19
//if((*pt)->getInitPosition().getx()==25){
//continue;
//}
      if((*pt)->getType()==3) continue;  // not print out boundary particles

      ofs << std::setw(20) << (*pt)->getCurrPosition().x()
    << std::setw(20) << (*pt)->getCurrPosition().y()
    << std::setw(20) << (*pt)->getCurrPosition().z()
    << std::setw(20) << (*pt)->getDisplacement().x()
    << std::setw(20) << (*pt)->getDisplacement().y()
    << std::setw(20) << (*pt)->getDisplacement().z()
    << std::setw(20) << (*pt)->getVelocity().x()
    << std::setw(20) << (*pt)->getVelocity().y()
    << std::setw(20) << (*pt)->getVelocity().z();

      (*pt)->calculateParticlePressure();
       ofs << std::setw(20) <<(*pt)->getParticlePressure();

      ofs << std::setw(20) << (*pt)->getVelocityDot().x()
    << std::setw(20) << (*pt)->getVelocityDot().y()
    << std::setw(20) << (*pt)->getVelocityDot().z()
    << std::setw(20) << (*pt)->getDensityDot()
    << std::setw(20) << (*pt)->getParticleDensity() << std::endl;

  }

    ofs.close();
  }

    void SmoothParticleHydro::initialSPHVelocity2D(){
  commuParticle();
  commuSPHParticle();  // this will update container and mergeSPHParticleVec,
both are needed for divideSPHDomain
  calculateSPHDensityDotVelocityDotLinkedList2D();  // calculate velocityDot and
densityDot
  // fix the y for all SPH points
  for(std::vector<sph::SPHParticle*>::iterator pt=d_sphParticleVec.begin();
pt!=d_sphParticleVec.end(); pt++){
      switch((*pt)->getType()){
        case 1: // free sph particle
    (*pt)->fixY();
    break;
        case 2: // ghost sph particle
    break;
        case 3: // boundary sph particle
    (*pt)->fixXYZ();
    break;
        default:
    std::cout << "SPH particle type of pta should be 1, 2 or 3!" << std::endl;
    exit(-1);
      } // switch
  }
  initialSPHLeapFrogVelocity();  // initial velocity only for free SPH particles
based on equation (4.3)

        releaseRecvParticle();
        releaseRecvSPHParticle();
    } // initialSPHVelocity()

    void SmoothParticleHydro::initialSPHVelocity3D(){
  commuParticle();
  commuSPHParticle();  // this will update container and mergeSPHParticleVec,
both are needed for divideSPHDomain
  calculateSPHDensityDotVelocityDotLinkedList3D();  // calculate velocityDot and
densityDot
  initialSPHLeapFrogVelocity();  // initial velocity only for free SPH particles
based on equation (4.3)

        releaseRecvParticle();
        releaseRecvSPHParticle();
    } // initialSPHVelocity()

    void SmoothParticleHydro::initialSPHVelocityCopyDEM3D(){
  commuSPHParticle();  // this will update container and mergeSPHParticleVec,
both are needed for divideSPHDomain
  calculateSPHDensityDotVelocityDotLinkedList3D();  // calculate velocityDot and
densityDot
  initialSPHLeapFrogVelocity();  // initial velocity only for free SPH particles
based on equation (4.3)

        releaseRecvSPHParticle();
    } // initialSPHVelocity()


    void SmoothParticleHydro::initialSPHLeapFrogVelocity(){  // initial particle
velocity only for free SPH particles based on equation (4.3)
  for(std::vector<sph::SPHParticle*>::iterator pt=d_sphParticleVec.begin();
pt!=d_sphParticleVec.end(); pt++){
      if((*pt)->getType() == 1) (*pt)->initialParticleVelocityLeapFrog();
  }
    } // end initialSPHLeapFrogVelocity

    void SmoothParticleHydro::updateSPHLeapFrogPositionDensity(){  // update
particle position and density based on equation (4.1)
  for(std::vector<sph::SPHParticle*>::iterator pt=d_sphParticleVec.begin();
pt!=d_sphParticleVec.end(); pt++){
      switch((*pt)->getType()){
        case 1: // free sph particle
    (*pt)->updateParticlePositionDensityLeapFrog();
    break;
        case 2: // ghost sph particle
    break;
        case 3: // boundary sph particle
    (*pt)->updateParticleDensity();
    break;
        default:
    std::cout << "SPH particle type of pta should be 1, 2 or 3!" << std::endl;
    exit(-1);
      } // switch
  }

  std::vector<sph::SPHParticle*>::iterator gt;
  for(std::vector<Particle*>::iterator demt=particleVec.begin();
demt!=particleVec.end(); demt++){
      (*demt)->updateSPHGhostParticle();  // update position and density by dem
particle and sph particle density, and velocity
  }
    } // end updateSPHLeapFrogPositionDensity


    void SmoothParticleHydro::updateSPHLeapFrogVelocity(){  // update particle
velocity only for free SPH particles based on equation (4.2)
  for(std::vector<sph::SPHParticle*>::iterator pt=d_sphParticleVec.begin();
pt!=d_sphParticleVec.end(); pt++){
      if((*pt)->getType()==1) (*pt)->updateParticleVelocityLeapFrog();
  }
    } // end updateSPHLeapFrogVelocity


//  REAL factor = 1.0/(120.0*dem::PI*h*h*h);  // 3D quintic kernel factor
//  REAL factor = 7.0/(478.0*dem::PI*h*h);    // 2D quintic kernel factor
    // kernel function, Vec is the position of a b, h is smoothing length
    inline REAL SmoothParticleHydro::kernelFunction(const dem::Vec& a, const
dem::Vec& b){
  REAL rab = dem::vfabs(a-b);
  REAL s = rab*one_devide_h;
  REAL item1_5 = (3-s)*(3-s)*(3-s)*(3-s)*(3-s);  // (3-s)^5
  REAL item2_5 = (2-s)*(2-s)*(2-s)*(2-s)*(2-s);   // (2-s)^5
  REAL item3_5 = (1-s)*(1-s)*(1-s)*(1-s)*(1-s);   // (1-s)^5
  if(s<1){
      return factor_kernel*(item1_5-6*item2_5+15*item3_5);
  }
  else if(s<2){
      return factor_kernel*(item1_5-6*item2_5);
  }
  else if(s<3){
      return factor_kernel*item1_5;
  }
  else{
      return 0.0;
  }

    } // end kernelFunction

    // kernel function, s is rab/h, h is smoothing length
    inline REAL SmoothParticleHydro::kernelFunction(REAL s){
  REAL item1_5 = (3-s)*(3-s)*(3-s)*(3-s)*(3-s);  // (3-s)^5
  REAL item2_5 = (2-s)*(2-s)*(2-s)*(2-s)*(2-s);   // (2-s)^5
  REAL item3_5 = (1-s)*(1-s)*(1-s)*(1-s)*(1-s);   // (1-s)^5
  if(s<1){
      return factor_kernel*(item1_5-6*item2_5+15*item3_5);
  }
  else if(s<2){
      return factor_kernel*(item1_5-6*item2_5);
  }
  else if(s<3){
      return factor_kernel*item1_5;
  }
  else{
      return 0.0;
  }

    } // end kernelFunction

//  REAL factor = 1.0/(120.0*dem::PI*h*h*h*h);  // 3D quintic kernel factor
//  REAL factor = 1.0/(120.0*h*h);  // 1D quintic kernel factor
//  REAL factor = 7.0/(478.0*dem::PI*h*h*h);  // 2D quintic kernel factor
    // to calculate delta_aWab, where a is the position of the first particle
    inline dem::Vec SmoothParticleHydro::gradientKernelFunction(const dem::Vec&
a, const dem::Vec& b){
  REAL rab = dem::vfabs(a-b);
  REAL s = rab*one_devide_h;
  REAL item1_4 = (3-s)*(3-s)*(3-s)*(3-s);  // (3-s)^4
  REAL item2_4 = (2-s)*(2-s)*(2-s)*(2-s); // (2-s)^4
  REAL item3_4 = (1-s)*(1-s)*(1-s)*(1-s); // (1-s)^4
  if(s<1){
      return factor_kernel_gradient
*(-5*item1_4+30*item2_4-75*item3_4)*(a-b)/rab;  // it is strange that s is
devided here. compare to Liu's SPH code (National University of Singapore)
  }
  else if(s<2){
      return factor_kernel_gradient *(-5*item1_4+30*item2_4)*(a-b)/rab;
  }
  else if(s<3){
      return factor_kernel_gradient *(-5*item1_4)*(a-b)/rab;
  }
  else{
      return dem::Vec(0.0);
  }

    } // end gradientKernelFunction

    // to calculate partial differential dWab_dra
    inline REAL SmoothParticleHydro::partialKernelFunction(const dem::Vec& a,
const dem::Vec& b){
  REAL rab = dem::vfabs(a-b);
  REAL s = rab*one_devide_h;
  REAL item1_4 = (3-s)*(3-s)*(3-s)*(3-s);  // (3-s)^4
  REAL item2_4 = (2-s)*(2-s)*(2-s)*(2-s); // (2-s)^4
  REAL item3_4 = (1-s)*(1-s)*(1-s)*(1-s); // (1-s)^4
  if(s<1){
      return factor_kernel_gradient *(-5*item1_4+30*item2_4-75*item3_4);
  }
  else if(s<2){
      return factor_kernel_gradient *(-5*item1_4+30*item2_4);
  }
  else if(s<3){
      return factor_kernel_gradient *(-5*item1_4);
  }
  else{
      return 0.0;
  }

    } // end partialKernelFunction


    // divide SPH domain in each cpu into different cells in 2D in xz plane.
    // see the notes 5/20/2015 and 5/21/2015 or Simpson's paper "Numerical
techniques for three-dimensional Smoothed Particle Hydrodynamics"
    void SmoothParticleHydro::divideSPHDomain2D(){

  // clear the std::vector< std::vector<sph::SPHParticle*> > SPHParticleCellVec
  for(int pvec=0; pvec<SPHParticleCellVec.size(); pvec++){
      SPHParticleCellVec[pvec].clear();
  }
  SPHParticleCellVec.clear();

      Vec  vmin = container.getMinCorner();
      Vec  vmax = container.getMaxCorner();
  REAL small_value = 0.01*spaceInterval;
      REAL xmin = vmin.x()-sphCellSize-small_value; REAL zmin =
vmin.z()-sphCellSize-small_value;  // expand the container by sphCellSize for
cells domain is necessary
      REAL xmax = vmax.x()+sphCellSize+small_value; REAL zmax =
vmax.z()+sphCellSize+small_value; // since the sph domain that we divide is
mergeSPHParticleVec
  Nx = (xmax-xmin)/kernelSize+1;  // (xmax-xmin)/(3h)+1
  Nz = (zmax-zmin)/kernelSize+1;  // (zmax-zmin)/(3h)+1
  numCell = Nx*Nz;
  SPHParticleCellVec.resize(numCell);  // at this point, the cellVec contains
numCell vectors,
            // each vector is empty but ready to store the pointer of SPH
particles (this process will be in the calculation of SPH forces)

  dem::Vec tmp_xyz;
  int num;
  for(std::vector<sph::SPHParticle*>::iterator pt=mergeSPHParticleVec.begin();
pt!=mergeSPHParticleVec.end(); pt++){ // mergeSPHParticle contains also the sph
particles from neighboring cpus
      tmp_xyz = (*pt)->getCurrPosition();
      num = int( (tmp_xyz.z()-zmin)/kernelSize )*Nx + int(
(tmp_xyz.x()-xmin)/kernelSize );
      SPHParticleCellVec[num].push_back(*pt);
  }

  REAL maxRadius = gradation.getPtclMaxRadius();
  std::vector<sph::SPHParticle*>::iterator gt;
  for(std::vector<Particle*>::iterator pdt=particleVec.begin();
pdt!=particleVec.end(); ++pdt){  // the sph ghost particles in the current cpu's
dem particles
                          // will be definitely inside the [xmin, ymin, zmin,
xmax ...]
      tmp_xyz = (*pdt)->currentPosition();
      if(tmp_xyz.x()>=xmax+maxRadius || tmp_xyz.z()>=zmax+maxRadius
      || tmp_xyz.x()<=xmin-maxRadius || tmp_xyz.z()<=zmin-maxRadius )  // dem
particle is outside of the
    continue;
      for(gt=(*pdt)->SPHGhostParticleVec.begin();
gt!=(*pdt)->SPHGhostParticleVec.end(); gt++){  // set all SPH ghost particles
into their cells
        tmp_xyz = (*gt)->getCurrPosition();
    if(tmp_xyz.x()>=xmax || tmp_xyz.z()>=zmax
    || tmp_xyz.x()<=xmin || tmp_xyz.z()<=zmin )
       continue;  // this sph ghost particle is outside of the container, go to
next sph ghost particle
        num = int( (tmp_xyz.z()-zmin)/kernelSize )*Nx + int(
(tmp_xyz.x()-xmin)/kernelSize );
        SPHParticleCellVec[num].push_back(*gt);
      }
  }

  for(std::vector<Particle*>::iterator pdt=recvParticleVec.begin();
pdt!=recvParticleVec.end(); pdt++){  // the this time, recvParticleVec are the
dem particles that are communicated by the commuParticle
      tmp_xyz = (*pdt)->currentPosition();
      if(tmp_xyz.x()>=xmax+maxRadius || tmp_xyz.z()>=zmax+maxRadius
      || tmp_xyz.x()<=xmin-maxRadius || tmp_xyz.z()<=zmin-maxRadius )  // dem
particle is outside of the
    continue;                        // expanded domain

      for(gt=(*pdt)->SPHGhostParticleVec.begin();
gt!=(*pdt)->SPHGhostParticleVec.end(); gt++){  // set part SPH ghost particles
into their cells
        tmp_xyz = (*gt)->getCurrPosition();
    // not all sph ghost particles in these received particles are inside the
container
    if(tmp_xyz.x()>=xmax || tmp_xyz.z()>=zmax
    || tmp_xyz.x()<=xmin || tmp_xyz.z()<=zmin )
       continue;  // this sph ghost particle is outside of the container, go to
next sph ghost particle
        num = int( (tmp_xyz.z()-zmin)/kernelSize )*Nx + int(
(tmp_xyz.x()-xmin)/kernelSize );
        SPHParticleCellVec[num].push_back(*gt);
      }
  }

    } // divideSPHDomain2D

    //// the momentum equilibrium equation and state equation are implemented as
in Monaghan's paper (1994), simulate free surface flow using sph
    //// here the neighboring list of SPH particles is searched by the cells,
vector< vector<sph::SPHParticle*> > SPHParticleCellVec;
    void SmoothParticleHydro::calculateSPHDensityDotVelocityDotLinkedList2D(){

  // divide the SPH domain into different cells, each cell will contain SPH
particles within it
  divideSPHDomain2D();

//  checkDivision();  // pass, May 22, 2015
//  checkNeighborCells2D();  // pass, May 22, 2015

  // initialize the densityDot and velocityDot of all the SPH particles
  dem::Vec tmp_vec = dem::Vec(0,0,
-(util::getParam<REAL>("gravAccel"])*(util::getParam<REAL>("gravScale"]) );
  dem::Vec zero_vec = dem::Vec(0,0,0);
  // in the calculation of forces between sph particles, we need to use the
mergeSPHParticleVec,
  // since mergeVec contains the sph particles from neighboring cpus. While we
can only update and migrate and communicate d_sphParticleVec
  for(std::vector<sph::SPHParticle*>::iterator pt=mergeSPHParticleVec.begin();
pt!=mergeSPHParticleVec.end(); pt++){
      (*pt)->setDensityDotVelocityDotZero();
      (*pt)->calculateParticleViscosity();  // everytime when use
getParticleViscolisity(), make sure that pressure has been calculated!!!!
      (*pt)->calculateParticlePressure();    // everytime when use
getParticlePressure(), make sure that pressure has been calculated!!!!
      switch((*pt)->getType()){
        case 1: // free sph particle
    (*pt)->addVelocityDot(tmp_vec);
    break;
        case 2: // ghost sph particle
    break;
        case 3: // boundary sph particle
    (*pt)->addVelocityDot(zero_vec);
    break;
        default:
    std::cout << "SPH particle type of pta should be 1, 2 or 3!" << std::endl;
    exit(-1);
      } // switch
  }

  std::vector<sph::SPHParticle*>::iterator gt;
  for(std::vector<Particle*>::iterator pdt=mergeParticleVec.begin();
pdt!=mergeParticleVec.end(); pdt++){  // all sph ghost particles
      for(gt=(*pdt)->SPHGhostParticleVec.begin();
gt!=(*pdt)->SPHGhostParticleVec.end(); gt++){
        (*gt)->setDensityDotVelocityDotZero();
        (*gt)->calculateParticleViscosity();  // everytime when use
getParticleViscolisity(), make sure that pressure has been calculated!!!!
        (*gt)->calculateParticlePressure();  // everytime when use
getParticlePressure(), make sure that pressure has been calculated!!!!
      }
  }

  // temporary variables used in the loop
  dem::Vec pta_position;
  dem::Vec ptb_position;
  dem::Vec delta_aWab;
  dem::Vec delta_bWba;
  dem::Vec vab;
  dem::Vec vba;
  dem::Vec vdem;
  dem::Vec dva_dt;
  dem::Vec dvb_dt;
  dem::Vec delta_a;
  dem::Vec delta_b;
  REAL pa, pb, rhoa, rhob, mua, mub;
  REAL rab;
        REAL dWab_dra;
  REAL dWba_drb;
  REAL Wab, Wba;
  REAL da, dB;
  REAL beta;
  REAL xa, ya, xB, yB, k, sqrt_xaya;  // variables for Morris' method to
calculate da/dB
  std::vector<sph::SPHParticle*>::iterator ptb;
  REAL ra, rb, rc;  // the geometry of the dem particle
  dem::Vec pta_local, ptb_local;  // local position of pta and ptb in the dem
particle
  dem::Vec demt_curr;
  REAL Gamma_ab, mu_ab, vr_dot;
  REAL alpha = util::getParam<REAL>("alpha"];
    REAL alpha_zero = 0;  // the viscous between free and ghost/boundary
  REAL epsilon = util::getParam<REAL>("epsilon"];  // parameter for velocity
correction
  REAL Wq, Ra, Rb, phi_4, coefficient_a, coefficient_b;
  dem::Particle* demt;

        int pnum;
  std::vector<sph::SPHParticle*> tmp_particleVec;  // sph particles in
neighboring cells
  for(int pvec=0; pvec<SPHParticleCellVec.size(); ++pvec){

      // store all SPH particles in pvec's neighboring cells
      tmp_particleVec.clear();  // it is the same for the particles in the same
cell
      if(pvec+1<numCell){
    pnum = pvec+1;
            for(std::vector<sph::SPHParticle*>::iterator pt =
SPHParticleCellVec[pnum].begin(); pt!= SPHParticleCellVec[pnum].end(); pt++) {
        tmp_particleVec.push_back(*pt);
            }
      }
      if(pvec+Nx-1<numCell){
    pnum = pvec+Nx-1;
            for(std::vector<sph::SPHParticle*>::iterator pt =
SPHParticleCellVec[pnum].begin(); pt!= SPHParticleCellVec[pnum].end(); pt++) {
        tmp_particleVec.push_back(*pt);
            }
      }
      if(pvec+Nx<numCell){
    pnum = pvec+Nx;
            for(std::vector<sph::SPHParticle*>::iterator pt =
SPHParticleCellVec[pnum].begin(); pt!= SPHParticleCellVec[pnum].end(); pt++) {
        tmp_particleVec.push_back(*pt);
            }
      }
      if(pvec+Nx+1<numCell){
    pnum = pvec+Nx+1;
            for(std::vector<sph::SPHParticle*>::iterator pt =
SPHParticleCellVec[pnum].begin(); pt!= SPHParticleCellVec[pnum].end(); pt++) {
        tmp_particleVec.push_back(*pt);
            }
      }

      for(std::vector<sph::SPHParticle*>::iterator
pta=SPHParticleCellVec[pvec].begin(); pta!=SPHParticleCellVec[pvec].end();
++pta){  // SPH particles in cell pvec
    pa = (*pta)->getParticlePressure();
        if(pa>=0){
        Ra = 0.006;
        }
        else{
        Ra = 0.6;
        }
//        mua = (*pta)->getParticleViscosity();
        rhoa = (*pta)->getParticleDensity();
        pta_position = (*pta)->getCurrPosition();

//    if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta is
ghost or boundary particles
//        continue;  // do not consider pta, as we treat before
//    }

    for(ptb=pta+1; ptb!=SPHParticleCellVec[pvec].end(); ++ptb){  // sum over the
SPH particles in the same cell pvec
        ptb_position = (*ptb)->getCurrPosition();
        rab = dem::vfabs(pta_position-ptb_position);
        if(rab<=kernelSize){  // ptb is in the smooth kernel
          pb = (*ptb)->getParticlePressure();
//          mub = (*ptb)->getParticleViscosity();
          rhob = (*ptb)->getParticleDensity();

          Wq = kernelFunction(rab/smoothLength);
          phi_4 = pow(Wq/Wqmin,4);
          if(pb>=0){
          Rb = 0.006;
          }
          else{
          Rb = 0.6;
          }
          coefficient_a = 1+Ra*phi_4;
          coefficient_b = 1+Rb*phi_4;

      // we have three types of SPH particles: 1, free particle; 2, ghost
particle; 3, boundary particle
      // Then we have 3x3 = 9 different types of interactions with the three
types of particles
      // so we cannot judge the interaction type only by the type of particle
ptb, we need to consider pta also
      // pta          ptb          need to consider or not
      // 1            1              V      free with free
      // 1            2              V      free with ghost
      // 1            3              V      free with boundary

      // 2            1              V      ghost with free
      // 2            2              X      ghost with ghost
      // 2            3              X      ghost with boundary

      // 3            1              V      boundary with free
      // 3            2              X      boundary with ghost
      // 3            3              X       boundary with boundary


      // add the density dot for pta and ptb
          delta_aWab = gradientKernelFunction(pta_position, ptb_position);  //
this is to add SPH pta
          delta_bWba = - delta_aWab;
          dWab_dra = partialKernelFunction(pta_position, ptb_position);  // this
is to add SPH pta
          dWba_drb = dWab_dra;
          Wab = kernelFunction(pta_position, ptb_position);  // this is to add
SPH pta
          Wba = Wab;

      switch((*ptb)->getType()){
        case 1:  // ptb is free SPH particle
          switch((*pta)->getType()){
            case 1:  // free with free
                vab = (*pta)->getVelocity()-(*ptb)->getVelocity();
               vba = -vab;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
             Gamma_ab = 0;
                }


                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
                (*pta)->addVelocityDot(dva_dt);
                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);

                delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
                (*pta)->addVelocityCorrection(delta_a);
                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                (*ptb)->addVelocityCorrection(delta_b);
              break;
            case 2:  // ghost with free
        demt = (*pta)->getDemParticle();
        demt_curr = demt->currentPosition();
            ptb_local = demt->globalToLocal(ptb_position-demt_curr);  // the
local position of sph point ptb
            ra = demt->getA(); rc = demt->getC();
            k =
1.0/(sqrt(ptb_local.x()*ptb_local.x()/(ra*ra)+ptb_local.z()*ptb_local.z()/(rc*rc)
) );
            da = dem::vfabs(ptb_local-k*ptb_local);  // the distance is the same
in rotated coordinates

                // calculate Vab as the method shown in Morris's paper, 1996

                // (1) here I wanna use the distance from the point a/b to the
surface of the ellipsoid to simplify the problem
            pta_local = (*pta)->getLocalPosition();
            k =
1.0/(sqrt(pta_local.x()*pta_local.x()/(ra*ra)+pta_local.z()*pta_local.z()/(rc*rc)
) );
                dB = dem::vfabs(pta_local-k*pta_local);

//              // (2) here the Morris's method is used
//              xa = pta_position.getx(); ya = pta_position.gety();
//              xB = ptb_position.getx(); yB = ptb_position.gety();
//              if(ya==0) {da = xa-radius; dB = radius-xB;}
//              else if(xa==0) {da = ya-radius; dB = radius-yB;}
//              else {
//          sqrt_xaya = sqrt(xa*xa+ya*ya);
//          k = radius/sqrt_xaya;
//          da = radius/k - radius;
//          dB = fabs(xa*xB+ya*yB-k*radius*radius)/sqrt_xaya;
//              }


                beta = 1+dB/da;
                if(beta>2.0 || beta<0 || isnan(beta)){ beta = 2.0; }

            vdem = (*pta)->getVelocity();
                vba = beta*((*ptb)->getVelocity()-vdem);
                vab = -vba;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
                mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                    Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
                 Gamma_ab = 0;
                }

                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
            demt->addForce((*pta)->getParticleMass()*dva_dt);
            demt->addMoment( (pta_position-demt_curr) %
((*pta)->getParticleMass()*dva_dt) );
//                (*pta)->addVelocityDot(dva_dt);

                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

//                  delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
//                  (*pta)->addVelocityCorrection(delta_a);
                  delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                  (*ptb)->addVelocityCorrection(delta_b);

        break;

            case 3:  // boundary with free

        // calculate Vab as the method shown in Morris's paper, 1996
                // interact with boundary particles
                da = ptb_position.z()-allContainer.getMinCorner().z();  //
assume with the bottom boundary
        dB = allContainer.getMinCorner().z()-pta_position.x();  // assume with
the bottom boundary
        if(pta_position.x()<allContainer.getMinCorner().x()){  // with left
boundary
            da = ptb_position.x()-allContainer.getMinCorner().x();
            dB = allContainer.getMinCorner().x()-pta_position.x();
        }
        else if(pta_position.x()>allContainer.getMaxCorner().x()){ // with right
boundary
            da = allContainer.getMaxCorner().x()-ptb_position.x();
            dB = pta_position.x()-allContainer.getMaxCorner().x();
        }
        else if(pta_position.z()>allContainer.getMaxCorner().z()){ // with top
boundary
            da = allContainer.getMaxCorner().z()-ptb_position.z();
            dB = pta_position.z()-allContainer.getMaxCorner().z();
        }

                beta = 1+dB/da;
                if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

                vba = beta*(*ptb)->getVelocity();
                vab = -vba;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
             Gamma_ab = 0;
                }

//                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
//                (*pta)->addVelocityDot(dva_dt);
                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)+pa/(rhoa*rhoa)+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

//            delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
//                (*pta)->addVelocityCorrection(delta_a);
                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                (*ptb)->addVelocityCorrection(delta_b);

//            // apply the boundary forces by Lennard-Jones potential as in
Monaghan's paper(1994)
//                if(rab<=spaceInterval){ // ptb is in the smooth kernel
//                dvb_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(ptb_position-pta_position)/(rab*rab);
//                (*ptb)->addVelocityDot(dvb_dt);
//                } // end if

        break;
            default:
            std::cout << "SPH particle type of pta should be 1, 2 or 3!" <<
std::endl;
            exit(-1);
            } // end switch pta

          break;
        case 2:  // ptb is ghost particle

          if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta
is ghost or boundary particles
            break;  // do not consider pta, as we treat before
          }
          demt = (*ptb)->getDemParticle();
          demt_curr = demt->currentPosition();
          pta_local = demt->globalToLocal(pta_position-demt_curr);  // the local
position of sph point pta
          ra = demt->getA(); rc = demt->getC();
          k =
1.0/(sqrt(pta_local.x()*pta_local.x()/(ra*ra)+pta_local.z()*pta_local.z()/(rc*rc)
) );
          da = dem::vfabs(pta_local-k*pta_local);  // the distance is the same
in rotated coordinates

              // calculate Vab as the method shown in Morris's paper, 1996

              // (1) here I wanna use the distance from the point a/b to the
surface of the ellipsoid to simplify the problem
          ptb_local = (*ptb)->getLocalPosition();
          k =
1.0/(sqrt(ptb_local.x()*ptb_local.x()/(ra*ra)+ptb_local.z()*ptb_local.z()/(rc*rc)
) );
              dB = dem::vfabs(ptb_local-k*ptb_local);

//              // (2) here the Morris's method is used
//              xa = pta_position.getx(); ya = pta_position.gety();
//              xB = ptb_position.getx(); yB = ptb_position.gety();
//              if(ya==0) {da = xa-radius; dB = radius-xB;}
//              else if(xa==0) {da = ya-radius; dB = radius-yB;}
//              else {
//          sqrt_xaya = sqrt(xa*xa+ya*ya);
//          k = radius/sqrt_xaya;
//          da = radius/k - radius;
//          dB = fabs(xa*xB+ya*yB-k*radius*radius)/sqrt_xaya;
//              }


              beta = 1+dB/da;
              if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

          vdem = (*ptb)->getVelocity();
              vab = beta*((*pta)->getVelocity()-vdem);
              vba = -vab;

              (*pta)->addDensityDot( (*ptb)->getParticleMass()*(vab*delta_aWab)
);
              (*ptb)->addDensityDot( (*pta)->getParticleMass()*(vba*delta_bWba)
);

              vr_dot = vab*(pta_position-ptb_position);
              if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
              }
              else{
             Gamma_ab = 0;
              }

              dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
              (*pta)->addVelocityDot(dva_dt);
              dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
          demt->addForce((*ptb)->getParticleMass()*dvb_dt);
          demt->addMoment( (ptb_position-demt_curr) %
((*ptb)->getParticleMass()*dvb_dt) );
//              (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

                delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
                (*pta)->addVelocityCorrection(delta_a);
//                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
//                (*ptb)->addVelocityCorrection(delta_b);

          break;
        case 3:  // ptb is boundary particle

          if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta
is ghost or boundary particles
            break;  // do not consider pta, as we treat before
          }

          // calculate Vab as the method shown in Morris's paper, 1996
              // interact with boundary particles
              da = pta_position.z()-allContainer.getMinCorner().z();  // assume
with the bottom boundary
          dB = allContainer.getMinCorner().z()-ptb_position.z();  // assume with
the bottom boundary
          if(ptb_position.x() < allContainer.getMinCorner().x()){  // with left
boundary
        da = pta_position.x() - allContainer.getMinCorner().x();
        dB = allContainer.getMinCorner().x() - ptb_position.x();
          }
          else if(ptb_position.x() > allContainer.getMaxCorner().x()){  // with
right boundary
        da = allContainer.getMaxCorner().x() - pta_position.x();
        dB = ptb_position.x() - allContainer.getMaxCorner().x();
          }
          else if(ptb_position.z() > allContainer.getMaxCorner().z()){  // with
top boundary
        da = allContainer.getMaxCorner().z()-pta_position.z();
        dB = ptb_position.z()-allContainer.getMaxCorner().z();
          }

              beta = 1+dB/da;
              if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

              vab = beta*(*pta)->getVelocity();
              vba = -vab;

              (*pta)->addDensityDot( (*ptb)->getParticleMass()*(vab*delta_aWab)
);
              (*ptb)->addDensityDot( (*pta)->getParticleMass()*(vba*delta_bWba)
);

              vr_dot = vab*(pta_position-ptb_position);
              if(vr_dot<0){
        mu_ab = smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
            Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
              }
              else{
         Gamma_ab = 0;
              }

              dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
              (*pta)->addVelocityDot(dva_dt);
//              dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)+pa/(rhoa*rhoa)+Gamma_ab)*delta_bWba;
//              (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

              delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
              (*pta)->addVelocityCorrection(delta_a);
//              delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
//              (*ptb)->addVelocityCorrection(delta_b);

//          // apply the boundary forces by Lennard-Jones potential as in
Monaghan's paper(1994)
//              if(rab<=spaceInterval){ // ptb is in the smooth kernel
//            dva_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(pta_position-ptb_position)/(rab*rab);
//            (*pta)->addVelocityDot(dva_dt);
//              } // end if
          break;
        default:
          std::cout << "SPH particle type should be 1, 2 or 3!" << std::endl;
          exit(-1);

      } //end swtich type
        } // end if 3h
    } // end for ptb in the same cell

    for(ptb=tmp_particleVec.begin(); ptb!=tmp_particleVec.end(); ptb++){  // all
particles in pvec's neighboring cells
        ptb_position = (*ptb)->getCurrPosition();
        rab = dem::vfabs(pta_position-ptb_position);
        if(rab<=kernelSize){  // ptb is in the smooth kernel
          pb = (*ptb)->getParticlePressure();
//          mub = (*ptb)->getParticleViscosity();
          rhob = (*ptb)->getParticleDensity();

          Wq = kernelFunction(rab/smoothLength);
          phi_4 = pow(Wq/Wqmin,4);
          if(pb>=0){
          Rb = 0.006;
          }
          else{
          Rb = 0.6;
          }
          coefficient_a = 1+Ra*phi_4;
          coefficient_b = 1+Rb*phi_4;

      // add the density dot for pta and ptb
          delta_aWab = gradientKernelFunction(pta_position, ptb_position);  //
this is to add SPH pta
          delta_bWba = - delta_aWab;
          dWab_dra = partialKernelFunction(pta_position, ptb_position);  // this
is to add SPH pta
          dWba_drb = dWab_dra;
          Wab = kernelFunction(pta_position, ptb_position);  // this is to add
SPH pta
          Wba = Wab;
      switch((*ptb)->getType()){
        case 1:  // ptb is free SPH particle
          switch((*pta)->getType()){
            case 1:  // free with free
                vab = (*pta)->getVelocity()-(*ptb)->getVelocity();
               vba = -vab;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
             Gamma_ab = 0;
                }


                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
                (*pta)->addVelocityDot(dva_dt);
                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);

                delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
                (*pta)->addVelocityCorrection(delta_a);
                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                (*ptb)->addVelocityCorrection(delta_b);
              break;
            case 2:  // ghost with free
        demt = (*pta)->getDemParticle();
        demt_curr = demt->currentPosition();
            ptb_local = demt->globalToLocal(ptb_position-demt_curr);  // the
local position of sph point ptb
            ra = demt->getA(); rc = demt->getC();
            k =
1.0/(sqrt(ptb_local.x()*ptb_local.x()/(ra*ra)+ptb_local.z()*ptb_local.z()/(rc*rc)
) );
            da = dem::vfabs(ptb_local-k*ptb_local);  // the distance is the same
in rotated coordinates

                // calculate Vab as the method shown in Morris's paper, 1996

                // (1) here I wanna use the distance from the point a/b to the
surface of the ellipsoid to simplify the problem
            pta_local = (*pta)->getLocalPosition();
            k =
1.0/(sqrt(pta_local.x()*pta_local.x()/(ra*ra)+pta_local.z()*pta_local.z()/(rc*rc)
) );
                dB = dem::vfabs(pta_local-k*pta_local);

//              // (2) here the Morris's method is used
//              xa = pta_position.getx(); ya = pta_position.gety();
//              xB = ptb_position.getx(); yB = ptb_position.gety();
//              if(ya==0) {da = xa-radius; dB = radius-xB;}
//              else if(xa==0) {da = ya-radius; dB = radius-yB;}
//              else {
//          sqrt_xaya = sqrt(xa*xa+ya*ya);
//          k = radius/sqrt_xaya;
//          da = radius/k - radius;
//          dB = fabs(xa*xB+ya*yB-k*radius*radius)/sqrt_xaya;
//              }


                beta = 1+dB/da;
                if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

            vdem = (*pta)->getVelocity();
                vba = beta*((*ptb)->getVelocity()-vdem);
                vab = -vba;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
                mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                    Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
                 Gamma_ab = 0;
                }

                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
            demt->addForce((*pta)->getParticleMass()*dva_dt);
            demt->addMoment( (pta_position-demt_curr) %
((*pta)->getParticleMass()*dva_dt) );
//                (*pta)->addVelocityDot(dva_dt);

                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

//                  delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
//                  (*pta)->addVelocityCorrection(delta_a);
                  delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                  (*ptb)->addVelocityCorrection(delta_b);

        break;

            case 3:  // boundary with free

        // calculate Vab as the method shown in Morris's paper, 1996
                // interact with boundary particles
                da = ptb_position.z()-allContainer.getMinCorner().z();  //
assume with the bottom boundary
        dB = allContainer.getMinCorner().z()-pta_position.x();  // assume with
the bottom boundary
        if(pta_position.x()<allContainer.getMinCorner().x()){  // with left
boundary
            da = ptb_position.x()-allContainer.getMinCorner().x();
            dB = allContainer.getMinCorner().x()-pta_position.x();
        }
        else if(pta_position.x()>allContainer.getMaxCorner().x()){ // with right
boundary
            da = allContainer.getMaxCorner().x()-ptb_position.x();
            dB = pta_position.x()-allContainer.getMaxCorner().x();
        }
        else if(pta_position.z()>allContainer.getMaxCorner().z()){ // with top
boundary
            da = allContainer.getMaxCorner().z()-ptb_position.z();
            dB = pta_position.z()-allContainer.getMaxCorner().z();
        }

                beta = 1+dB/da;
                if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

                vba = beta*(*ptb)->getVelocity();
                vab = -vba;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
             Gamma_ab = 0;
                }

//                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
//                (*pta)->addVelocityDot(dva_dt);
                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)+pa/(rhoa*rhoa)+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

//            delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
//                (*pta)->addVelocityCorrection(delta_a);
                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                (*ptb)->addVelocityCorrection(delta_b);

            // apply the boundary forces by Lennard-Jones potential as in
Monaghan's paper(1994)
                if(rab<=spaceInterval){ // ptb is in the smooth kernel
                dvb_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(ptb_position-pta_position)/(rab*rab);
                (*ptb)->addVelocityDot(dvb_dt);
                } // end if

        break;
            default:
            std::cout << "SPH particle type of pta should be 1, 2 or 3!" <<
std::endl;
            exit(-1);
            } // end switch pta

          break;
        case 2:  // ptb is ghost particle


          if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta
is ghost or boundary particles
            break;  // do not consider pta, as we treat before
          }
          demt = (*ptb)->getDemParticle();
          demt_curr = demt->currentPosition();
          pta_local = demt->globalToLocal(pta_position-demt_curr);  // the local
position of sph point pta
          ra = demt->getA(); rc = demt->getC();
          k =
1.0/(sqrt(pta_local.x()*pta_local.x()/(ra*ra)+pta_local.z()*pta_local.z()/(rc*rc)
) );
          da = dem::vfabs(pta_local-k*pta_local);  // the distance is the same
in rotated coordinates

              // calculate Vab as the method shown in Morris's paper, 1996

              // (1) here I wanna use the distance from the point a/b to the
surface of the ellipsoid to simplify the problem
          ptb_local = (*ptb)->getLocalPosition();
          k =
1.0/(sqrt(ptb_local.x()*ptb_local.x()/(ra*ra)+ptb_local.z()*ptb_local.z()/(rc*rc)
) );
              dB = dem::vfabs(ptb_local-k*ptb_local);

//              // (2) here the Morris's method is used
//              xa = pta_position.getx(); ya = pta_position.gety();
//              xB = ptb_position.getx(); yB = ptb_position.gety();
//              if(ya==0) {da = xa-radius; dB = radius-xB;}
//              else if(xa==0) {da = ya-radius; dB = radius-yB;}
//              else {
//          sqrt_xaya = sqrt(xa*xa+ya*ya);
//          k = radius/sqrt_xaya;
//          da = radius/k - radius;
//          dB = fabs(xa*xB+ya*yB-k*radius*radius)/sqrt_xaya;
//              }


              beta = 1+dB/da;
              if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

          vdem = (*ptb)->getVelocity();
              vab = beta*((*pta)->getVelocity()-vdem);
              vba = -vab;

              (*pta)->addDensityDot( (*ptb)->getParticleMass()*(vab*delta_aWab)
);
              (*ptb)->addDensityDot( (*pta)->getParticleMass()*(vba*delta_bWba)
);

              vr_dot = vab*(pta_position-ptb_position);
              if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
              }
              else{
             Gamma_ab = 0;
              }

              dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
              (*pta)->addVelocityDot(dva_dt);
              dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
          demt->addForce((*ptb)->getParticleMass()*dvb_dt);
          demt->addMoment( (ptb_position-demt_curr) %
((*ptb)->getParticleMass()*dvb_dt) );
//              (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

                delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
                (*pta)->addVelocityCorrection(delta_a);
//                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
//                (*ptb)->addVelocityCorrection(delta_b);

          break;

        case 3:  // ptb is boundary particle

          if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta
is ghost or boundary particles
            break;  // do not consider pta, as we treat before
          }

          // calculate Vab as the method shown in Morris's paper, 1996
              // interact with boundary particles
              da = pta_position.z()-allContainer.getMinCorner().z();  // assume
with the bottom boundary
          dB = allContainer.getMinCorner().z()-ptb_position.z();  // assume with
the bottom boundary
          if(ptb_position.x() < allContainer.getMinCorner().x()){  // with left
boundary
        da = pta_position.x() - allContainer.getMinCorner().x();
        dB = allContainer.getMinCorner().x() - ptb_position.x();
          }
          else if(ptb_position.x() > allContainer.getMaxCorner().x()){  // with
right boundary
        da = allContainer.getMaxCorner().x() - pta_position.x();
        dB = ptb_position.x() - allContainer.getMaxCorner().x();
          }
          else if(ptb_position.z() > allContainer.getMaxCorner().z()){  // with
top boundary
        da = allContainer.getMaxCorner().z()-pta_position.z();
        dB = ptb_position.z()-allContainer.getMaxCorner().z();
          }

              beta = 1+dB/da;
              if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

              vab = beta*(*pta)->getVelocity();
              vba = -vab;

              (*pta)->addDensityDot( (*ptb)->getParticleMass()*(vab*delta_aWab)
);
              (*ptb)->addDensityDot( (*pta)->getParticleMass()*(vba*delta_bWba)
);

              vr_dot = vab*(pta_position-ptb_position);
              if(vr_dot<0){
        mu_ab = smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
            Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
              }
              else{
         Gamma_ab = 0;
              }

              dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
              (*pta)->addVelocityDot(dva_dt);
//              dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)+pa/(rhoa*rhoa)+Gamma_ab)*delta_bWba;
//              (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

              delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
              (*pta)->addVelocityCorrection(delta_a);
//              delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
//              (*ptb)->addVelocityCorrection(delta_b);

              // apply the boundary forces by Lennard-Jones potential as in
Monaghan's paper(1994)
              if(rab<=spaceInterval){ // ptb is in the smooth kernel
            dva_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(pta_position-ptb_position)/(rab*rab);
            (*pta)->addVelocityDot(dva_dt);
              } // end if

          break;
        default:
          std::cout << "SPH particle type should be 1, 2 or 3!" << std::endl;
          exit(-1);

      } //end swtich type
        } // end if 3h
    } // end for ptb in neighbor cells
      } // end for pta

      tmp_particleVec.clear();  // clear elements in tmp-vector for particles
neighboring cells, it is important

  } // end for pvec, different cells


//      // apply the boundary forces by Lennard-Jones potential as in Monaghan's
paper(1994)
//      for(ptb=SPHBoundaryParticleVec.begin();
ptb!=SPHBoundaryParticleVec.end(); ptb++){
//    ptb_position = (*ptb)->getCurrPosition();
//    rab = dem::vfabs(pta_position-ptb_position);
//    if(rab<=spaceInterval){ // ptb is in the smooth kernel
//        dva_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(pta_position-ptb_position)/(rab*rab);
//        (*pta)->addVelocityDot(dva_dt);
//    } // end if
//      } // end ptb


    } // end calculateSPHDensityDotVelocityDotLinkedList2D()


    // divide SPH domain in each cpu into different cells in 2D in xz plane.
    // see the notes 5/20/2015 and 5/21/2015 or Simpson's paper "Numerical
techniques for three-dimensional Smoothed Particle Hydrodynamics"
    void SmoothParticleHydro::divideSPHDomain3D(){

  // clear the std::vector< std::vector<sph::SPHParticle*> > SPHParticleCellVec
  for(int pvec=0; pvec<SPHParticleCellVec.size(); pvec++){
      SPHParticleCellVec[pvec].clear();
  }
  SPHParticleCellVec.clear();

      Vec  vmin = container.getMinCorner();
      Vec  vmax = container.getMaxCorner();
  REAL small_value = 0.01*spaceInterval;
      REAL xmin = vmin.x()-sphCellSize-small_value; REAL ymin =
vmin.y()-sphCellSize-small_value; REAL zmin = vmin.z()-sphCellSize-small_value;
// expand the container by sphCellSize for cells domain is necessary
      REAL xmax = vmax.x()+sphCellSize+small_value; REAL ymax =
vmax.y()+sphCellSize+small_value; REAL zmax = vmax.z()+sphCellSize+small_value;
// since the sph domain that we divide is mergeSPHParticleVec
  Nx = (xmax-xmin)/kernelSize+1;  // (xmax-xmin)/(3h)+1
  Nz = (zmax-zmin)/kernelSize+1;  // (zmax-zmin)/(3h)+1
  Ny = (ymax-ymin)/kernelSize+1;  // (ymax-ymin)/(3h)+1
  numCell = Nx*Nz*Ny;
  SPHParticleCellVec.resize(numCell);  // at this point, the cellVec contains
numCell vectors,
            // each vector is empty but ready to store the pointer of SPH
particles (this process will be in the calculation of SPH forces)
        pnum_vec.clear();
  pnum_vec.push_back(1); pnum_vec.push_back(Nx-1); pnum_vec.push_back(Nx);
  pnum_vec.push_back(Nx+1); pnum_vec.push_back(Nx*Nz);
pnum_vec.push_back(Nx*Nz-Nx);
  pnum_vec.push_back(Nx*Nz+Nx); pnum_vec.push_back(Nx*Nz-1);
pnum_vec.push_back(Nx*Nz-1-Nx);
  pnum_vec.push_back(Nx*Nz-1+Nx); pnum_vec.push_back(Nx*Nz+1);
pnum_vec.push_back(Nx*Nz+1-Nx);
  pnum_vec.push_back(Nx*Nz+1+Nx);

  dem::Vec tmp_xyz;
  int num;
  for(std::vector<sph::SPHParticle*>::iterator pt=mergeSPHParticleVec.begin();
pt!=mergeSPHParticleVec.end(); pt++){ // mergeSPHParticle contains also the sph
particles from neighboring cpus
      tmp_xyz = (*pt)->getCurrPosition();
      num = int( (tmp_xyz.y()-ymin)/kernelSize )*Nx*Nz + int(
(tmp_xyz.z()-zmin)/kernelSize )*Nx + int( (tmp_xyz.x()-xmin)/kernelSize );
      SPHParticleCellVec[num].push_back(*pt);
  }

  REAL maxRadius = gradation.getPtclMaxRadius();
  std::vector<sph::SPHParticle*>::iterator gt;
  for(std::vector<Particle*>::iterator pdt=particleVec.begin();
pdt!=particleVec.end(); pdt++){  // the sph ghost particles in the current cpu's
dem particles
                          // will be definitely inside the [xmin, ymin, zmin,
xmax ...]
      tmp_xyz = (*pdt)->currentPosition();
      if(tmp_xyz.x()>=xmax+maxRadius || tmp_xyz.y()>=ymax+maxRadius ||
tmp_xyz.z()>=zmax+maxRadius
      || tmp_xyz.x()<=xmin-maxRadius || tmp_xyz.y()<=ymin-maxRadius ||
tmp_xyz.z()<=zmin-maxRadius )  // dem particle is outside of the
    continue;
      for(gt=(*pdt)->SPHGhostParticleVec.begin();
gt!=(*pdt)->SPHGhostParticleVec.end(); gt++){  // set all SPH ghost particles
into their cells
        tmp_xyz = (*gt)->getCurrPosition();
    if(tmp_xyz.x()>=xmax || tmp_xyz.y()>=ymax || tmp_xyz.z()>=zmax
    || tmp_xyz.x()<=xmin || tmp_xyz.y()<=ymin || tmp_xyz.z()<=zmin )
       continue;  // this sph ghost particle is outside of the container, go to
next sph ghost particle
        num = int( (tmp_xyz.y()-ymin)/kernelSize )*Nx*Nz + int(
(tmp_xyz.z()-zmin)/kernelSize )*Nx + int( (tmp_xyz.x()-xmin)/kernelSize );
        SPHParticleCellVec[num].push_back(*gt);
      }
  }

  for(std::vector<Particle*>::iterator pdt=recvParticleVec.begin();
pdt!=recvParticleVec.end(); pdt++){  // the this time, recvParticleVec are the
dem particles that are communicated by the commuParticle
      tmp_xyz = (*pdt)->currentPosition();
      if(tmp_xyz.x()>=xmax+maxRadius || tmp_xyz.y()>=ymax+maxRadius ||
tmp_xyz.z()>=zmax+maxRadius
      || tmp_xyz.x()<=xmin-maxRadius || tmp_xyz.y()<=ymin-maxRadius ||
tmp_xyz.z()<=zmin-maxRadius )  // dem particle is outside of the
    continue;                        // expanded domain

      for(gt=(*pdt)->SPHGhostParticleVec.begin();
gt!=(*pdt)->SPHGhostParticleVec.end(); gt++){  // set part SPH ghost particles
into their cells
        tmp_xyz = (*gt)->getCurrPosition();
    // not all sph ghost particles in these received particles are inside the
container
    if(tmp_xyz.x()>=xmax || tmp_xyz.y()>=ymax || tmp_xyz.z()>=zmax
    || tmp_xyz.x()<=xmin || tmp_xyz.y()<=ymin || tmp_xyz.z()<=zmin )
       continue;  // this sph ghost particle is outside of the container, go to
next sph ghost particle
        num = int( (tmp_xyz.y()-ymin)/kernelSize )*Nx*Nz + int(
(tmp_xyz.z()-zmin)/kernelSize )*Nx + int( (tmp_xyz.x()-xmin)/kernelSize );
        SPHParticleCellVec[num].push_back(*gt);
      }
  }

    } // divideSPHDomain3D

    //// the momentum equilibrium equation and state equation are implemented as
in Monaghan's paper (1994), simulate free surface flow using sph
    //// here the neighboring list of SPH particles is searched by the cells,
vector< vector<sph::SPHParticle*> > SPHParticleCellVec;
    void SmoothParticleHydro::calculateSPHDensityDotVelocityDotLinkedList3D(){

  // divide the SPH domain into different cells, each cell will contain SPH
particles within it
  divideSPHDomain3D();

//  checkDivision();  // pass, May 22, 2015
//  checkNeighborCells3D();  // pass, May 22, 2015

  // initialize the densityDot and velocityDot of all the SPH particles
  dem::Vec tmp_vec = dem::Vec(0,0,
-(util::getParam<REAL>("gravAccel"])*(util::getParam<REAL>("gravScale"]) );
  dem::Vec zero_vec = dem::Vec(0,0,0);
  // in the calculation of forces between sph particles, we need to use the
mergeSPHParticleVec,
  // since mergeVec contains the sph particles from neighboring cpus. While we
can only update and migrate and communicate d_sphParticleVec
  for(std::vector<sph::SPHParticle*>::iterator pt=mergeSPHParticleVec.begin();
pt!=mergeSPHParticleVec.end(); pt++){
      (*pt)->setDensityDotVelocityDotZero();
      (*pt)->calculateParticleViscosity();  // everytime when use
getParticleViscolisity(), make sure that pressure has been calculated!!!!
      (*pt)->calculateParticlePressure();    // everytime when use
getParticlePressure(), make sure that pressure has been calculated!!!!
      switch((*pt)->getType()){
        case 1: // free sph particle
    (*pt)->addVelocityDot(tmp_vec);
    break;
        case 2: // ghost sph particle
    break;
        case 3: // boundary sph particle
    (*pt)->addVelocityDot(zero_vec);
    break;
        default:
    std::cout << "SPH particle type of pta should be 1, 2 or 3!" << std::endl;
    exit(-1);
      } // switch
  }

  std::vector<sph::SPHParticle*>::iterator gt;
  for(std::vector<Particle*>::iterator pdt=mergeParticleVec.begin();
pdt!=mergeParticleVec.end(); pdt++){  // all sph ghost particles
      for(gt=(*pdt)->SPHGhostParticleVec.begin();
gt!=(*pdt)->SPHGhostParticleVec.end(); gt++){
        (*gt)->setDensityDotVelocityDotZero();
        (*gt)->calculateParticleViscosity();  // everytime when use
getParticleViscolisity(), make sure that pressure has been calculated!!!!
        (*gt)->calculateParticlePressure();  // everytime when use
getParticlePressure(), make sure that pressure has been calculated!!!!
      }
  }

  // temporary variables used in the loop
  dem::Vec pta_position;
  dem::Vec ptb_position;
  dem::Vec delta_aWab;
  dem::Vec delta_bWba;
  dem::Vec vab;
  dem::Vec vba;
  dem::Vec vdem;
  dem::Vec dva_dt;
  dem::Vec dvb_dt;
  dem::Vec delta_a;
  dem::Vec delta_b;
  REAL pa, pb, rhoa, rhob, mua, mub;
  REAL rab;
        REAL dWab_dra;
  REAL dWba_drb;
  REAL Wab, Wba;
  REAL da, dB;
  REAL beta;
  REAL xa, ya, xB, yB, k, sqrt_xaya;  // variables for Morris' method to
calculate da/dB
  std::vector<sph::SPHParticle*>::iterator ptb;
  REAL ra, rb, rc;  // the geometry of the dem particle
  dem::Vec pta_local, ptb_local;  // local position of pta and ptb in the dem
particle
  dem::Vec demt_curr;
  REAL Gamma_ab, mu_ab, vr_dot;
  REAL alpha = util::getParam<REAL>("alpha"];
     REAL alpha_zero = 0;  // the viscous between free and ghost/boundary
  REAL epsilon = util::getParam<REAL>("epsilon"];  // parameter for velocity
correction
  REAL Wq, Ra, Rb, phi_4, coefficient_a, coefficient_b;
  dem::Particle* demt;

  int pnum;
  std::vector<sph::SPHParticle*> tmp_particleVec;  // sph particles in
neighboring cells
  for(int pvec=0; pvec<SPHParticleCellVec.size(); pvec++){

      // store all SPH particles in pvec's neighboring cells
      tmp_particleVec.clear();  // it is the same for the particles in the same
cell
      for(std::vector<int>::const_iterator pint=pnum_vec.begin();
pint!=pnum_vec.end(); pint++){
    pnum = pvec+(*pint);
    if(pnum<numCell){
        for(std::vector<sph::SPHParticle*>::iterator pt =
SPHParticleCellVec[pnum].begin(); pt!= SPHParticleCellVec[pnum].end(); pt++) {
          tmp_particleVec.push_back(*pt);
                }
    }
      }

      for(std::vector<sph::SPHParticle*>::iterator
pta=SPHParticleCellVec[pvec].begin(); pta!=SPHParticleCellVec[pvec].end();
pta++){  // SPH particles in cell pvec
    pa = (*pta)->getParticlePressure();
        if(pa>=0){
        Ra = 0.006;
        }
        else{
        Ra = 0.6;
        }
//        mua = (*pta)->getParticleViscosity();
        rhoa = (*pta)->getParticleDensity();
        pta_position = (*pta)->getCurrPosition();

//    if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta is
ghost or boundary particles
//        continue;  // do not consider pta, as we treat before
//    }

    for(ptb=pta+1; ptb!=SPHParticleCellVec[pvec].end(); ptb++){  // sum over the
SPH particles in the same cell pvec
        ptb_position = (*ptb)->getCurrPosition();
        rab = dem::vfabs(pta_position-ptb_position);
        if(rab<=kernelSize){  // ptb is in the smooth kernel
          pb = (*ptb)->getParticlePressure();
//          mub = (*ptb)->getParticleViscosity();
          rhob = (*ptb)->getParticleDensity();

          Wq = kernelFunction(rab/smoothLength);
          phi_4 = pow(Wq/Wqmin,4);
          if(pb>=0){
          Rb = 0.006;
          }
          else{
          Rb = 0.6;
          }
          coefficient_a = 1+Ra*phi_4;
          coefficient_b = 1+Rb*phi_4;

      // we have three types of SPH particles: 1, free particle; 2, ghost
particle; 3, boundary particle
      // Then we have 3x3 = 9 different types of interactions with the three
types of particles
      // so we cannot judge the interaction type only by the type of particle
ptb, we need to consider pta also
      // pta          ptb          need to consider or not
      // 1            1              V      free with free
      // 1            2              V      free with ghost
      // 1            3              V      free with boundary

      // 2            1              V      ghost with free
      // 2            2              X      ghost with ghost
      // 2            3              X      ghost with boundary

      // 3            1              V      boundary with free
      // 3            2              X      boundary with ghost
      // 3            3              X       boundary with boundary


      // add the density dot for pta and ptb
          delta_aWab = gradientKernelFunction(pta_position, ptb_position);  //
this is to add SPH pta
          delta_bWba = - delta_aWab;
          dWab_dra = partialKernelFunction(pta_position, ptb_position);  // this
is to add SPH pta
          dWba_drb = dWab_dra;
          Wab = kernelFunction(pta_position, ptb_position);  // this is to add
SPH pta
          Wba = Wab;

      switch((*ptb)->getType()){
        case 1:  // ptb is free SPH particle
          switch((*pta)->getType()){
            case 1:  // free with free
                vab = (*pta)->getVelocity()-(*ptb)->getVelocity();
               vba = -vab;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
             Gamma_ab = 0;
                }


                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
                (*pta)->addVelocityDot(dva_dt);
                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);

                delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
                (*pta)->addVelocityCorrection(delta_a);
                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                (*ptb)->addVelocityCorrection(delta_b);
              break;
            case 2:  // ghost with free
        demt = (*pta)->getDemParticle();
        demt_curr = demt->currentPosition();
            ptb_local = demt->globalToLocal(ptb_position-demt_curr);  // the
local position of sph point ptb
            ra = demt->getA(); rb = demt->getB(); rc = demt->getC();
            k =
1.0/(sqrt(ptb_local.x()*ptb_local.x()/(ra*ra)+ptb_local.y()*ptb_local.y()/(rb*rb)+ptb_local.z()*ptb_local.z()/(rc*rc)
) );
            da = dem::vfabs(ptb_local-k*ptb_local);  // the distance is the same
in rotated coordinates

                // calculate Vab as the method shown in Morris's paper, 1996

                // (1) here I wanna use the distance from the point a/b to the
surface of the ellipsoid to simplify the problem
            pta_local = (*pta)->getLocalPosition();
            k =
1.0/(sqrt(pta_local.x()*pta_local.x()/(ra*ra)+pta_local.y()*pta_local.y()/(rb*rb)+pta_local.z()*pta_local.z()/(rc*rc)
) );
                dB = dem::vfabs(pta_local-k*pta_local);

//              // (2) here the Morris's method is used
//              xa = pta_position.x(); ya = pta_position.y();
//              xB = ptb_position.x(); yB = ptb_position.y();
//              if(ya==0) {da = xa-radius; dB = radius-xB;}
//              else if(xa==0) {da = ya-radius; dB = radius-yB;}
//              else {
//          sqrt_xaya = sqrt(xa*xa+ya*ya);
//          k = radius/sqrt_xaya;
//          da = radius/k - radius;
//          dB = fabs(xa*xB+ya*yB-k*radius*radius)/sqrt_xaya;
//              }

                beta = 1+dB/da;
                if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

            vdem = (*pta)->getVelocity();
                vba = beta*((*ptb)->getVelocity()-vdem);
                vab = -vba;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
                mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                    Gamma_ab =
(-alpha_zero*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
                 Gamma_ab = 0;
                }

                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
            demt->addForce((*pta)->getParticleMass()*dva_dt);
            demt->addMoment( (pta_position-demt_curr) %
((*pta)->getParticleMass()*dva_dt) );
//                (*pta)->addVelocityDot(dva_dt);

                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

//                  delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
//                  (*pta)->addVelocityCorrection(delta_a);
                  delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                  (*ptb)->addVelocityCorrection(delta_b);
        break;

            case 3:  // boundary with free

        // calculate Vab as the method shown in Morris's paper, 1996
                // interact with boundary particles
                da = ptb_position.z()-allContainer.getMinCorner().z();  //
assume with the bottom boundary
        dB = allContainer.getMinCorner().z()-pta_position.x();  // assume with
the bottom boundary
        if(pta_position.x()<allContainer.getMinCorner().x()){  // with left
boundary
            da = ptb_position.x()-allContainer.getMinCorner().x();
            dB = allContainer.getMinCorner().x()-pta_position.x();
        }
        else if(pta_position.x()>allContainer.getMaxCorner().x()){ // with right
boundary
            da = allContainer.getMaxCorner().x()-ptb_position.x();
            dB = pta_position.x()-allContainer.getMaxCorner().x();
        }
        else if(pta_position.z()>allContainer.getMaxCorner().z()){ // with top
boundary
            da = allContainer.getMaxCorner().z()-ptb_position.z();
            dB = pta_position.z()-allContainer.getMaxCorner().z();
        }

                beta = 1+dB/da;
                if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

                vba = beta*(*ptb)->getVelocity();
                vab = -vba;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha_zero*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
             Gamma_ab = 0;
                }

//                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
//                (*pta)->addVelocityDot(dva_dt);
                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)+pa/(rhoa*rhoa)+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

//            delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
//                (*pta)->addVelocityCorrection(delta_a);
                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                (*ptb)->addVelocityCorrection(delta_b);

//            // apply the boundary forces by Lennard-Jones potential as in
Monaghan's paper(1994)
//                if(rab<=spaceInterval){ // ptb is in the smooth kernel
//                dvb_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(ptb_position-pta_position)/(rab*rab);
//                (*ptb)->addVelocityDot(dvb_dt);
//                } // end if

        break;
            default:
            std::cout << "SPH particle type of pta should be 1, 2 or 3!" <<
std::endl;
            exit(-1);
            } // end switch pta

          break;
        case 2:  // ptb is ghost particle

          if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta
is ghost or boundary particles
            break;  // do not consider pta, as we treat before
          }
          demt = (*ptb)->getDemParticle();
          demt_curr = demt->currentPosition();
          pta_local = demt->globalToLocal(pta_position-demt_curr);  // the local
position of sph point pta
          ra = demt->getA(); rb = demt->getB(); rc = demt->getC();
          k =
1.0/(sqrt(pta_local.x()*pta_local.x()/(ra*ra)+pta_local.y()*pta_local.y()/(rb*rb)+pta_local.z()*pta_local.z()/(rc*rc)
) );
          da = dem::vfabs(pta_local-k*pta_local);  // the distance is the same
in rotated coordinates

              // calculate Vab as the method shown in Morris's paper, 1996

              // (1) here I wanna use the distance from the point a/b to the
surface of the ellipsoid to simplify the problem
          ptb_local = (*ptb)->getLocalPosition();
          k =
1.0/(sqrt(ptb_local.x()*ptb_local.x()/(ra*ra)+ptb_local.y()*ptb_local.y()/(rb*rb)+ptb_local.z()*ptb_local.z()/(rc*rc)
) );
              dB = dem::vfabs(ptb_local-k*ptb_local);

//              // (2) here the Morris's method is used
//              xa = pta_position.x(); ya = pta_position.gety();
//              xB = ptb_position.x(); yB = ptb_position.gety();
//              if(ya==0) {da = xa-radius; dB = radius-xB;}
//              else if(xa==0) {da = ya-radius; dB = radius-yB;}
//              else {
//          sqrt_xaya = sqrt(xa*xa+ya*ya);
//          k = radius/sqrt_xaya;
//          da = radius/k - radius;
//          dB = fabs(xa*xB+ya*yB-k*radius*radius)/sqrt_xaya;
//              }


              beta = 1+dB/da;
              if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

          vdem = (*ptb)->getVelocity();
              vab = beta*((*pta)->getVelocity()-vdem);
              vba = -vab;

              (*pta)->addDensityDot( (*ptb)->getParticleMass()*(vab*delta_aWab)
);
              (*ptb)->addDensityDot( (*pta)->getParticleMass()*(vba*delta_bWba)
);

              vr_dot = vab*(pta_position-ptb_position);
              if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha_zero*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
              }
              else{
             Gamma_ab = 0;
              }

              dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
              (*pta)->addVelocityDot(dva_dt);
              dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
          demt->addForce((*ptb)->getParticleMass()*dvb_dt);
          demt->addMoment( (ptb_position-demt_curr) %
((*ptb)->getParticleMass()*dvb_dt) );
//              (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

                delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
                (*pta)->addVelocityCorrection(delta_a);
//                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
//                (*ptb)->addVelocityCorrection(delta_b);
          break;
        case 3:  // ptb is boundary particle

          if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta
is ghost or boundary particles
            break;  // do not consider pta, as we treat before
          }

          // calculate Vab as the method shown in Morris's paper, 1996
              // interact with boundary particles
              da = pta_position.z()-allContainer.getMinCorner().z();  // assume
with the bottom boundary
          dB = allContainer.getMinCorner().z()-ptb_position.z();  // assume with
the bottom boundary
          if(ptb_position.x() < allContainer.getMinCorner().x()){  // with left
boundary
        da = pta_position.x() - allContainer.getMinCorner().x();
        dB = allContainer.getMinCorner().x() - ptb_position.x();
          }
          else if(ptb_position.x() > allContainer.getMaxCorner().x()){  // with
right boundary
        da = allContainer.getMaxCorner().x() - pta_position.x();
        dB = ptb_position.x() - allContainer.getMaxCorner().x();
          }
          else if(ptb_position.z() > allContainer.getMaxCorner().z()){  // with
top boundary
        da = allContainer.getMaxCorner().z()-pta_position.z();
        dB = ptb_position.z()-allContainer.getMaxCorner().z();
          }

              beta = 1+dB/da;
              if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

              vab = beta*(*pta)->getVelocity();
              vba = -vab;

              (*pta)->addDensityDot( (*ptb)->getParticleMass()*(vab*delta_aWab)
);
              (*ptb)->addDensityDot( (*pta)->getParticleMass()*(vba*delta_bWba)
);

              vr_dot = vab*(pta_position-ptb_position);
              if(vr_dot<0){
        mu_ab = smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
            Gamma_ab =
(-alpha_zero*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
              }
              else{
         Gamma_ab = 0;
              }

              dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
              (*pta)->addVelocityDot(dva_dt);
//              dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)+pa/(rhoa*rhoa)+Gamma_ab)*delta_bWba;
//              (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

              delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
              (*pta)->addVelocityCorrection(delta_a);
//              delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
//              (*ptb)->addVelocityCorrection(delta_b);

//          // apply the boundary forces by Lennard-Jones potential as in
Monaghan's paper(1994)
//              if(rab<=spaceInterval){ // ptb is in the smooth kernel
//            dva_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(pta_position-ptb_position)/(rab*rab);
//            (*pta)->addVelocityDot(dva_dt);
//              } // end if
          break;
        default:
          std::cout << "SPH particle type should be 1, 2 or 3!" << std::endl;
          exit(-1);

      } //end swtich type
        } // end if 3h
    } // end for ptb in the same cell

    for(ptb=tmp_particleVec.begin(); ptb!=tmp_particleVec.end(); ptb++){  // all
particles in pvec's neighboring cells
        ptb_position = (*ptb)->getCurrPosition();
        rab = dem::vfabs(pta_position-ptb_position);
        if(rab<=kernelSize){  // ptb is in the smooth kernel
          pb = (*ptb)->getParticlePressure();
//          mub = (*ptb)->getParticleViscosity();
          rhob = (*ptb)->getParticleDensity();

          Wq = kernelFunction(rab/smoothLength);
          phi_4 = pow(Wq/Wqmin,4);
          if(pb>=0){
          Rb = 0.006;
          }
          else{
          Rb = 0.6;
          }
          coefficient_a = 1+Ra*phi_4;
          coefficient_b = 1+Rb*phi_4;

      // add the density dot for pta and ptb
          delta_aWab = gradientKernelFunction(pta_position, ptb_position);  //
this is to add SPH pta
          delta_bWba = - delta_aWab;
          dWab_dra = partialKernelFunction(pta_position, ptb_position);  // this
is to add SPH pta
          dWba_drb = dWab_dra;
          Wab = kernelFunction(pta_position, ptb_position);  // this is to add
SPH pta
          Wba = Wab;
      switch((*ptb)->getType()){
        case 1:  // ptb is free SPH particle
          switch((*pta)->getType()){
            case 1:  // free with free
                vab = (*pta)->getVelocity()-(*ptb)->getVelocity();
               vba = -vab;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
             Gamma_ab = 0;
                }


                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
                (*pta)->addVelocityDot(dva_dt);
                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);

                delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
                (*pta)->addVelocityCorrection(delta_a);
                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                (*ptb)->addVelocityCorrection(delta_b);
              break;
            case 2:  // ghost with free

        demt = (*pta)->getDemParticle();
        demt_curr = demt->currentPosition();
            ptb_local = demt->globalToLocal(ptb_position-demt_curr);  // the
local position of sph point ptb
            ra = demt->getA(); rb = demt->getB(); rc = demt->getC();
            k =
1.0/(sqrt(ptb_local.x()*ptb_local.x()/(ra*ra)+ptb_local.y()*ptb_local.y()/(rb*rb)+ptb_local.z()*ptb_local.z()/(rc*rc)
) );
            da = dem::vfabs(ptb_local-k*ptb_local);  // the distance is the same
in rotated coordinates

                // calculate Vab as the method shown in Morris's paper, 1996

                // (1) here I wanna use the distance from the point a/b to the
surface of the ellipsoid to simplify the problem
            pta_local = (*pta)->getLocalPosition();
            k =
1.0/(sqrt(pta_local.x()*pta_local.x()/(ra*ra)+pta_local.y()*pta_local.y()/(rb*rb)+pta_local.z()*pta_local.z()/(rc*rc)
) );
                dB = dem::vfabs(pta_local-k*pta_local);

//              // (2) here the Morris's method is used
//              xa = pta_position.x(); ya = pta_position.y();
//              xB = ptb_position.x(); yB = ptb_position.y();
//              if(ya==0) {da = xa-radius; dB = radius-xB;}
//              else if(xa==0) {da = ya-radius; dB = radius-yB;}
//              else {
//          sqrt_xaya = sqrt(xa*xa+ya*ya);
//          k = radius/sqrt_xaya;
//          da = radius/k - radius;
//          dB = fabs(xa*xB+ya*yB-k*radius*radius)/sqrt_xaya;
//              }


                beta = 1+dB/da;
                if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

            vdem = (*pta)->getVelocity();
                vba = beta*((*ptb)->getVelocity()-vdem);
                vab = -vba;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
                mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                    Gamma_ab =
(-alpha_zero*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
                 Gamma_ab = 0;
                }

                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
            demt->addForce((*pta)->getParticleMass()*dva_dt);
            demt->addMoment( (pta_position-demt_curr) %
((*pta)->getParticleMass()*dva_dt) );
//                (*pta)->addVelocityDot(dva_dt);

                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

//                  delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
//                  (*pta)->addVelocityCorrection(delta_a);
                  delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                  (*ptb)->addVelocityCorrection(delta_b);
        break;
            case 3:  // boundary with free

        // calculate Vab as the method shown in Morris's paper, 1996
                // interact with boundary particles
                da = ptb_position.z()-allContainer.getMinCorner().z();  //
assume with the bottom boundary
        dB = allContainer.getMinCorner().z()-pta_position.x();  // assume with
the bottom boundary
        if(pta_position.x()<allContainer.getMinCorner().x()){  // with left
boundary
            da = ptb_position.x()-allContainer.getMinCorner().x();
            dB = allContainer.getMinCorner().x()-pta_position.x();
        }
        else if(pta_position.x()>allContainer.getMaxCorner().x()){ // with right
boundary
            da = allContainer.getMaxCorner().x()-ptb_position.x();
            dB = pta_position.x()-allContainer.getMaxCorner().x();
        }
        else if(pta_position.z()>allContainer.getMaxCorner().z()){ // with top
boundary
            da = allContainer.getMaxCorner().z()-ptb_position.z();
            dB = pta_position.z()-allContainer.getMaxCorner().z();
        }

                beta = 1+dB/da;
                if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

                vba = beta*(*ptb)->getVelocity();
                vab = -vba;

                (*pta)->addDensityDot(
(*ptb)->getParticleMass()*(vab*delta_aWab) );
                (*ptb)->addDensityDot(
(*pta)->getParticleMass()*(vba*delta_bWba) );

                vr_dot = vab*(pta_position-ptb_position);
                if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha_zero*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
                }
                else{
             Gamma_ab = 0;
                }

//                dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
//                (*pta)->addVelocityDot(dva_dt);
                dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)+pa/(rhoa*rhoa)+Gamma_ab)*delta_bWba;
                (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

//            delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
//                (*pta)->addVelocityCorrection(delta_a);
                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
                (*ptb)->addVelocityCorrection(delta_b);

            // apply the boundary forces by Lennard-Jones potential as in
Monaghan's paper(1994)
                if(rab<=spaceInterval){ // ptb is in the smooth kernel
                dvb_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(ptb_position-pta_position)/(rab*rab);
                (*ptb)->addVelocityDot(dvb_dt);
                } // end if

        break;
            default:
            std::cout << "SPH particle type of pta should be 1, 2 or 3!" <<
std::endl;
            exit(-1);
            } // end switch pta

          break;
        case 2:  // ptb is ghost particle

          if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta
is ghost or boundary particles
            break;  // do not consider pta, as we treat before
          }

          demt = (*ptb)->getDemParticle();
          demt_curr = demt->currentPosition();
          pta_local = demt->globalToLocal(pta_position-demt_curr);  // the local
position of sph point pta
          ra = demt->getA(); rb = demt->getB(); rc = demt->getC();
          k =
1.0/(sqrt(pta_local.x()*pta_local.x()/(ra*ra)+pta_local.y()*pta_local.y()/(rb*rb)+pta_local.z()*pta_local.z()/(rc*rc)
) );
          da = dem::vfabs(pta_local-k*pta_local);  // the distance is the same
in rotated coordinates

              // calculate Vab as the method shown in Morris's paper, 1996

              // (1) here I wanna use the distance from the point a/b to the
surface of the ellipsoid to simplify the problem
          ptb_local = (*ptb)->getLocalPosition();
          k =
1.0/(sqrt(ptb_local.x()*ptb_local.x()/(ra*ra)+ptb_local.y()*ptb_local.y()/(rb*rb)+ptb_local.z()*ptb_local.z()/(rc*rc)
) );
              dB = dem::vfabs(ptb_local-k*ptb_local);

//              // (2) here the Morris's method is used
//              xa = pta_position.x(); ya = pta_position.y();
//              xB = ptb_position.x(); yB = ptb_position.y();
//              if(ya==0) {da = xa-radius; dB = radius-xB;}
//              else if(xa==0) {da = ya-radius; dB = radius-yB;}
//              else {
//          sqrt_xaya = sqrt(xa*xa+ya*ya);
//          k = radius/sqrt_xaya;
//          da = radius/k - radius;
//          dB = fabs(xa*xB+ya*yB-k*radius*radius)/sqrt_xaya;
//              }


              beta = 1+dB/da;
              if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

          vdem = (*ptb)->getVelocity();
              vab = beta*((*pta)->getVelocity()-vdem);
              vba = -vab;

              (*pta)->addDensityDot( (*ptb)->getParticleMass()*(vab*delta_aWab)
);
              (*ptb)->addDensityDot( (*pta)->getParticleMass()*(vba*delta_bWba)
);

              vr_dot = vab*(pta_position-ptb_position);
              if(vr_dot<0){
            mu_ab =
smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
                Gamma_ab =
(-alpha_zero*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
              }
              else{
             Gamma_ab = 0;
              }

              dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
              (*pta)->addVelocityDot(dva_dt);
              dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)*coefficient_b+pa/(rhoa*rhoa)*coefficient_a+Gamma_ab)*delta_bWba;
          demt->addForce((*ptb)->getParticleMass()*dvb_dt);
          demt->addMoment( (ptb_position-demt_curr) %
((*ptb)->getParticleMass()*dvb_dt) );
//              (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

                delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
                (*pta)->addVelocityCorrection(delta_a);
//                delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
//                (*ptb)->addVelocityCorrection(delta_b);
          break;
        case 3:  // ptb is boundary particle

          if((*pta)->getType()!=1){  // pta is not free sph particles, i.e. pta
is ghost or boundary particles
            break;  // do not consider pta, as we treat before
          }

          // calculate Vab as the method shown in Morris's paper, 1996
              // interact with boundary particles
              da = pta_position.z()-allContainer.getMinCorner().z();  // assume
with the bottom boundary
          dB = allContainer.getMinCorner().z()-ptb_position.z();  // assume with
the bottom boundary
          if(ptb_position.x() < allContainer.getMinCorner().x()){  // with left
boundary
        da = pta_position.x() - allContainer.getMinCorner().x();
        dB = allContainer.getMinCorner().x() - ptb_position.x();
          }
          else if(ptb_position.x() > allContainer.getMaxCorner().x()){  // with
right boundary
        da = allContainer.getMaxCorner().x() - pta_position.x();
        dB = ptb_position.x() - allContainer.getMaxCorner().x();
          }
          else if(ptb_position.z() > allContainer.getMaxCorner().z()){  // with
top boundary
        da = allContainer.getMaxCorner().z()-pta_position.z();
        dB = ptb_position.z()-allContainer.getMaxCorner().z();
          }

              beta = 1+dB/da;
              if(beta>2 || beta<0 || isnan(beta)){ beta = 2; }

              vab = beta*(*pta)->getVelocity();
              vba = -vab;

              (*pta)->addDensityDot( (*ptb)->getParticleMass()*(vab*delta_aWab)
);
              (*ptb)->addDensityDot( (*pta)->getParticleMass()*(vba*delta_bWba)
);

              vr_dot = vab*(pta_position-ptb_position);
              if(vr_dot<0){
        mu_ab = smoothLength*vr_dot/(rab*rab+0.01*smoothLength*smoothLength);
            Gamma_ab =
(-alpha_zero*(util::getParam<REAL>("soundSpeed"])*mu_ab)/(rhoa+rhob)*2;
              }
              else{
         Gamma_ab = 0;
              }

              dva_dt =
-(*ptb)->getParticleMass()*(pa/(rhoa*rhoa)*coefficient_a+pb/(rhob*rhob)*coefficient_b+Gamma_ab)*delta_aWab;
              (*pta)->addVelocityDot(dva_dt);
//              dvb_dt =
-(*pta)->getParticleMass()*(pb/(rhob*rhob)+pa/(rhoa*rhoa)+Gamma_ab)*delta_bWba;
//              (*ptb)->addVelocityDot(dvb_dt);  // the velocities of the ghost
particles will not envolve as the same way as others

              delta_a =
epsilon*(*ptb)->getParticleMass()*(-vab)*Wab/(rhoa+rhob)*2;
              (*pta)->addVelocityCorrection(delta_a);
//              delta_b =
epsilon*(*pta)->getParticleMass()*(vab)*Wba/(rhoa+rhob)*2;
//              (*ptb)->addVelocityCorrection(delta_b);

              // apply the boundary forces by Lennard-Jones potential as in
Monaghan's paper(1994)
              if(rab<=spaceInterval){ // ptb is in the smooth kernel
            dva_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(pta_position-ptb_position)/(rab*rab);
            (*pta)->addVelocityDot(dva_dt);
              } // end if

          break;
        default:
          std::cout << "SPH particle type should be 1, 2 or 3!" << std::endl;
          exit(-1);

      } //end swtich type
        } // end if 3h
    } // end for ptb in neighbor cells
      } // end for pta

      tmp_particleVec.clear();  // clear elements in tmp-vector for particles
neighboring cells, it is important

  } // end for pvec, different cells


//      // apply the boundary forces by Lennard-Jones potential as in Monaghan's
paper(1994)
//      for(ptb=SPHBoundaryParticleVec.begin();
ptb!=SPHBoundaryParticleVec.end(); ptb++){
//    ptb_position = (*ptb)->getCurrPosition();
//    rab = dem::vfabs(pta_position-ptb_position);
//    if(rab<=spaceInterval){ // ptb is in the smooth kernel
//        dva_dt = D*(pow(spaceInterval/rab, p1)-pow(spaceInterval/rab,
p2))*(pta_position-ptb_position)/(rab*rab);
//        (*pta)->addVelocityDot(dva_dt);
//    } // end if
//      } // end ptb


    } // end calculateSPHDensityDotVelocityDotLinkedList3D()
    */
