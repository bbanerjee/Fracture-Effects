#include <CCA/Components/MPM/ConstitutiveModel/Models/YieldCond_Tabular.h>
#include <CCA/Components/MPM/ConstitutiveModel/Models/ModelState_Tabular.h>

#include <Core/Malloc/Allocator.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/ProblemSpec/ProblemSpecP.h>
#include <Core/Exceptions/ProblemSetupException.h>
#include <Core/Exceptions/InvalidValue.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace Vaango;
using Uintah::ProblemSpec;
using Uintah::ProblemSpecP;
using Uintah::ProblemSetupException;
using Uintah::InvalidValue;
using nlohmann::json;

class YieldCondTabularTest : public ::testing::Test {

protected:

  static void SetUpTestCase() {
    char currPath[2000];
    if (!getcwd(currPath, sizeof(currPath))) {
      std::cout << "Current path not found\n";
    }
    std::string json_file = std::string(currPath) + "/" + "table_yield.json";

    // Create a new document
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");

    // Create root node
    xmlNodePtr rootNode = xmlNewNode(nullptr, BAD_CAST "plastic_yield_condition");
    xmlNewProp(rootNode, BAD_CAST "type", BAD_CAST "tabular");
    xmlDocSetRootElement(doc, rootNode);

    // Create a child node
    xmlNewChild(rootNode, nullptr, BAD_CAST "filename", 
                BAD_CAST "table_yield.json");
    xmlNewChild(rootNode, nullptr, BAD_CAST "independent_variables", 
                BAD_CAST "Pressure");
    xmlNewChild(rootNode, nullptr, BAD_CAST "dependent_variables", 
                BAD_CAST "SqrtJ2");
    auto interp = xmlNewChild(rootNode, nullptr, BAD_CAST "interpolation",
                              BAD_CAST "");
    xmlNewProp(interp, BAD_CAST "type", BAD_CAST "linear");

    // Print the document to stdout
    xmlSaveFormatFileEnc("-", doc, "ISO-8859-1", 1);

    // Create a ProblemSpec
    ps = scinew ProblemSpec(xmlDocGetRootElement(doc), false);
    if (!ps) {
      std::cout << "**Error** Could not create ProblemSpec." << std::endl;
      std::cout << __FILE__ << ":" << __LINE__ << std::endl;
      exit(-1);
    }
  }

  static void TearDownTestCase() {}

  static ProblemSpecP ps;
};

ProblemSpecP YieldCondTabularTest::ps = nullptr;

TEST_F(YieldCondTabularTest, constructorTest)
{
  // Create a model
  YieldCond_Tabular model(ps);
  //std::cout << model;

  // Copy
  YieldCond_Tabular modelCopy(&model);
  //std::cout << modelCopy;
}

TEST_F(YieldCondTabularTest, evalYieldCondition)
{
  YieldCond_Tabular model(ps);
  ModelState_Tabular state;

  state.I1 = 300*3; // Tension
  state.sqrt_J2 = 1000;
  EXPECT_EQ(model.evalYieldCondition(&state), 1);

  state.I1 = 2*3;  // Tension
  state.sqrt_J2 = 39;
  EXPECT_EQ(model.evalYieldCondition(&state), -1);

  state.I1 = -1000*3;  // Compression
  state.sqrt_J2 = 625;
  EXPECT_EQ(model.evalYieldCondition(&state), -1);

  state.I1 = -1000*3;  // Compression
  state.sqrt_J2 = 605;
  EXPECT_EQ(model.evalYieldCondition(&state), -1);

  state.I1 = -1000*3;  // Compression
  state.sqrt_J2 = 635;
  EXPECT_EQ(model.evalYieldCondition(&state), 1);

  state.I1 = -7000*3;  // Compression
  state.sqrt_J2 = 1000;
  EXPECT_THROW(model.evalYieldCondition(&state), Uintah::InvalidValue);

  EXPECT_EQ(model.evalYieldConditionMax(&state), 900);
}

TEST_F(YieldCondTabularTest, eval_df_dsigma)
{
  YieldCond_Tabular model(ps);
  ModelState_Tabular state;
  Matrix3 zero(0.0);
  Matrix3 df_dsigma(0.0);

  model.eval_df_dsigma(zero, &state, df_dsigma);
  EXPECT_NEAR(df_dsigma(0,0), 1.666666, 1.0e-5);

  state.I1 = 300*3; // Tension
  state.sqrt_J2 = 1000;
  try {
  model.eval_df_dsigma(zero, &state, df_dsigma);
  } catch (Uintah::InvalidValue e)  {
    std::cout <<  e.message() << std::endl;
  }

  std::cout << "df_dsigma = " << df_dsigma << std::endl;
  //std::cout << "Has yielded = " << hasYielded << std::endl;
  /*
  try {
    YieldCond moduli = model.getCurrentYieldCond(&state);
    EXPECT_NEAR(moduli.bulkModulus, 9040, 1.0e-7);
    EXPECT_NEAR(moduli.shearModulus, 6780, 1.0e-7);
    //std::cout << "K,G = " << moduli.bulkModulus << "," 
    //            << moduli.shearModulus << std::endl;
  } catch (Uintah::InvalidValue e) {
    std::cout << e.message() << std::endl;
  }
  */
}
