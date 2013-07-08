#ifndef __EMU2DC_BOX_GEOMETRY_PIECE_H__
#define __EMU2DC_BOX_GEOMETRY_PIECE_H__

#include <GeometryPiece/GeometryPiece.h>
#include <Types.h>
#include <NodePArray.h>
#include <ElementPArray.h>
#include <map>

#include <Core/ProblemSpec/ProblemSpecP.h>

namespace Emu2DC 
{

  class BoxGeometryPiece : public GeometryPiece
  {
  public:

    BoxGeometryPiece(Uintah::ProblemSpecP& ps, NodePArray& nodes, ElementPArray& elements);
    virtual ~BoxGeometryPiece();

    Box3D boundingBox() const;

    bool inside (const Point3D& pt) const;

    std::string name() const;

  protected:

    void createNodes(NodePArray& nodes);

    void createElements(ElementPArray& elem);

    void findNodalAdjacentElements(ElementPArray& elements);

  private:

    Box3D d_box;
    IntArray3 d_num_elements;

    typedef std::map<int, NodeP> NodeIDMap;
    NodeIDMap d_id_ptr_map;

  }; // end class

} // end namespace

#endif