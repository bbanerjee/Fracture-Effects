#ifndef MATITI_MESHNODE_H
#define MATITI_MESHNODE_H

#include <Vaango/Core/Geometry/Point.h>
#include <Mesh/MeshBond.h>

#include <vector>

namespace Matiti {

  class MeshNode
  {
    public:
      MeshNode(const long int& id, const double& x, const double& y);
      MeshNode(const long int& id, const double& x, const double& y, const double& z);
      MeshNode(const long int& id, const SCIRun::Point& pt);
      ~MeshNode();
      long int MeshNode::id() const
      SCIRun::Point& MeshNode::position() const

    private:
      long int d_id;
      double   d_x;
      double   d_y;
      double   d_z;
      double   d_horizon;
      bool     d_hangingNode;  // true => handing node
      std::vector<MeshBond*> d_bonds;

      // prevent blank creation and copying
      MeshNode();
      MeshNode(const MeshNode& node);

  };
} // End namespace Matiti

#endif // MATITI_MESHNODE_H