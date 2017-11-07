#ifndef DEM_PARTICLE_CREATOR_H
#define DEM_PARTICLE_CREATOR_H

#include <DiscreteElements/DEMContainers.h>
#include <DiscreteElements/DEMParticle.h>
#include <Core/Geometry/Box.h>

namespace dem {

class DEMParticleCreator
{
public:

  struct ParticleParameters {
    REAL youngModulus;
    REAL poissonRatio;
    REAL maxDiameter;
    REAL edge;
    REAL offset;
  };

  DEMParticleCreator() = default;
  ~DEMParticleCreator() = default;

  DEMParticlePArray generateDEMParticles(std::size_t layerFlag,
                                         DEMParticle::DEMParticleShape,
                                         const dem::Box& spatialDomain,
                                         dem::Gradation& gradation);

  DEMParticlePArray generatePeriodicDEMParticles(const DEMParticlePArray& parts,
                                                 const dem::Box& spatialDomain);

private:

  ParticleParameters getParticleParameters(const dem::Gradation& gradation);

  template <std::size_t layerFlag>
  DEMParticlePArray generateDEMParticles(DEMParticle::DEMParticleShape type,
                                         const dem::Box& spatialDomain,
                                         dem::Gradation& gradation);

  template <std::size_t layerFlag>
  DEMParticlePArray createParticles(DEMParticle::DEMParticleShape type,
                                    const dem::Box& spatialDomain, 
                                    dem::Gradation& gradation, 
                                    const ParticleParameters& params);

  void removeDuplicates(DEMParticlePArray& input);
};

} // End namespace dem

#endif