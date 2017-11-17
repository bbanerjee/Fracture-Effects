#include <Core/Math/IntVec.h>
#include <Core/Math/Vec.h>
#include <InputOutput/InputParameter.h>
#include <InputOutput/zenxml/xml.h>
#include <InputOutput/IOUtils.h>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream>

using namespace dem;

bool
InputParameter::readInXML(const std::string& inputFileName)
{
  // Read the input file
  zen::XmlDoc doc;
  try {
    //std::cout << "Input file name= " << inputFileName << "\n";
    doc = zen::load(inputFileName);
  } catch (const zen::XmlFileError& err) {
    std::cerr << "*ERROR** Could not read input file " << inputFileName << "\n";
    std::cerr << "    Error # = " << err.lastError << "\n";
    return false;
  } catch (const zen::XmlParsingError& err) {
    std::cerr << "*ERROR** Could not read input file " << inputFileName << "\n";
    std::cerr << "    Parse Error in line: " << err.row + 1
              << " col: " << err.col << "\n";
    return false;
  }

  // Check whether this is the right type of input file
  if (doc.root().getNameAs<std::string>() != "Ellip3D_input") {
    std::cerr << "*ERROR** Could not find tag <Ellip3D_input> in input file "
              << inputFileName << "\n";
    return false;
  }

  // Load the document into input proxy for easier element access
  zen::XmlIn ps(doc);

  // Set up a lambda function to trim strings
  auto trim = [](std::string& str) {
    str.erase(0, str.find_first_not_of(" \n\r\t"));
    str.erase(str.find_last_not_of(" \n\r\t") + 1);
    return str;
  };

  // Read the title
  std::string title;
  if (!ps["Meta"]["title"](title)) {
    std::cerr << "*ERROR** Could not find simulation title in input file "
              << inputFileName << "\n";
    std::cerr << "  Add the <title> tag inside a <Meta> tag\n";
    return false;
  }
  //std::cout << "title = " << trim(title) << "\n";

  // Read the simulation type
  int simType = 0;
  if (!ps["SimulationType"](simType)) {
    std::cerr << "*ERROR** Could not find simulation type in input file "
              << inputFileName << "\n";
    std::cerr << "  Add the <SimulationType> tag.\n";
    return false;
  }
  //std::cout << "simulationType = " << simType << "\n";
  param["simuType"] = simType;

  // Read the parallel setup
  std::string mpiProcStr;
  if (!ps["Parallel"]["mpiProc"](mpiProcStr)) {
    std::cerr << "*ERROR** Could not find mpi proc info in input file "
              << inputFileName << "\n";
    std::cerr << "  Add the <mpiProc> tag inside the <Parallel> tag.\n";
    return false;
  }
  IntVec mpiProc = IntVec::fromString(mpiProcStr);
  //std::cout << "mpiProcX = " << mpiProc.x() << " mpiProcY = " << mpiProc.y()
  //          << " mpiProcZ = " << mpiProc.z() << "\n";
  param["mpiProcX"] = mpiProc.x();
  param["mpiProcY"] = mpiProc.y();
  param["mpiProcZ"] = mpiProc.z();

  int ompThreads = 1;
  if (!ps["Parallel"]["ompThreads"](ompThreads)) {
    std::cerr << "*ERROR** Could not find omp thread info in input file "
              << inputFileName << "\n";
    std::cerr << "  Add the <ompThreads> tag inside the <Parallel> tag.\n";
    return false;
  }
  //std::cout << "ompThreads = " << ompThreads << "\n";
  param["ompThreads"] = ompThreads;

  // Read time stepping info
  int startStep = 1;
  int endStep = 100;
  REAL timeAccrued = 0.0;
  REAL timeStep = 0.1;
  ps["Time"]["startStep"](startStep);
  ps["Time"]["endStep"](endStep);
  ps["Time"]["timeAccrued"](timeAccrued);
  ps["Time"]["timeStep"](timeStep);

  param["startStep"] = startStep;
  param["endStep"] = endStep;
  param["timeAccrued"] = timeAccrued;
  param["timeStep"] = timeStep;
  //std::cout << "startStep = " << startStep << "\n"
  //          << "endStep = " << endStep << "\n"
  //          << "timeAccrued = " << timeAccrued << "\n"
  //          << "timeStep = " << timeStep << "\n";

  // Read the output info
  int startSnapshot = 1;
  int endSnapshot = 100;
  std::string outputFolderName = "deposit";
  ps["Output"]["outputFolder"](outputFolderName);
  ps["Output"]["startSnapshot"](startSnapshot);
  ps["Output"]["endSnapshot"](endSnapshot);

  datafile["outputFolder"] = trim(outputFolderName);
  param["startSnap"] = startSnapshot;
  param["endSnap"] = endSnapshot;
  //std::cout << "startSnapshot = " << startSnapshot << "\n"
  //          << "endSnapshot = " << endSnapshot << "\n";

  // Read the physical constants
  REAL gravity = 9.8;
  REAL gravityScale = 1.0;
  ps["PhysicalConstants"]["gravityAcceleration"](gravity);
  ps["PhysicalConstants"]["gravityScaleFactor"](gravityScale);

  param["gravAccel"] = gravity;
  param["gravScale"] = gravityScale;
  //std::cout << "gravityAcc = " << gravity << "\n"
  //          << "gravityScale = " << gravityScale << "\n";

  // Read the boundary information
  std::string boundaryFilename;
  REAL boundaryFriction = 0.0;
  ps["Boundary"]["boundaryFilename"](boundaryFilename);
  ps["Boundary"]["boundaryFriction"](boundaryFriction);

  datafile["boundaryFilename"] = trim(boundaryFilename);
  param["boundaryFric"] = boundaryFriction;
  //std::cout << "boundaryFilename = " << trim(boundaryFilename) << "\n"
  //          << "boundaryFriction = " << boundaryFriction << "\n";

  // Read the DEM particle file information
  bool initializeFromFile = true;
  ps["DEM"]["initializeFromFile"](initializeFromFile);
  param["demToInitParticle"] = static_cast<int>(initializeFromFile);

  REAL massScaleFactor = 1.0;
  REAL momentScaleFactor = 1.0;
  REAL pileRate = 0.0;

  if (simType == 002 || simType == 101) {

    // Read the number of particle layers
    ps["DEM"]["particleLayers"](param["particleLayers"]);
    
    // Read the minZ and maxZ for initial particle generation
    ps["DEM"]["floatMinZ"](param["floatMinZ"]);
    ps["DEM"]["floatMaxZ"](param["floatMaxZ"]);

    // Read the trimming height after particle deposition
    ps["DEM"]["trimHeight"](param["trimHeight"]);

    // Read the gradation information
    auto sieve_ps = ps["DEM"]["Sieves"];
    if (!sieve_ps) {
      std::cerr << "**ERROR** For particles to be generated you will"
                << " have to provide gradation information in the input"
                << " file." << std::endl;
      exit(-1);
    }

    std::size_t numSieves;
    sieve_ps.attribute("number", numSieves);
    param["sieveNum"] = numSieves;

    std::string percentPassingStr;
    sieve_ps["percent_passing"](percentPassingStr);
    std::vector<REAL> percentPassing = 
      Ellip3D::Util::convertStrArray<REAL>(percentPassingStr);
    assert(percentPassing.size() == numSieves);

    std::string sizeStr;
    sieve_ps["size"](sizeStr);
    std::vector<REAL> size = Ellip3D::Util::convertStrArray<REAL>(sizeStr);
    assert(size.size() == numSieves);

    for (std::size_t i = 0; i < numSieves; ++i) {
      gradation.push_back(std::pair<REAL,REAL>(percentPassing[i], size[i]));
    }

    REAL ratio_ba, ratio_ca;
    sieve_ps["sieve_ratio"]["ratio_ba"](ratio_ba);
    sieve_ps["sieve_ratio"]["ratio_ca"](ratio_ca);
    param["ratioBA"] = ratio_ba;
    param["ratioCA"] = ratio_ba;

  } else {

    std::string particleFilename;
    ps["DEM"]["particleFilename"](particleFilename);
    datafile["particleFilename"] = trim(particleFilename);

    // Read the trimming height after particle deposition
    // if needed
    if (simType == 102) {
      ps["DEM"]["trimHeight"](param["trimHeight"]);
    }

  }

  // Read other DEM information
  ps["DEM"]["massScaleFactor"](massScaleFactor);
  ps["DEM"]["momentScaleFactor"](momentScaleFactor);
  ps["DEM"]["pileRate"](pileRate);

  param["massScale"] = massScaleFactor;
  param["mntScale"] = momentScaleFactor;
  param["pileRate"] = pileRate;

  //std::cout << "particleFilename = " << trim(particleFilename) << "\n"
  //          << "massScaleFactor = " << massScaleFactor << "\n"
  //          << "momentScaleFactor = " << momentScaleFactor << "\n";

  // Read the DEM material information
  REAL youngModulus = 1.0e10;
  REAL poissonRatio = 0.3;
  REAL specificGravity = 1.0;
  REAL membraneYoungModulus = 1.0e6;
  REAL forceDamping = 0.0;
  REAL momentDamping = 0.0;
  ps["DEM"]["Material"]["youngModulus"](youngModulus);
  ps["DEM"]["Material"]["poissonRatio"](poissonRatio);
  ps["DEM"]["Material"]["specificGravity"](specificGravity);
  ps["DEM"]["Material"]["membraneYoungModulus"](membraneYoungModulus);
  ps["DEM"]["Material"]["forceDamping"](forceDamping);
  ps["DEM"]["Material"]["momentDamping"](momentDamping);

  param["young"] = youngModulus;
  param["poisson"] = poissonRatio;
  param["specificG"] = specificGravity;
  param["memYoung"] = membraneYoungModulus;
  param["forceDamp"] = forceDamping;
  param["momentDamp"] = momentDamping;

  //std::cout << "youngModulus = " << youngModulus << "\n"
  //          << " poissonRatio = " << poissonRatio << "\n"
  //          << " specificGravity = " << specificGravity << "\n"
  //          << " membraneYoungModulus = " << membraneYoungModulus << "\n"
  //          << " forceDamping = " << forceDamping << "\n"
  //          << " momentDamping = " << momentDamping << "\n";

  // Read the DEM contact information
  ps["DEM"]["Contact"]["contactDamping"](param["contactDamp"]);
  ps["DEM"]["Contact"]["contactFriction"](param["contactFric"]);
  ps["DEM"]["Contact"]["contactCohesion"](param["contactCohesion"]);
  ps["DEM"]["Contact"]["minRelativeOverlap"](param["minRelaOverlap"]);
  ps["DEM"]["Contact"]["maxRelativeOverlap"](param["maxRelaOverlap"]);
  ps["DEM"]["Contact"]["measurableOverlap"](param["measureOverlap"]);

  //std::cout << "contactDamping = " << param["contactDamp"] << "\n"
  //          << "contactFriction = " << param["contactFric"] << "\n"
  //          << "contactCohesion = " << param["contactCohesion"] << "\n"
  //          << "minRelativeOverlap = " << param["minRelaOverlap"] << "\n"
  //          << "maxRelativeOverlap = " << param["maxRelaOverlap"] << "\n"
  //          << "measurableOverlap = " << param["measureOverlap"] << "\n";

  // Read the periodic particle generation controls
  auto periodicGen_ps = ps["DEM"]["PeriodicParticleGeneration"];
  if (periodicGen_ps) {
    periodicGen_ps["boundaryMarginFactor"](param["periodicBoundaryMarginFactor"]);
    periodicGen_ps["boundaryFaceShiftFactor"](param["periodicBoundaryFaceShiftFactor"]);
    std::string filename;
    periodicGen_ps["boundaryFilename"](filename);
    datafile["periodicBoundaryOutputFilename"] = trim(filename);
    periodicGen_ps["particleFilename"](filename);
    datafile["periodicParticleOutputFilename"] = trim(filename);
  } else {
    param["periodicBoundaryMarginFactor"] = 2.0;
    param["periodicBoundaryFaceShiftFactor"] = 0.0;
    datafile["periodicBoundaryOutputFilename"] = "generated_periodic_boundary";
    datafile["periodicParticleOutputFilename"] = "generated_periodic_particles";
  }

  // DEM BCs (read from input file)
  auto dem_ps = ps["DEM"];
  auto dem_bc_ps = dem_ps["BoundaryConditions"];
  std::string demBCFile = "none";
  if (dem_bc_ps) {
    dem_bc_ps["inputFile"](demBCFile);
  }
  datafile["DEMBCInputFile"] = demBCFile;

  // Check if a peridynamics section exists
  auto peri_ps = ps["Peridynamics"];
  if (peri_ps) {

    std::string periFilename;
    peri_ps["periFilename"](periFilename);
    datafile["periFilename"] = trim(periFilename);
    //std::cout << "periFilename = " << trim(periFilename) << "\n";

    REAL fac = 1.0;
    if (!peri_ps["periGeomScaleFactor"](fac)) {
      std::cerr
        << "*WARNING** Could not find peridynamic geometry"
        << " scale factor in input file "
        << inputFileName << "\n";
      std::cerr << " Proceeding with default value."
                << " Add the <periGeomScaleFactor> tag"
                << " inside the <Peridynamics> tag.\n";
    } 
    param["periGeomScaleFac"] = fac;

    std::string vecStr;
    if (!peri_ps["periGeomTranslationVector"](vecStr)) {
      std::cerr
        << "*WARNING** Could not find peridynamic geometry"
        << " translation vector in input file "
        << inputFileName << "\n";
      std::cerr << " Proceeding with default value."
                << " Add the <periGeomTranslationVector> tag"
                << " inside the <Peridynamics> tag.\n";
      param["periTransVecX"] = 0.0;
      param["periTransVecY"] = 0.0;
      param["periTransVecZ"] = 0.0;
    } else {
      Vec vec = Vec::fromString(vecStr);
      param["periTransVecX"] = vec.x();
      param["periTransVecY"] = vec.y();
      param["periTransVecZ"] = vec.z();
    }

    if (!peri_ps["periGeomReflectionVector"](vecStr)) {
      std::cerr
        << "*WARNING** Could not find peridynamic geometry"
        << " reflection vector in input file "
        << inputFileName << "\n";
      std::cerr << " Proceeding with default value."
                << " Add the <periGeomReflectionVector> tag"
                << " inside the <Peridynamics> tag.\n";
      param["periReflVecX"] = 1.0;
      param["periReflVecY"] = 1.0;
      param["periReflVecZ"] = 1.0;
    } else {
      Vec vec = Vec::fromString(vecStr);
      param["periReflVecX"] = vec.x();
      param["periReflVecY"] = vec.y();
      param["periReflVecZ"] = vec.z();
    }

    bool initializeFromFile = true;
    peri_ps["initializeFromFile"](initializeFromFile);
    //std::cout << "initializeFromFile = " << initializeFromFile << "\n";
    param["periToInitParticle"] = static_cast<int>(initializeFromFile);

    if (!peri_ps["minPeriDomain"](vecStr)) {
      std::cerr
        << "*ERROR** Could not find min peridynamic domain info in input file "
        << inputFileName << "\n";
      std::cerr
        << "  Add the <minPeriDomain> tag inside the <Peridynamics> tag.\n";
      return false;
    }
    Vec minPeriDomain = Vec::fromString(vecStr);
    //std::cout << "minPeriX = " << minPeriDomain.x()
    //          << " minPeriY = " << minPeriDomain.y()
    //          << " minPeriZ = " << minPeriDomain.z() << "\n";
    param["Xmin"] = minPeriDomain.x();
    param["Ymin"] = minPeriDomain.y();
    param["Zmin"] = minPeriDomain.z();

    if (!peri_ps["maxPeriDomain"](vecStr)) {
      std::cerr
        << "*ERROR** Could not find max peridynamic domain info in input file "
        << inputFileName << "\n";
      std::cerr
        << "  Add the <maxPeriDomain> tag inside the <Peridynamics> tag.\n";
      return false;
    }
    Vec maxPeriDomain = Vec::fromString(vecStr);
    //std::cout << "maxPeriX = " << maxPeriDomain.x()
    //          << " maxPeriY = " << maxPeriDomain.y()
    //          << " maxPeriZ = " << maxPeriDomain.z() << "\n";
    param["Xmax"] = maxPeriDomain.x();
    param["Ymax"] = maxPeriDomain.y();
    param["Zmax"] = maxPeriDomain.z();

    bool removePeriParticles = true;
    bool removeDEMParticles = false;
    if (!peri_ps["removePeriParticlesInsideDEM"](removePeriParticles) ||
        !peri_ps["removeDEMParticlesInsidePeri"](removeDEMParticles)) {
      std::cerr
        << "*WARNING** Could not find flags to remove peri/dem particles"
        << " that overlap (in input file "
        << inputFileName << ")\n";
      std::cerr << " Proceeding with default value."
                << " Add the <removePeriParticlesInsideDEM> = 1 (true) and"
                << "  <removeDEMParticlesInsidePeri> = 0 (false) tags "
                << " inside the <Peridynamics> tag.\n";
    } 
    param["removePeriParticles"] = static_cast<REAL>(removePeriParticles);
    param["removeDEMParticles"] = static_cast<REAL>(!removePeriParticles);

    /*
    std::cout << "** Remove: " << std::boolalpha 
              << removePeriParticles
              << ":" << static_cast<REAL>(removePeriParticles) << " and "
              << removeDEMParticles
              << ":" << static_cast<REAL>(removeDEMParticles) << std::endl;
    std::cout << "removePeriParticles = " << param["removePeriParticles"] << "\n";
    */

    // Peridynamics material properties
    auto peri_mat_ps = peri_ps["Material"];
    if (!peri_mat_ps) {
      std::cerr
        << "*ERROR** No peridynamics material properties found in input file "
        << inputFileName << "\n";
      std::cerr << "  Add the <Material> tag inside the <Peridynamics> tag.\n";
      return false;
    }

    peri_mat_ps["typeConstitutive"](param["typeConstitutive"]);

    peri_mat_ps["periDensity"](param["periDensity"]);
    peri_mat_ps["bodyDensity"](param["bodyDensity"]);
    peri_mat_ps["hchi"](param["hchi"]);
    peri_mat_ps["chi"](param["chi"]);
    peri_mat_ps["c"](param["c"]);
    peri_mat_ps["phi"](param["phi"]);
    peri_mat_ps["psi"](param["psi"]);
    peri_mat_ps["kappa"](param["kappa"]);
    peri_mat_ps["rEllip"](param["rEllip"]);
    peri_mat_ps["beta"](param["beta"]);
    peri_mat_ps["bondStretchLimit"](param["bondStretchLimit"]);

    /*
    std::cout << "periDensity " << param["periDensity"] << "\n"
              << " bodyDensity " << param["bodyDensity"] << "\n"
              << " hchi " << param["hchi"] << "\n"
              << " chi " << param["chi"] << "\n"
              << " c " << param["c"] << "\n"
              << " phi " << param["phi"] << "\n"
              << " psi " << param["psi"] << "\n"
              << " kappa " << param["kappa"] << "\n"
              << " rEllip " << param["rEllip"] << "\n"
              << " beta " << param["beta"] << "\n"
              << " bondStretchLimit " << param["bondStretchLimit"] << "\n";
    */

    // Peridynamics constitutive model
    auto peri_cm_ps = peri_mat_ps["constitutive_model"];
    if (!peri_cm_ps) {
      std::cerr << "*ERROR** No peridynamics material constitutive model found "
                   "in input file "
                << inputFileName << "\n";
      std::cerr
        << "  Add the <constitutive_model> tag inside the <Material> tag "
        << " inside the <Peridynamics> tag.\n";
      return false;
    }

    // Get the material model type
    std::string model_type;
    if (!peri_cm_ps.attribute("type", model_type)) {
      std::cerr
        << "**ERROR** Peridynamics constitutive model type not provided."
        << " Specify <constitutive_model type=\"xxxx\" >"
        << "\n";
      return false;
    }

    if (model_type == "linear_elastic") {
      peri_cm_ps["poissonRatio"](param["periPoisson"]);
      peri_cm_ps["youngModulus"](param["periYoung"]);
    } else {
      std::cerr << "**ERROR** Only linear_elastic models are allowed\n";
      return false;
    }
    //std::cout << "periPoisson = " << param["periPoisson"] << "\n"
    //          << "periYoung = " << param["periYoung"] << "\n";

    param["lambda"] =
      param["periPoisson"] * param["periYoung"] /
      ((1.0 + param["periPoisson"]) * (1.0 - 2.0 * param["periPoisson"]));
    param["mu"] = param["periYoung"] / (2.0 * (1.0 + param["periPoisson"]));
    param["kBulk"] =
      param["periYoung"] / (3.0 * (1.0 - 2.0 * param["periPoisson"]));
    param["tangentModulus11"] = param["lambda"] + 2.0 * param["mu"];
    param["tangentModulus12"] = param["lambda"];
    param["tangentModulus13"] = param["lambda"];
    param["tangentModulus21"] = param["lambda"];
    param["tangentModulus22"] = param["lambda"] + 2.0 * param["mu"];
    param["tangentModulus23"] = param["lambda"];
    param["tangentModulus31"] = param["lambda"];
    param["tangentModulus32"] = param["lambda"];
    param["tangentModulus33"] = param["lambda"] + 2.0 * param["mu"];
    param["tangentModulus44"] = param["mu"];
    param["tangentModulus55"] = param["mu"];
    param["tangentModulus66"] = param["mu"];
    param["Aphi"] = 2 * sqrt(6.0) * cos(param["phi"]) /
                    (3.0 + param["beta"] * sin(param["phi"]));
    param["Bphi"] = 2 * sqrt(6.0) * sin(param["phi"]) /
                    (3.0 + param["beta"] * sin(param["phi"]));
    param["Apsi"] = 2 * sqrt(6.0) * cos(param["psi"]) /
                    (3.0 + param["beta"] * sin(param["psi"]));
    param["Bpsi"] = 2 * sqrt(6.0) * sin(param["psi"]) /
                    (3.0 + param["beta"] * sin(param["psi"]));

    // Peridynamics BCs
    auto peri_bc_ps = peri_ps["BoundaryConditions"];
    if (!peri_bc_ps) {
      std::cerr
        << "*ERROR** No peridynamics displacement/load BC found in input file "
        << inputFileName << "\n";
      std::cerr << "  Add the <BoundaryConditions> tag inside the <Peridynamics> tag.\n";
      return false;
    }
    peri_bc_ps["fixRadius"](param["fixRadius"]);
    peri_bc_ps["periFixCentroidX"](param["periFixCentroidX"]);
    peri_bc_ps["periFixCentroidY"](param["periFixCentroidY"]);
    peri_bc_ps["periFixCentroidZ"](param["periFixCentroidZ"]);
    peri_bc_ps["periForce"](param["periForce"]);
    peri_bc_ps["rampStep"](param["rampStep"]);
  }

  return true;
}

void
InputParameter::writeOutXML()
{
}

void
InputParameter::readIn(const char* input)
{
  if (readInXML(input))
    return;

  std::cerr << "**WARNING** Failed to read XML input file " << input << "\n";
  std::cerr << "            Trying to read the file as ordinary text\n";

  std::ifstream ifs;
  ifs.open(input);
  if (!ifs) {
    std::cerr << "stream error: InputParameter.cpp" << std::endl;
    exit(-1);
  }
  std::string line;
  std::istringstream ssline;
  std::string str, str2;
  REAL val;

  // 28 generic parameters
  for (std::size_t i = 0; i < 28; ++i) {
    while (getline(ifs, line))
      if (line[0] != '#' && line.compare("") != 0)
        break;
    ssline.clear();
    ssline.str(line);
    ssline >> str >> val;
    param[str] = val;
  }

  // for different types of simulation
  std::size_t simuType = static_cast<std::size_t>(param["simuType"]);
  switch (simuType) {
    case 001: // proceed from preset state
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 1; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 002: // tuneMassPercentage
      for (std::size_t i = 0; i < 12; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      for (std::size_t i = 0; i < static_cast<std::size_t>(param["sieveNum"]);
           ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        REAL percent, size;
        ssline >> percent >> size;
        gradation.push_back(std::make_pair(percent, size));
      }
      break;

    case 003: // trimOnly
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 1; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 101: // depositIntoContainer
      for (std::size_t i = 0; i < 12; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      for (std::size_t i = 0; i < static_cast<std::size_t>(param["sieveNum"]);
           ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        REAL percent, size;
        ssline >> percent >> size;
        gradation.push_back(std::make_pair(percent, size));
      }
      break;

    case 102: // resumeDepositIntoContainer
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 201: // isotropic 1
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 6; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 202: // isotropic 2
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 7; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 203: // isotropic 3
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 3; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      for (std::size_t i = 0;
           i < static_cast<std::size_t>(param["sigmaPoints"]); ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        REAL sigma;
        ssline >> sigma;
        sigmaPath.push_back(sigma);
      }
      for (std::size_t i = 0; i < 3; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 301: // odometer 1
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 7; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 302: // odometer 2
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 3; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      for (std::size_t i = 0;
           i < static_cast<std::size_t>(param["sigmaPoints"]); ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        REAL sigma;
        ssline >> sigma;
        sigmaPath.push_back(sigma);
      }
      for (std::size_t i = 0; i < 3; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 401: // triaxial 1
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 5; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 402: // triaxial 2
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 6; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 411: // plane strain 1
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 6; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 412: // plain strain 2
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 7; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 501: // true triaxial 1
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 9; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 502: // true triaxial 2
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 10; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 601: // expandCavityParticle
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 7; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 602: // resumeExpandCavityParticle
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 1; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 701: // couple with sonic fluid flow
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 19; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      break;

    case 3001: // couple with peridynamics clay
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 26; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      param["lambda"] =
        param["periPoisson"] * param["periYoung"] /
        ((1.0 + param["periPoisson"]) * (1.0 - 2.0 * param["periPoisson"]));
      param["mu"] = param["periYoung"] / (2.0 * (1.0 + param["periPoisson"]));
      param["kBulk"] =
        param["periYoung"] / (3.0 * (1.0 - 2.0 * param["periPoisson"]));
      param["tangentModulus11"] = param["lambda"] + 2.0 * param["mu"];
      param["tangentModulus12"] = param["lambda"];
      param["tangentModulus13"] = param["lambda"];
      param["tangentModulus21"] = param["lambda"];
      param["tangentModulus22"] = param["lambda"] + 2.0 * param["mu"];
      param["tangentModulus23"] = param["lambda"];
      param["tangentModulus31"] = param["lambda"];
      param["tangentModulus32"] = param["lambda"];
      param["tangentModulus33"] = param["lambda"] + 2.0 * param["mu"];
      param["tangentModulus44"] = param["mu"];
      param["tangentModulus55"] = param["mu"];
      param["tangentModulus66"] = param["mu"];
      param["Aphi"] = 2 * sqrt(6.0) * cos(param["phi"]) /
                      (3.0 + param["beta"] * sin(param["phi"]));
      param["Bphi"] = 2 * sqrt(6.0) * sin(param["phi"]) /
                      (3.0 + param["beta"] * sin(param["phi"]));
      param["Apsi"] = 2 * sqrt(6.0) * cos(param["psi"]) /
                      (3.0 + param["beta"] * sin(param["psi"]));
      param["Bpsi"] = 2 * sqrt(6.0) * sin(param["psi"]) /
                      (3.0 + param["beta"] * sin(param["psi"]));

      break;

    case 3002: // couple with peridynamics clay, pull out DEM particles in
               // peri-domain
      for (std::size_t i = 0; i < 2; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> str2;
        datafile[str] = str2;
      }
      for (std::size_t i = 0; i < 21; ++i) {
        while (getline(ifs, line))
          if (line[0] != '#' && line.compare("") != 0)
            break;
        ssline.clear();
        ssline.str(line);
        ssline >> str >> val;
        param[str] = val;
      }
      param["lambda"] =
        param["periPoisson"] * param["periYoung"] /
        ((1.0 + param["periPoisson"]) * (1.0 - 2.0 * param["periPoisson"]));
      param["mu"] = param["periYoung"] / (2.0 * (1.0 + param["periPoisson"]));
      param["kBulk"] =
        param["periYoung"] / (3.0 * (1.0 - 2.0 * param["periPoisson"]));
      param["tangentModulus11"] = param["lambda"] + 2.0 * param["mu"];
      param["tangentModulus12"] = param["lambda"];
      param["tangentModulus13"] = param["lambda"];
      param["tangentModulus21"] = param["lambda"];
      param["tangentModulus22"] = param["lambda"] + 2.0 * param["mu"];
      param["tangentModulus23"] = param["lambda"];
      param["tangentModulus31"] = param["lambda"];
      param["tangentModulus32"] = param["lambda"];
      param["tangentModulus33"] = param["lambda"] + 2.0 * param["mu"];
      param["tangentModulus44"] = param["mu"];
      param["tangentModulus55"] = param["mu"];
      param["tangentModulus66"] = param["mu"];
      param["Aphi"] = 2 * sqrt(6.0) * cos(param["phi"]) /
                      (3.0 + param["beta"] * sin(param["phi"]));
      param["Bphi"] = 2 * sqrt(6.0) * sin(param["phi"]) /
                      (3.0 + param["beta"] * sin(param["phi"]));
      param["Apsi"] = 2 * sqrt(6.0) * cos(param["psi"]) /
                      (3.0 + param["beta"] * sin(param["psi"]));
      param["Bpsi"] = 2 * sqrt(6.0) * sin(param["psi"]) /
                      (3.0 + param["beta"] * sin(param["psi"]));

      break;
  }

  ifs.close();
}

void
InputParameter::writeOut()
{
  std::map<std::string, REAL>& param = InputParameter::get().param;
  std::vector<std::pair<REAL, REAL>>& grada = InputParameter::get().gradation;
  std::map<std::string, std::string>& file = InputParameter::get().datafile;
  std::vector<REAL>& sigma = InputParameter::get().sigmaPath;

  for (std::map<std::string, REAL>::const_iterator it = param.begin();
       it != param.end(); ++it)
    std::cout << it->first << "  " << it->second << std::endl;

  for (auto& i : grada)
    std::cout << i.first << "  " << i.second << std::endl;

  for (std::map<std::string, std::string>::const_iterator it = file.begin();
       it != file.end(); ++it)
    std::cout << it->first << "  " << it->second << std::endl;

  for (std::vector<REAL>::const_iterator it = sigma.begin(); it != sigma.end();
       ++it)
    std::cout << (*it) << std::endl;
}
