#ifndef EMU2DC_DAMAGE_MODEL_H
#define EMU2DC_DAMAGE_MODEL_H

#include <Types.h>
#include <Core/ProblemSpec/ProblemSpecP.h>
#include <iostream>

namespace Emu2DC {

  class DamageModel 
  {
  public:

    friend std::ostream& operator<<(std::ostream& out, const Emu2DC::DamageModel& dam);

  public:
  
    DamageModel();
    virtual ~DamageModel();

    void initialize(const Uintah::ProblemSpecP& ps);

    const Array3& damageViscosity() const {return d_damage_viscosity;}
    inline double damageIndex() const {return d_damage_index;}
    const Array3& damageStretch() const {return d_damage_stretch;}

  protected:

    Array3 d_damage_viscosity;
    double d_damage_index;
    Array3 d_damage_stretch;

  }; // end class

} // end namespace

#endif