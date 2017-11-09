#include <Simulations/DEM/PeriodicRVECreation.h>
#include <DiscreteElements/DEMParticleCreator.h>
#include <InputOutput/DEMParticleFileWriter.h>
#include <Boundary/BoundaryFileWriter.h>
#include <Core/Util/Utility.h>

using namespace dem;
using util::combine;

void
PeriodicRVECreation::execute(DiscreteElements* dem)
{
  if (dem->getMPIRank() == 0) {

    // Read the particles and boundary from file
    std::string boundaryFile = InputParameter::get().datafile["boundaryFilename"];
    std::string particleFile = InputParameter::get().datafile["particleFilename"];
    dem->readBoundary(boundaryFile);
    dem->readParticles(particleFile);

    // Create the output writer in the master process
    // <outputFolder> foldername </outputFolder>
    std::string outputFolder(".");
    auto folderName =  dem::InputParameter::get().datafile["outputFolder"];
    outputFolder = util::createOutputFolder(folderName);
    dem->createOutputWriter(outputFolder, 0);

    // Get the control knobs for particle generation
    // Good values: marginFactor = 1, faceShiftFactor = 5
    // Default values: marginFactor = 2, faceShiftFactor = 0
    auto marginFactor = util::getParam<REAL>("periodicBoundaryMarginFactor");
    auto faceShiftFactor = util::getParam<REAL>("periodicBoundaryFaceShiftFactor");

    // Generate periodic particles 
    auto& particles = dem->getModifiableAllParticleVec(); 
    auto& spatialDomain = dem->getModifiableSpatialDomain();
    DEMParticleCreator creator;
    DEMParticlePArray periodic = creator.generatePeriodicDEMParticles(particles, 
      spatialDomain, marginFactor, faceShiftFactor);
  
    particles.insert(particles.end(), periodic.begin(), periodic.end());
    std::cout << "periodic = " << periodic.size() << "\n";
    std::cout << "total particles = " << particles.size() << "\n";

    // Write VTK files for visualization
    dem->writeBoundaryToFile();
    dem->writeParticlesToFile(0);

    // Write new boundary to input files
    auto filename = 
      dem::InputParameter::get().datafile["periodicBoundaryOutputFilename"];
    BoundaryFileWriter boundWriter;
    boundWriter.writeCSV(6, filename + ".csv", spatialDomain);
    boundWriter.writeXML(6, filename + ".xml", spatialDomain);

    // Write new particle distribution to input files
    filename = 
      dem::InputParameter::get().datafile["periodicParticleOutputFilename"];
    DEMParticleFileWriter partWriter;
    partWriter.writeCSV(particles, Gradation(), filename + ".csv");
    partWriter.writeXML(particles, Gradation(), filename + ".xml");
  }
}
