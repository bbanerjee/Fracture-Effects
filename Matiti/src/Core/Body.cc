#include <Core/Body.h>
#include <Core/Node.h>
#include <Core/Bond.h>
#include <Containers/NodePArray.h>
#include <Containers/BondPArray.h>
#include <Containers/LoadBCSPArray.h>
#include <Core/Element.h>
#include <BoundaryConditions/LoadBC.h>
#include <BoundaryConditions/LoadBCFactory.h>
#include <Core/Exception.h>
#include <GeometryPiece/GeometryPiece.h>
#include <GeometryPiece/GeometryPieceFactory.h>
#include <Core/ProblemSpec/ProblemSpec.h>

//#include <random>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace Matiti;

   
Body::Body()
  : d_id(0), d_mat_id(0)
{
  d_nodes.reserve(1000);
  d_elements.reserve(1000);
}

Body::~Body()
{
}

void 
Body::initialize(Uintah::ProblemSpecP& ps,
                 const Domain& domain,
                 const SimulationState& state,
                 const MaterialSPArray& matList)
{
  if (!ps) return;
  
  // Read the material tag from the input file
  readMaterialInput(ps, matList);

  // Get the geometry (from input node and element files)
  GeometryPiece* geom = GeometryPieceFactory::create(ps, d_nodes, d_elements);

  // Set initial horizon.  This is recomputed correctly later.
  SCIRun::Vector cell_size = domain.cellSize();
  setInitialNodeHorizon(std::max(std::max(cell_size[0], cell_size[1]), cell_size[2]));

  // Assign nodal materials  (each node starts of with the same material but material properties 
  // may evolve independently and may be based on a probability distribution)
  assignNodeMaterial(matList);

  // Compute nodal volumes using adjacant element information
  computeNodalVolumes();

  // Create the cell-node map for the family computer
  initializeFamilyComputer(domain);

  // Read the initial conditions for each body
  // 1) velocity 2) body force 3) initial cracks
  d_ic.readInitialConditions(ps);
  d_ic.applyInitialVelocity(d_nodes);

  // Read the external force boundary conditions (displacement/velocity bcs may be added later)
  // The BC information is used to update the external force on particles/nodes.
  // Not all simulations will have applied external forces
  LoadBCSPArray load_bcs;
  Uintah::ProblemSpecP bc_ps = ps->findBlock("BoundaryConditions");
  if (bc_ps) {
    for (Uintah::ProblemSpecP force_ps = bc_ps->findBlock("LoadBC"); force_ps != 0;
         force_ps = force_ps->findNextBlock("LoadBC")) {
      LoadBCSP load = LoadBCFactory::create(force_ps); 
      load->initialize(force_ps, d_nodes, d_elements);
      load_bcs.emplace_back(load);
    }
  }

  // delete geometry piece
  delete geom;
    
}

void
Body::readMaterialInput(Uintah::ProblemSpecP& ps,
                        const MaterialSPArray& matList)
{
  // Find the Material block
  Uintah::ProblemSpecP material_ps = ps->findBlock("Material");
  if (!material_ps) {
    throw Exception("A body must have a Material tag and an associated material.", __FILE__, __LINE__);
  }

  // Find the Material name
  Uintah::ProblemSpecP mat_ps = material_ps->findBlock("material");
  if (!mat_ps) {
    throw Exception("A body must have a material assigned to it", __FILE__, __LINE__);
  }

  std::string mat_name;
  if (!(mat_ps->getAttribute("name", mat_name))) {
    throw Exception("A material assigned to a body does not have a name", __FILE__, __LINE__);
  }

  // Check if the name corresponds to an actual material
  bool found_mat_name = false;
  for (auto iter = matList.begin(); iter != matList.end(); iter++) {
    Material* mat = (*iter).get();
    if (mat_name == mat->name()) {
      found_mat_name = true;
      d_mat_id = mat->id();
      break;
    }
  }
  if (!found_mat_name) {
    throw Exception("Material assigned to the body does not have a known name", __FILE__, __LINE__);
  }

  // Read the distribution and range of values
  d_mat_dist = "constant";
  d_mat_cov = 0.0;
  d_mat_seed = 0.0;
  material_ps->get("distribution", d_mat_dist);
  material_ps->get("coeff_of_variation", d_mat_cov);
  material_ps->get("seed", d_mat_seed);
  if (d_mat_dist.compare("constant") != 0 && d_mat_dist.compare("uniform") != 0 && 
      d_mat_dist.compare("gaussian") != 0) {
    throw Exception("Unknown probability distribution specified", __FILE__, __LINE__);
  }
}


void
Body::setInitialNodeHorizon(const double horizon)
{
  for (auto iter = d_nodes.begin(); iter != d_nodes.end(); ++iter) {
    (*iter)->horizonSize(horizon);
  }
}


// general method to calculate the volume of each node in any type of grid (uniform and non-uniform)
void 
Body::computeNodalVolumes()
{
  for (auto node_iter = d_nodes.begin(); node_iter != d_nodes.end(); ++node_iter) {

    NodeP cur_node = *node_iter;

    // Omit nodes that have no adjacanet elements
    if (cur_node->numAdjacentElements() == 0) {
      cur_node->omit(true);
      cur_node->volume(0.0);
      continue;
    }

    const ElementPArray& adj_elems = cur_node->getAdjacentElements();
    double vol = 0.0;

    for (auto elem_iter = adj_elems.begin(); elem_iter != adj_elems.end(); ++elem_iter) {

      double elem_vol = (*elem_iter)->volume();
      int num_nodes = (*elem_iter)->numNodes();
      vol += elem_vol/(double) num_nodes;
    }

    cur_node->volume(vol);
    //std::cout << "Node = " << (*node_iter)->getID() << " volume = " << vol 
    //          << " set vol = " << (*node_iter)->volume() << std::endl;
  }
}

void 
Body::initializeFamilyComputer(const Domain& domain)
{
  if (d_nodes.size() == 0) {
    std::ostringstream out;
    out << "The body has to be discretized into nodes before a cell-node map can be created" << std::endl;
    throw Exception(out.str(), __FILE__, __LINE__);
  }  
  d_family_computer.createCellNodeMap(domain, d_nodes);
  //d_family_computer.printCellNodeMap();
}

void 
Body::createInitialFamily(const Domain& domain)
{
  // Loop through the nodes in the body
  std::cout << "Computing initial family for nodes in body " << this << std::endl;
  for (auto iter = d_nodes.begin(); iter != d_nodes.end(); ++iter) {
    NodeP cur_node = *iter;
    if (cur_node->omit()) continue;

    // Use the FamilyComputer to get a list of neighbors inside horizon of cur_node
    NodePArray neighbor_list;
    d_family_computer.getInitialFamily(cur_node, domain, neighbor_list);
    //cur_node->setFamily(neighbor_list);
    cur_node->initialFamilySize((int) neighbor_list.size());

    // Create bonds
    BondPArray bond_list;
    for (auto fam_iter = neighbor_list.begin(); fam_iter != neighbor_list.end(); ++fam_iter) {
      NodeP fam_node = *fam_iter;
      bond_list.emplace_back(std::make_shared<Bond>(cur_node, fam_node, cur_node->material(),
                                                    fam_node->material()));
    }
    cur_node->setBonds(bond_list);
  }

  // Print the family
  // printFamily();
}

void 
Body::updateFamily(const Domain& domain)
{
  // Loop through the nodes in the body
  for (auto iter = d_nodes.begin(); iter != d_nodes.end(); ++iter) {
    NodeP cur_node = *iter;
    if (cur_node->omit()) continue;

    // Use the FamilyComputer to get a list of neighbors inside horizon of cur_node
    NodePArray neighbor_list;
    d_family_computer.getCurrentFamily(cur_node, domain, neighbor_list);
    //cur_node->setFamily(neighbor_list);

    // Create bonds
    BondPArray bond_list;
    for (auto fam_iter = neighbor_list.begin(); fam_iter != neighbor_list.end(); ++fam_iter) {
      NodeP fam_node = *fam_iter;
      bond_list.emplace_back(std::make_shared<Bond>(cur_node, fam_node, cur_node->material(),
                                                    fam_node->material()));
    }
    cur_node->setBonds(bond_list);
  }
}

// Update the damage index of each node
void
Body::updateDamageIndex() const
{
  for (auto iter = d_nodes.begin(); iter != d_nodes.end(); ++iter) {
    NodeP cur_node = *iter;
    if (cur_node->omit()) continue;
    cur_node->updateDamageIndex();
  }
}

/*
void
Body::assignNodeMaterial(const MaterialSPArray& matList)
{
  // Assign materials
  for (auto mat_iter = matList.begin(); mat_iter != matList.end(); mat_iter++) {
    Material* mat = (*mat_iter).get();
    if (d_mat_id == mat->id()) {

      for (auto node_iter = d_nodes.begin(); node_iter != d_nodes.end(); ++node_iter) {
        if ((*node_iter)->omit()) continue;
        (*node_iter)->assignMaterial(mat);
      }
      break;
    }
  }
}
*/

void
Body::assignNodeMaterial(const MaterialSPArray& matList)
{
  // Assign materials
  for (auto mat_iter = matList.begin(); mat_iter != matList.end(); mat_iter++) {
    Material* mat = (*mat_iter).get();
    if (d_mat_id == mat->id()) {

      if (d_mat_dist == "constant") {
        for (auto node_iter = d_nodes.begin(); node_iter != d_nodes.end(); ++node_iter) {
          if ((*node_iter)->omit()) continue;
          (*node_iter)->assignMaterial(mat);
        }
      } else if (d_mat_dist == "uniform") {
        // Initialize random number generator 
        std::default_random_engine generator(d_mat_seed);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);

        for (auto node_iter = d_nodes.begin(); node_iter != d_nodes.end(); ++node_iter) {
          if ((*node_iter)->omit()) continue;

          // Get random number between -1.0 and 1.0
          double random_num = dist(generator);
          (*node_iter)->assignMaterial(mat, random_num, d_mat_cov);
        }

      } else if (d_mat_dist == "gaussian") {
        // Initialize random number generator 
        std::default_random_engine generator(d_mat_seed);
        std::normal_distribution<double> dist(0.0, 1.0);

        for (auto node_iter = d_nodes.begin(); node_iter != d_nodes.end(); ++node_iter) {
          if ((*node_iter)->omit()) continue;

          // Get random number between -1.0 and 1.0
          double random_num = dist(generator);
          //std::cout << "randon_num = " << random_num << std::endl;
          (*node_iter)->assignMaterial(mat, random_num, d_mat_cov);
        }

      } else {
        std::ostringstream out;
        out << "Material property distribution " << d_mat_dist << " unknown" << std::endl;
        throw Exception(out.str(), __FILE__, __LINE__);
      }

      break;
    }
  }
}

void 
Body::printFamily()
{
  for (auto iter = d_nodes.begin(); iter != d_nodes.end(); ++iter) {

    NodeP cur_node = *iter;
    if (cur_node->omit()) continue;

    BondPArray bonds = cur_node->getBonds();
    NodePArray neighbor_list;
    for (auto iter = bonds.begin(); iter != bonds.end(); ++iter) {
      neighbor_list.emplace_back((*iter)->second());
    }
    
    int count = 0;
    std::cout << "Current node = " << cur_node->getID() << " Neighbors = " ;
    for (auto fam_iter = neighbor_list.begin(); fam_iter != neighbor_list.end(); fam_iter++) {
      std::cout <<  "[" << ++count << "]:"<< (*fam_iter)->getID() << ", ";
    } 
    std::cout << std::endl;
  }
}

namespace Matiti {

  std::ostream& operator<<(std::ostream& out, const Body& body)
  {
    out.setf(std::ios::floatfield);
    out.precision(6);
    out << "Body:" << body.d_id << std::endl;
    out << "  Material = " << body.d_mat_id << std::endl;
    out << "    Dist = " << body.d_mat_dist << " COV = " << body.d_mat_cov << " Seed = " << body.d_mat_seed
        << std::endl;
    for (auto iter = (body.d_nodes).begin(); iter != (body.d_nodes).end(); ++iter) {
      if ((*iter)->omit()) continue;
      out << *(*iter) << std::endl ;
    }
    for (auto iter = (body.d_elements).begin(); iter != (body.d_elements).end(); ++iter) {
      out << "  " << *(*iter) << std::endl ;
    }
    out << "  " << body.d_ic;
    return out;
  }
}
