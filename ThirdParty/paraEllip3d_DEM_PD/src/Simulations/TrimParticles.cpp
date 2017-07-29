#include <Simulations/TrimParticles.h>

using namespace dem;
void
TrimParticles::execute(DiscreteElements* dem)
{
  if (dem->getMPIRank() == 0) {
    dem->readBoundary(
      Parameter::get().datafile["boundaryFile"].c_str());
    dem->trim(
      true, Parameter::get().datafile["particleFile"].c_str(),
      "trim_particle_end");
  }
}
