#include <Boundary/BoundaryReader.h>
#include <InputOutput/Parameter.h>
#include <Simulations/ProceedFromPreset.h>

#include <iostream>
#include <string>

using namespace dem;
void
ProceedFromPreset::execute(DiscreteElements* dem)
{
  std::string boundFile =
    Parameter::get().datafile["boundaryFile"];
  std::string partFile =
    Parameter::get().datafile["particleFile"];
  //std::cout << "Called ProceedFromPresetCommand with: "
  //          << " BoundaryFile = " << boundFile << " and "
  //          << " ParticleFile = " << partFile << std::endl;
  // dem->deposit(boundFile.c_str(), partFile.c_str());
  dem->deposit(boundFile, partFile.c_str());
}
