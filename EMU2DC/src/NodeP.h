#ifndef EMU2DC_NODEP_H
#define EMU2DC_NODEP_H

#include <memory>

namespace Emu2DC {
  
  // Original definition
  /* 
     template<class T> class Handle;
     class Node;
     typedef Handle<Node> NodeP; 
  */

  // New definition
  // Forward declaration.  Make sure <Node.h> is included before using NodeP.
  // using stdlib shared_ptr instead of SCIRun::Handle
  class Node;
  typedef std::shared_ptr<Node> NodeP;
}

#endif
