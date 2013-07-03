#include <Peridynamics.h>
#include <FamilyComputer.h>
#include <Material.h>
#include <Body.h>
#include <Node.h>
#include <Bond.h>
#include <Exception.h>

#include <Core/ProblemSpec/ProblemSpec.h>

#include <memory>
#include <chrono>
#include <fstream>
#include <iostream>
#include <unistd.h>

using namespace Emu2DC;

Peridynamics::Peridynamics() 
{
}

Peridynamics::~Peridynamics() 
{
}

void
Peridynamics::problemSetup(Uintah::ProblemSpecP& ps)
{
  // Set up the simulation state data
  d_state.initialize(ps);
  // std::cout << d_state ;

  // Set up the time information
  d_time.initialize(ps);
  // std::cout << d_time ;

  // Set up the output information
  d_output.initialize(ps);
  // std::cout << d_output ;

  // Set up the domain
  d_domain.initialize(ps);
  // std::cout << d_domain ;

  // Set up the initial material list
  int count = 0;
  for (Uintah::ProblemSpecP mat_ps = ps->findBlock("Material"); mat_ps != 0;
       mat_ps = mat_ps->findNextBlock("Material")) {
    MaterialSP mat = std::make_shared<Material>();
    mat->initialize(mat_ps); 
    mat->id(count);
    d_mat_list.emplace_back(mat);
    ++count;
    // std::cout << *mat ;
  }

  // Set up the body information
  count = 0;
  for (Uintah::ProblemSpecP body_ps = ps->findBlock("Body"); body_ps != 0;
       body_ps = body_ps->findNextBlock("Body")) {

    // Initialize the body (nodes, elements, cracks)
    BodySP body = std::make_shared<Body>();
    body->initialize(body_ps, d_domain, d_state, d_mat_list); 
    body->id(count);
    d_body_list.emplace_back(body);
    ++count;
    // std::cout << *body;
 
    // Compute the horizons of nodes in the body
    HorizonComputer compute_horizon;
    compute_horizon(body, d_state);
  }

  // Create the initial family of each node and remove any bonds
  // intersected by initial cracks
  for (auto iter = d_body_list.begin(); iter != d_body_list.end(); ++iter) {

    // Create family
    (*iter)->createInitialFamily(d_domain);

    // Remove bonds
    (*iter)->removeBondsIntersectedByCracks();

    // Print body information
    std::cout << *(*iter);
  }

  d_num_broken_bonds = 0;
}

void
Peridynamics::run()
{
  std::cerr << "....Begin Solver...." << std::endl;

  // Constants
  Vector3D Zero(0.0, 0.0, 0.0);

  // Write the output at the beginning of the simulation
  d_output.write(d_time, d_domain, d_body_list);

  // Displacement driven computation. Body alreay has initial velocity.  
  // Compute initial displacement
  applyInitialConditions();

  // Do time incrementation
  double cur_time = 0.0;
  int cur_iter = 1;
  while (cur_time < d_time.maxTime() && cur_iter < d_time.maxIter()) {

    auto t1 = std::chrono::high_resolution_clock::now();

    // Get the current delT
    double delT = d_time.delT();

    // Do the computations separately for each body
    for (auto body_iter = d_body_list.begin(); body_iter != d_body_list.end(); ++body_iter) {

      // Get body force per unit volume
      Vector3D body_force = (*body_iter)->bodyForce();

      // Update nodal velocity and nodal displacement
      // 1. v(n+1/2) = v(n) + dt/2m * f(u(n))
      // 2. u(n+1) = u(n) + dt * v(n+1/2)
      const NodePArray& node_list = (*body_iter)->nodes();
      for (auto node_iter = node_list.begin(); node_iter != node_list.end(); ++node_iter) {

        // Get the node
        NodeP cur_node = *node_iter;
        if (cur_node->omit()) {
          cur_node->oldDisplacement(cur_node->displacement());
          cur_node->velocity(Zero);
          cur_node->displacement(Zero);
          cur_node->newVelocity(Zero);
          cur_node->newDisplacement(Zero);
          continue;  // skip this node
        }

        // Compute the internal force at the node 
        Vector3D internal_force(0.0, 0.0, 0.0);
        computeInternalForce(cur_node, internal_force);

        // Apply external and body forces
        Vector3D external_force = cur_node->externalForce();
        external_force += (body_force*cur_node->density());

        // Apply any external forces due to contact  **TODO**
        //applyContactForces();
        
        // Compute acceleration (F_ext - F_int = m a)
        // **TODO** Make sure mass is conserved
        //std::cout << "F_ext = " << external_force << " F_int = " << internal_force
        //          << " density = " << cur_node->density() 
        //          << " volume = " << cur_node->volume() << std::endl;
        Vector3D acceleration = (external_force + internal_force)/cur_node->density();
        // Integrate acceleration with velocity Verlet algorithm
        // and Update nodal velocity
        // 1. v(n+1/2) = v(n) + dt/2m * f(u(n))
        Vector3D velocity(0.0, 0.0, 0.0);
        integrateNodalAcceleration(cur_node, acceleration, 0.5*delT, velocity );

        // Integrate the mid step velocity
        // and Update nodal displacement
        // 2. u(n+1) = u(n) + dt * v(n+1/2)
        Vector3D displacement(0.0, 0.0, 0.0);
        integrateNodalVelocity(cur_node, velocity, delT, displacement);

        // Update vel_new and disp_new
        cur_node->newVelocity(velocity);
        cur_node->newDisplacement(displacement);

        std::cout << "After Stage 1: Node = " << cur_node->getID() 
                  << " vel_old = " << cur_node->velocity() 
                  << " vel_new = " << cur_node->newVelocity() 
                  << " disp_old = " << cur_node->displacement() 
                  << " disp_new = " << cur_node->newDisplacement() 
                  << std::endl;
      }

    }
    
    // Apply domain boundary conditions to the body
    // Update kinematic quantities
    for (auto body_iter = d_body_list.begin(); body_iter != d_body_list.end(); ++body_iter) {
      d_domain.applyVelocityBC(*body_iter);

      const NodePArray& node_list = (*body_iter)->nodes();
      for (auto node_iter = node_list.begin(); node_iter != node_list.end(); ++node_iter) {
        NodeP cur_node = *node_iter;
        cur_node->oldDisplacement(cur_node->displacement());
        cur_node->displacement(cur_node->newDisplacement());
        cur_node->velocity(cur_node->newVelocity());
      }
    }

    // Do the computations separately for each body
    for (auto body_iter = d_body_list.begin(); body_iter != d_body_list.end(); ++body_iter) {

      // Get body force per unit volume
      Vector3D body_force = (*body_iter)->bodyForce();

      // Update nodal velocity
      //   3. v(n+1) = v(n+1/2) + dt/2m * f(q(n+1))
      const NodePArray& node_list = (*body_iter)->nodes();
      for (auto node_iter = node_list.begin(); node_iter != node_list.end(); ++node_iter) {

        // Get the node
        NodeP cur_node = *node_iter;

        std::cout << "Before stage 2 : Node = " << cur_node->getID() << " vel = " << cur_node->velocity() << " disp = " << cur_node->displacement() << std::endl;

        // Compute updated internal force from updated nodal displacements
        Vector3D internal_force(0.0, 0.0, 0.0);
        computeInternalForce(cur_node, internal_force);

        // Apply external and body forces
        Vector3D external_force = cur_node->externalForce();
        external_force += (body_force*cur_node->density());

        // Compute acceleration (F_ext - F_int = m a)
        Vector3D acceleration = (external_force + internal_force)/cur_node->density();
        // Integrate acceleration with velocity Verlet algorithm
        // and Update nodal velocity
        //   3. v(n+1) = v(n+1/2) + dt/2m * f(q(n+1))
        Vector3D velocity(0.0, 0.0, 0.0);
        integrateNodalAcceleration(cur_node, acceleration, 0.5*delT, velocity);
        cur_node->newVelocity(velocity);

        std::cout << "After stage 2 : Node = " << cur_node->getID() << " vel = " << cur_node->newVelocity() << " disp = " << cur_node->newDisplacement() << std::endl;
      }

      // Print body information
      std::cout << *(*body_iter);
    }

    // Update the velocity and update delT
    double delT_new = delT;
    for (auto body_iter = d_body_list.begin(); body_iter != d_body_list.end(); ++body_iter) {

      // Loop through nodes in the body
      const NodePArray& node_list = (*body_iter)->nodes();
      for (auto node_iter = node_list.begin(); node_iter != node_list.end(); ++node_iter) {

        // Get the node
        NodeP cur_node = *node_iter;

        // Update kinematic quantities
        cur_node->velocity(cur_node->newVelocity());

        // Compute stable delT
        delT_new = std::min(cur_node->computeStableTimestep(d_time.timeStepFactor()), delT_new);
      }

      // Break bonds
      breakBonds(node_list);
    }

    // Get memory usage
    double res_mem = 0.0, shar_mem = 0.0;
    checkMemoryUsage(res_mem, shar_mem);

    // Print out current time and iteration
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << "Time = " << cur_time << " Iteration = " << cur_iter 
              << " Compute time (millisec) = " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() 
              << " Memory (kB) = " << res_mem << "-" << shar_mem << std::endl;

    // Update the current time and current iteration
    cur_time = d_time.incrementTime(delT);
    d_time.setDelT(delT_new);
    ++cur_iter;

    // Output nodal information every snapshots_frequency iteration   
    int output_freq = d_output.outputIteratonInterval();
    if (cur_iter%output_freq == 0) {
      d_output.write(d_time, d_domain, d_body_list);
      std::cout << "Wrote out data at time " << d_time << std::endl;
    }
  }
}

// Apply the initial velocity 
void
Peridynamics::applyInitialConditions()
{
  // Compute initial displacement
  for (auto body_iter = d_body_list.begin(); body_iter != d_body_list.end(); ++body_iter) {

    // Get the initial velocity
    const Vector3D& init_vel = (*body_iter)->initialVelocity();
    
    // Loop through nodes in the body
    const NodePArray& node_list = (*body_iter)->nodes();
    for (auto node_iter = node_list.begin(); node_iter != node_list.end(); ++node_iter) {
      if ((*node_iter)->omit()) continue;
      (*node_iter)->velocity(init_vel);
    }
  }
}

// Compute the internal force 
void
Peridynamics::computeInternalForce(const NodeP& cur_node,
                                   Vector3D& internalForce)
{
  // Initialize strain energy and spsum
  double strain_energy = 0.0;
  double spsum = 0.0;

  // **WARNING** For now Family is fixed at start and is not recomputed each time step
  // Get the family of node mi (all the nodes within its horizon, delta).
  const BondPArray& bonds = cur_node->getBonds();
  //std::cout << " Node = " << cur_node->getID() << " Bonds = " << std::endl;
  //for (auto iter = bonds.begin(); iter != bonds.end(); ++iter) {
  //    std::cout << *(*iter);
  //}
  
  //const NodePArray& family_nodes = cur_node->getFamily();
  //const MaterialSPArray& bond_materials = cur_node->getBondMaterials();

  // Loop over the family of current node mi.
  for (auto family_iter = bonds.begin(); family_iter != bonds.end(); family_iter++) {

    BondP bond = *family_iter;

    const NodeP& family_node = bond->second();
    if (family_node->omit()) continue;  // skip this node

    // Find the peridynamic interparticle force.
    // = force density per unit volume due to peridynamic interaction between nodes
    bond->computeInternalForce();

    // Sum up the force on node mi due to all the attached bonds.
    // force at the current configuration (n+1)
    internalForce += bond->internalForce();

    strain_energy += bond->computeStrainEnergy();
    spsum += (bond->computeMicroModulus()/(cur_node->material())->density());
  }
  cur_node->internalForce(internalForce);
  cur_node->strainEnergy(strain_energy);
  cur_node->spSum(spsum);
  //std::cout << *cur_node;
}

// Integrates the node accelerations due to peridynamic ("structured") interaction.
// Update the node velocities
// [one-step Velocity-Verlet formulation]
// 1. v(n+1/2) = v(n) + dt/2m * f(q(n))
// 2. q(n+1) = q(n) + dt * v(n+1/2)
// 3. v(n+1) = v(n+1/2) + dt/2m * f(q(n+1))
void
Peridynamics::integrateNodalAcceleration(const NodeP& node,
                                         const Vector3D& acceleration,
                                         double delT,
                                         Vector3D& vel_new)
{
  const Vector3D& vel_old = node->velocity();
  vel_new = vel_old + acceleration*delT;
  if (acceleration.isnan() || vel_old.isnan()) {
    std::ostringstream out;
    out << "Acceleration/old velocity is nan.  Vel = " << vel_old << " acc = " << acceleration ;
    throw Exception(out.str(), __FILE__, __LINE__);
  }
}

void
Peridynamics::integrateNodalVelocity(const NodeP& node,
                                     const Vector3D& velocity,
                                     double delT,
                                     Vector3D& disp_new)
{
  const Vector3D& disp_old = node->displacement();
  disp_new = disp_old + velocity*delT;
}

void 
Peridynamics::breakBonds(const NodePArray& nodes)
{
  for (auto iter = nodes.begin(); iter != nodes.end(); iter++) {
    NodeP cur_node = *iter;
    if (cur_node->omit()) continue;  // skip this node

    // Break bonds and update the damage index
    cur_node->findAndDeleteBrokenBonds();
  }
}

void
Peridynamics::checkMemoryUsage(double& resident_mem, double& shared_mem)
{
  int tSize = 0, resident = 0, share = 0;
  std::ifstream buffer("/proc/self/statm");
  buffer >> tSize >> resident >> share;
  buffer.close();

  long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
  resident_mem = resident * page_size_kb;
  shared_mem = share * page_size_kb;
}

