#include <Boundary/Boundary.h>
#include <DiscreteElements/DEMParticle.h>
// use both pointer to and variable of class DEMParticle

namespace dem {

Boundary::Boundary()
{
  b_numContacts = 0;
  b_normalForce = 0;
  b_tangentForce = 0;
  b_penetration = 0;
}

void
Boundary::print(std::ostream& os)
{
  os << std::endl
     << std::setw(OWID) << static_cast<int>(b_type) 
     << std::setw(OWID) << b_extraNum << std::endl
     << std::setw(OWID) << static_cast<int>(b_id);
}


void
Boundary::clearStatForce()
{
  b_numContacts = 0;
  b_normalForce = 0;
  b_tangentForce = 0;
  b_penetration = 0;
}

void
Boundary::updateStatForce()
{
  clearStatForce();
  b_numContacts = b_contacts.size();
  for (auto& boundaryContact : b_contacts) {
    b_normalForce += boundaryContact.bc_normalForce;
    b_tangentForce += boundaryContact.bc_tangentForce;
    b_penetration += boundaryContact.bc_penetration;
  }
  if (b_numContacts != 0)
    b_penetration /= b_numContacts;
}

void
Boundary::clearBoundaryContacts()
{
  b_probableBoundaryParticles.clear();
  b_contacts.clear();
}

} // namespace dem ends
