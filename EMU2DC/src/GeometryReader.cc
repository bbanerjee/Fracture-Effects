#include <GeometryReader.h>
#include <Node.h>
#include <Element.h>
#include <Exception.h>

#include <Core/ProblemSpec/ProblemSpec.h>

#include <string>
#include <fstream>
#include <algorithm>

using namespace Emu2DC; 

GeometryReader::GeometryReader()
{
  d_xmax = std::numeric_limits<double>::min();
  d_xmin = std::numeric_limits<double>::max();
  d_ymin = d_xmin;
  d_zmin = d_xmin;
  d_num_buckets_x = 20;
}

GeometryReader::~GeometryReader()
{
}

void 
GeometryReader::readGeometryInputFiles(Uintah::ProblemSpecP& ps,
                                       NodePArray& nodes,
                                       ElementPArray& elements)
{
  // Get the geometry (from input node and element files)
  Uintah::ProblemSpecP geom_ps = ps->findBlock("Geometry");
  std::string input_surface_mesh_file;
  std::string input_volume_mesh_file;
  geom_ps->require("input_surface_mesh_file", input_surface_mesh_file);
  geom_ps->require("input_volume_mesh_file", input_volume_mesh_file);
  std::cout << "Input geometry files: " << input_surface_mesh_file << ", " 
                                        << input_volume_mesh_file << std::endl;

  // Read the input surface node file
  readSurfaceMeshNodes(input_surface_mesh_file);

  // Read the input volume mesh file for nodes and elements
  readVolumeMeshNodesAndElements(input_volume_mesh_file, nodes, elements);

  // Find adjacent elements for each node in the volume mesh
  findNodalAdjacentElements(elements);

  // Find surface nodes
  findSurfaceNodes(nodes);
}

//--------------------------------------------------------------------------------
// Read input triangulated surface mesh file in Abaqus format
//--------------------------------------------------------------------------------
void 
GeometryReader::readSurfaceMeshNodes(const std::string& fileName)
{
  // Try to open file
  std::ifstream file(fileName);
  if (!file.is_open()) {
    std::string out = "Could not open node input surface mesh file " + fileName + " for reading \n";
    throw Exception(out, __FILE__, __LINE__);
  }

  // Read file
  std::string line;
  bool node_flag = false;
  while (std::getline(file, line, ',')) {

    // Ignore empty lines
    if (line.empty()) continue;

    // erase white spaces from the beginning of line
    line.erase(line.begin(), std::find_if(line.begin(), line.end(), 
         std::not1(std::ptr_fun<int, int>(std::isspace))));
    
    // Skip comment lines except *Node
    if (line[0] == '*') {
      node_flag = false;
      if (line.compare("*Node") == 0) {
        node_flag = true;
      }
      continue;
    }

    // Read the nodal coordinates
    if (node_flag) {
      std::istringstream data_stream(line);
      int node_id;
      double xcoord, ycoord, zcoord;
      if (!(data_stream >> node_id >> xcoord >> ycoord >> zcoord)) {
        throw Exception("Could not read nodal coordinates from input surface mesh file", 
                         __FILE__, __LINE__);
      }
      d_surf_pts.emplace_back(Point3D(xcoord, ycoord, zcoord)); 
      d_xmax = (xcoord > d_xmax) ? xcoord : d_xmax;
      d_xmin = (xcoord < d_xmin) ? xcoord : d_xmin;
      d_ymin = (ycoord < d_ymin) ? ycoord : d_ymin;
      d_zmin = (xcoord < d_zmin) ? zcoord : d_zmin;
    }
  }

  // Create a box that surrounds the surface and
  // divide the box into a grid (hardcoded for now)
  double dx = (d_xmax - d_xmin)/(double) d_num_buckets_x;

  // Loop through nodes and add to buckets
  int node_id = 0;
  for (auto iter = d_surf_pts.begin(); iter != d_surf_pts.end(); ++iter) {
    double xx = (*iter).x();
    double yy = (*iter).y();
    double zz = (*iter).z();
    int x_cell_id = std::ceil((xx - d_xmin)/dx); 
    int y_cell_id = std::ceil((yy - d_ymin)/dx); 
    int z_cell_id = std::ceil((zz - d_zmin)/dx); 
    long64 cell_id = ((long64)x_cell_id << 16) | ((long64)y_cell_id << 32) | ((long64)z_cell_id << 48);
    d_bucket_to_node_map.insert(std::pair<long64, int>(cell_id, node_id));
    ++node_id;
  }
}

//--------------------------------------------------------------------------------
// Read input tetrahedral volume mesh file in Abaqus format
//--------------------------------------------------------------------------------
void 
GeometryReader::readVolumeMeshNodesAndElements(const std::string& fileName,
                                               NodePArray& nodes,
                                               ElementPArray& elements)
{
  // Try to open file
  std::ifstream file(fileName);
  if (!file.is_open()) {
    std::string out = "Could not open node input volume mesh file " + fileName + " for reading \n";
    throw Exception(out, __FILE__, __LINE__);
  }

  // Read file
  std::string line;
  bool node_flag = false;
  bool elem_flag = false;
  while (std::getline(file, line, ',')) {

    // Ignore empty lines
    if (line.empty()) continue;

    // erase white spaces from the beginning of line
    line.erase(line.begin(), std::find_if(line.begin(), line.end(), 
         std::not1(std::ptr_fun<int, int>(std::isspace))));
    
    // Skip comment lines except *Node
    if (line[0] == '*') {
      node_flag = false;
      elem_flag = false;
      if (line.compare("*Node") == 0) {
        node_flag = true;
      }
      std::string str("*Element");
      if (line.compare(0, str.length(), str) == 0) {
        elem_flag = true;
      }
      continue;
    }

    // Read the nodal coordinates
    if (node_flag) {
      readVolumeMeshNode(line, nodes);
    }

    // Read the element connectivity
    if (elem_flag) {
      readVolumeMeshElement(line, elements);
    }
  }
}

void
GeometryReader::readVolumeMeshNode(const std::string& inputLine,
                                   NodePArray& nodes)
{
  std::istringstream data_stream(inputLine);
  int node_id;
  double xcoord, ycoord, zcoord;
  if (!(data_stream >> node_id >> xcoord >> ycoord >> zcoord)) {
    throw Exception("Could not read nodal coordinates from input surface mesh file", 
                    __FILE__, __LINE__);
  }

  // Save the data - surface_node_flag is 0
  bool on_surface = false;
  NodeP node(new Node(node_id, xcoord, ycoord, zcoord, on_surface));
  nodes.emplace_back(node);

  // Add to the node ID -> node ptr map
  d_id_ptr_map.insert(std::pair<int, NodeP>(node_id, node));
}

void
GeometryReader::readVolumeMeshElement(const std::string& inputLine,
                                      ElementPArray& elements)
{
  // Read the element id
  std::istringstream data_stream(inputLine);
  int element_id;
  if (!(data_stream >> element_id)) {
    throw Exception("Could not read element id from element input data stream", __FILE__, __LINE__);
  }

  // Read the element node ids
  std::vector<int> node_list;
  int node;
  while (data_stream >> node) {
    node_list.emplace_back(node);
  }
  if (node_list.empty()) {
    throw Exception("Could not find nodes in element input data stream", __FILE__, __LINE__);
  }

  // Find the node pointers
  NodePArray elem_nodes;
  if (d_id_ptr_map.empty()) {
    throw Exception("Could not find node id -> node ptr map", __FILE__, __LINE__);
  }
  for (auto iter = node_list.begin(); iter != node_list.end(); ++iter) {
    int node_id = *iter;
    auto id_ptr_pair = d_id_ptr_map.find(node_id);
    if (id_ptr_pair == d_id_ptr_map.end()) {
      std::string out = "Could not find node id -> node ptr pair for node " + node_id;
      throw Exception(out, __FILE__, __LINE__);
    }
    NodeP it = id_ptr_pair->second;
    elem_nodes.emplace_back(it); 
  }
     
  // Save the data
  ElementP elem(new Element(element_id, elem_nodes));
  elem->computeVolume();
  elements.emplace_back(elem);
}

void
GeometryReader::findNodalAdjacentElements(ElementPArray& elements)
{
  // Loop thru elements and find adjacent elements for each node
  for (auto elem_iter = elements.begin(); elem_iter != elements.end(); ++elem_iter) {
    ElementP cur_elem = *elem_iter;

    // Loop thru nodes of each element
    NodePArray elem_nodes = cur_elem->nodes();
    for (auto elem_node_iter = elem_nodes.begin();
              elem_node_iter != elem_nodes.end(); ++elem_node_iter) {

      NodeP cur_elem_node = *elem_node_iter;
      cur_elem_node->addAdjacentElement(cur_elem);
    }
  }
}

void
GeometryReader::findSurfaceNodes(NodePArray& nodes)
{
  // Loop through nodes in volume mesh
  double dx = (d_xmax - d_xmin)/(double) d_num_buckets_x;
  for (auto iter = nodes.begin(); iter != nodes.end(); ++iter) {

    // Get node position
    NodeP cur_node = *iter;
    Point3D pos = cur_node->position();

    // Compute bucket id
    double xx = pos.x();
    double yy = pos.y();
    double zz = pos.z();
    int x_cell_id = std::ceil((xx - d_xmin)/dx); 
    int y_cell_id = std::ceil((yy - d_ymin)/dx); 
    int z_cell_id = std::ceil((zz - d_zmin)/dx); 
    long64 cell_id = ((long64)x_cell_id << 16) | ((long64)y_cell_id << 32) | ((long64)z_cell_id << 48);

    // Find the cell id in the bucket-node map for the surface nodes
    auto range = d_bucket_to_node_map.equal_range(cell_id);

    // Loop over the range (surface nodes in the bucket)
    for (auto range_iter = range.first; range_iter != range.second; ++range_iter) {
      int node_id = (*range_iter).second;

      // Check if the point is on the surface
      if (pos == d_surf_pts[node_id]) {

        // Update the on-surface flags
        cur_node->onSurface(true);
        break;
      }
    }
  }
}
