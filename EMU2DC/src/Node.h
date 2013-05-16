#ifndef __EMU2DC_NODE_H__
#define __EMU2DC_NODE_H__

#include <ElementPArray.h>
#include <BondPArray.h>
#include <MaterialUP.h>
#include <Material.h>

//#include <NodePArray.h>
//#include <MaterialUPArray.h>

#include <Types.h>
#include <Geometry/Point3D.h>
#include <Geometry/Vector3D.h>
#include <iostream>
#include <cmath>

namespace Emu2DC {
    
  // This structure defines the node type
  class Node {

    public:

      friend std::ostream& operator<<(std::ostream& out, const Emu2DC::Node& node);

    public:

      Node();
      Node(const int id, const double xx, const double yy, const double zz, const int surfaceNode);
      Node(const Node& node);
      ~Node();

      bool operator<(const Node& node) const { return (d_id < node.d_id); }

      /**
       * Compute initial displacement
       * Inputs: initial velocity
       *         time increment
       * Effect: Changes nodal old_displacement and nodal velocity
       */
      void computeInitialDisplacement(const Vector3D& initVel, double delT);

      inline void dimension(const int dim) {d_dimension = dim;}
      inline int dimension() const {return d_dimension;}

      inline void omit(const bool& omit) { d_omit = omit; }
      inline bool omit() const { return d_omit; }

      inline void onSurface(const bool& flag) { d_surfaceNode = flag; }
      inline bool onSurface() const { return d_surfaceNode; }
      
      inline const long64& getID() const { return d_id; }
      inline void setID(const long64& id) { d_id = id; }

      inline const int& matType() const { return d_mat_type; }
      inline void matType(const int& mat_type) { d_mat_type = mat_type; }

      inline const double& horizonSize() const { return d_horizon_size; }
      inline void horizonSize(const double& horizon_size) { d_horizon_size = horizon_size; }

      inline const double& volume() const { return d_volume; }
      inline void volume(const double& volume) { d_volume = volume; }

      /**
       * Assign node material
       * ** WARNING** This is only for the initial setup before bonds are computed.
       */
      void assignMaterial(const Material* mat) { d_material->clone(mat);}
      const Material* material() const {return d_material.get();}
      double density() const {return d_material->density();}

      inline const Point3D& position() const { return d_pos; }
      inline void position(const Point3D& pos)  { d_pos = pos; }

      inline const Vector3D& displacement() const { return d_disp; }
      inline void displacement(const Vector3D& disp)  { d_disp = disp; }

      inline const Vector3D& oldDisplacement() const { return d_old_disp; }
      inline void oldDisplacement(const Vector3D& disp)  { d_old_disp = disp; }

      inline const Vector3D& newDisplacement() const { return d_new_disp; }
      inline void newDisplacement(const Vector3D& disp)  { d_new_disp = disp; }

      inline const Vector3D& velocity() const { return d_veloc; }
      inline void velocity(const Vector3D& veloc)  { d_veloc = veloc; }

      inline const Vector3D& acceleration() const { return d_accel; }
      inline void acceleration(const Vector3D& accel)  { d_accel = accel; }

      inline const Vector3D& externalForce() const { return d_ext_force; }
      inline void externalForce(const Vector3D& extForce)  { d_ext_force = extForce; }

      inline int numAdjacentElements() const { return d_adjacent_elements.size(); }

      inline double distance(const Node& node) const
      {
        double dx = d_pos.x() - node.d_pos.x();
        double dy = d_pos.y() - node.d_pos.y();
        double dz = d_pos.z() - node.d_pos.z();
        return std::sqrt(dx*dx + dy*dy + dz*dz);
      }

      const ElementPArray& getAdjacentElements() const
      {
        return d_adjacent_elements;
      }

      const ElementP& getAdjacentElement(const int& index) const
      {
        return d_adjacent_elements[index];
      }

      inline void addAdjacentElement(const ElementP& elem) 
      {
        d_adjacent_elements.push_back(elem);
      }

      // Node "family" = neighbor list access methods
      void setBonds(const BondPArray& fam) { d_bonds.clear(); d_bonds = fam; }
      const BondPArray& getBonds() const {return d_bonds;}

      // void setFamily(const NodePArray& fam);
      // const NodePArray& getFamily() const {return d_neighbor_list;}
      // const MaterialUPArray& getBondMaterials() const {return d_bond_materials;}

      void initialFamilySize(const int size) {d_initial_family_size = size;}
      int initialFamilySize() const {return d_initial_family_size;}
      int currentFamilySize() const {return (int) d_bonds.size();}

      /**
       *  Find and delete broken bonds
       */
      void findAndDeleteBrokenBonds();

      /**
       *  Damage index access methods
       */
      void updateDamageIndex();
      double damageIndex() const {return d_damage_index;}

      /**
       * Store some data in case it's needed later
       */
      void internalForce(const Vector3D& internalForce) {d_int_force = internalForce;}
      void strainEnergy(double energy) {d_strain_energy = energy;}
      void spSum(double spsum) {d_sp_sum = spsum;}

    private:

      int d_dimension;
      long64 d_id;
      int d_mat_type;
      double d_horizon_size;
      bool d_omit;         // Omit this node from the computation if true
      bool d_surfaceNode;  // This node is on the surface of the body if true
      double d_volume;
      MaterialUP d_material;  // For initial setup  **WARNING** Potential problems.

      ElementPArray d_adjacent_elements; // The elements adjacent to this node, 
      
      // Using array of structs instead of struct of arrays
      BondPArray d_bonds;                // The bonds attached to this node
      int d_initial_family_size;

      // NodePArray d_neighbor_list;        // The nodes inside the horizon of this node
      // MaterialUPArray d_bond_materials;  // One material per bond to store history

      Point3D d_pos;  // TODO: make into array 
      Vector3D d_disp;  // TODO: make into array
      Vector3D d_veloc;  // TODO: make into array
      Vector3D d_accel;  // TODO: make into array
      Vector3D d_new_veloc;  // TODO: make into array
      Vector3D d_new_disp;  // TODO: make into array
      Vector3D d_old_disp;  // TODO: make into array
      Vector3D d_ext_force;  // TODO: make into array

      // Not really necessary but storing for now
      Vector3D d_int_force;
      double d_strain_energy;
      double d_sp_sum;

      double d_damage_index;
  };

} // end namespace

#endif
