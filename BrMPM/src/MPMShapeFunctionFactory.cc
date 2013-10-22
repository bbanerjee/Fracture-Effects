#include <MPMShapeFunctionFactory.h>
#include <Exception.h>

#include <ShapeFunctions/MPMShapeFunction.h>
#include <ShapeFunctions/LinearShapeFunction.h>

#include <Core/ProblemSpec/ProblemSpec.h>

#include <iostream>
#include <string>

using namespace BrMPM;

MPMShapeFunctionFactory::MPMShapeFunctionFactory()
{
}

MPMShapeFunctionFactory::~MPMShapeFunctionFactory()
{
}

MPMShapeFunctionP
MPMShapeFunctionFactory::create(const Uintah::ProblemSpecP& ps)
{
  Uintah::ProblemSpecP shape_ps = ps->findBlock("ShapeFunction");
  if (!shape_ps) {
    throw Exception("**ERROR** <ShapeFunction> tag not found", __FILE__, __LINE__);
  }
 
  std::string shape_type;
  if (!shape_ps->getAttribute("type", shape_type)) {
    throw Exception("**ERROR** <ShapeFunction type=?> Type attribute not found",
                    __FILE__, __LINE__);
  }

  // Create shape function
  if (shape_type == "GIMP") {
    //return std::make_shared<GIMPShapeFunction>();
    return std::make_shared<LinearShapeFunction>();
  } else if (shape_type == "Quad") {
    //return std::make_shared<QuadShapeFunction>();
    return std::make_shared<LinearShapeFunction>();
  } else if (shape_type == "Linear") {
    return std::make_shared<LinearShapeFunction>();
  } else if (shape_type == "Cubic") {
    //return std::make_shared<CubicShapeFunction>();
    return std::make_shared<LinearShapeFunction>();
  } else {
    std::ostringstream out;
    out << "**ERROR** UNknown shape function" << shape_type << " for MPM. " << std::endl;
    throw Exception(out.str(), __FILE__, __LINE__);
  }
}


      















  
