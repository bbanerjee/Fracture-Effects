#include <InputOutput/DEMParticleFileWriter.h>
#include <InputOutput/DEMParticleFileReader.h>
#include <DiscreteElements/DEMParticleCreator.h>
#include <DiscreteElements/DEMParticle.h>
#include <Core/Util/Utility.h>
#include <InputOutput/InputParameter.h>
#include <gtest/gtest.h>

using namespace dem;

// Spherical particle - axis aligned
TEST(DEMParticleFileWriterTest, write) {

  // Setup the parameters that are used by the test
  dem::InputParameter::get().addParameter("young", 1.0e9);
  dem::InputParameter::get().addParameter("poisson", 0.3);
  dem::InputParameter::get().addParameter("specificG", 1.5);

  // Set up gradation
  Gradation gradation;
  auto sieveNum = 2u;
  std::vector<REAL> percent(sieveNum), size(sieveNum);
  percent[0] = 75;
  percent[1] = 25;
  size[0] = 100;
  size[1] = 1000;
  auto ratio_ba = 1.5;
  auto ratio_ca = 0.75;
  gradation.set(sieveNum, percent, size, ratio_ba, ratio_ca);

  // Set up domain
  Box allContainer(0, 0, 0, 1, 1, 1);

  // Set up layer flag (one layer)
  auto layerFlag = 1u;

  // Create particles
  DEMParticleCreator creator;
  DEMParticlePArray particles =
    creator.generateDEMParticles(layerFlag, 
                                 DEMParticle::DEMParticleShape::ELLIPSOID,
                                 allContainer, gradation);

  // Write particles
  DEMParticleFileWriter writer;
  writer.writeCSV(particles, gradation, "test_particles.csv");
  writer.writeXML(particles, gradation, "test_particles.xml");

  // Read the particles
  REAL young = util::getParam<REAL>("young");
  REAL poisson = util::getParam<REAL>("poisson");
  DEMParticlePArray readParticles;
  Gradation readGradation;
  DEMParticleFileReader reader;

  reader.read("test_particles.xml", young, poisson, true,
              readParticles, readGradation);
  writer.writeXML(readParticles, readGradation, "test_particles_rewrite.xml");

  // Compare file sizes
  std::ifstream file1("test_particles.xml", 
                      std::ifstream::ate | std::ifstream::binary);
  std::ifstream file2("test_particles_rewrite.xml", 
                      std::ifstream::ate | std::ifstream::binary);
  EXPECT_EQ(file1.tellg(),  file2.tellg());
}

