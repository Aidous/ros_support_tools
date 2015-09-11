#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <sstream>
#include <string.h>
#include <assert.h>
#include <convex_decomposition/vhacd/VHACD.h>

using namespace VHACD;
using namespace std;

struct Material
{

  float m_diffuseColor[3];
  float m_ambientIntensity;
  float m_specularColor[3];
  float m_emissiveColor[3];
  float m_shininess;
  float m_transparency;

  Material(void)
  {
      m_diffuseColor[0]  = 0.5f;
      m_diffuseColor[1]  = 0.5f;
      m_diffuseColor[2]  = 0.5f;
      m_specularColor[0] = 0.5f;
      m_specularColor[1] = 0.5f;
      m_specularColor[2] = 0.5f;
      m_ambientIntensity = 0.4f;
      m_emissiveColor[0] = 0.0f;
      m_emissiveColor[1] = 0.0f;
      m_emissiveColor[2] = 0.0f;
      m_shininess        = 0.4f;
      m_transparency     = 0.5f;
  };
};

class ProgressCallback : public IVHACD::IUserCallback
{
public:
  ProgressCallback(void) {}
  ~ProgressCallback() {};

  void Update(const double overallProgress,
              const double stageProgress,
              const double operationProgress,
              const char * const stage,
              const char * const operation)
  {
      cout << setfill(' ') << setw(3) << (int)(overallProgress  +0.5) << "% "
           << "[ " << stage     << " " << setfill(' ') << setw(3) << (int)(stageProgress    +0.5) << "% ] "
                   << operation << " " << setfill(' ') << setw(3) << (int)(operationProgress+0.5) << "%" << endl;
  };
};

class Logger : public IVHACD::IUserLogger
{
  public:
    Logger(void){}
    Logger(const string & fileName){ OpenFile(fileName);  }
    ~Logger() {};

    void Log(const char * const msg)
    {
      if (m_file.is_open())
      {
        m_file << msg;
        m_file.flush();
      }
    }
    void OpenFile(const string & fileName)
    {
      m_file.open(fileName.c_str());
    }

  private:
    ofstream m_file;
};

struct Parameters
{
    unsigned int m_oclPlatformID;
    unsigned int m_oclDeviceID;
    string m_fileNameIn;
    string m_fileNameOut;
    string m_fileNameLog;
    bool m_run;
    IVHACD::Parameters m_paramsVHACD;

    Parameters(void)
    {
      m_run           = true;
      m_oclPlatformID = 0;
      m_oclDeviceID   = 0;
      m_fileNameIn    = "";
      m_fileNameOut   = "output.wrl";
      m_fileNameLog   = "log.txt";
    }
};

class MeshConvexApproximation
{
public:
  MeshConvexApproximation()
  {

  }

  virtual ~MeshConvexApproximation()
  {

  }

  bool run(int argc,char** argv)
  {
    parseParameters(argc,argv,params_);
    logger_.OpenFile(params_.m_fileNameLog);
    params_.m_paramsVHACD.m_callback = &progress_callback_;
    params_.m_paramsVHACD.m_logger = &logger_;

    if(params_.m_fileNameIn.empty() || !params_.m_run)
    {
      if(params_.m_fileNameIn.empty())
      {
        std::cout<<"Empty input file"<<std::endl;
      }
      else
      {
        std::cout<<"Input file "<<params_.m_fileNameIn << std::endl;
      }

      usage(params_);
      return false;
    }

    printParameters(params_);

    // load mesh
    vector<float > points;
    vector<int > triangles;
    string fileExtension;
    getFileExtension(params_.m_fileNameIn, fileExtension);
    if (fileExtension == ".OFF")
    {
        if (!loadOFF(params_.m_fileNameIn, points, triangles, logger_))
        {
            return false;
        }
    }
    else if (fileExtension == ".OBJ")
    {
        if (!loadOBJ(params_.m_fileNameIn, points, triangles, logger_))
        {
            return false;
        }
    }
    else
    {
        std::cout<<"Format for input file "<<fileExtension <<" not supported!\n";
        return false;
    }

    // run V-HACD
    IVHACD * interfaceVHACD = CreateVHACD();
    bool res = interfaceVHACD->Compute(&points[0]   , 3, (unsigned int)points.size()    / 3,
                                       &triangles[0], 3, (unsigned int)triangles.size() / 3, params_.m_paramsVHACD);
    if (res)
    {
      // save output
      std::stringstream msg;
      unsigned int nConvexHulls = interfaceVHACD->GetNConvexHulls();
      msg.str("");
      msg << "+ Generate output: " << nConvexHulls << " convex-hulls " << endl;
      logger_.Log(msg.str().c_str());
      ofstream foutCH(params_.m_fileNameOut.c_str());
      IVHACD::ConvexHull ch;
      if (foutCH.is_open())
      {
          Material mat;
          for (unsigned int p = 0; p < nConvexHulls; ++p)
          {
              interfaceVHACD->GetConvexHull(p, ch);
              computeRandomColor(mat);
              saveVRML2(foutCH, ch.m_points, ch.m_triangles, ch.m_nPoints, ch.m_nTriangles, mat, logger_);
              msg.str("");
              msg << "\t CH[" << setfill('0') << setw(5) << p << "] " << ch.m_nPoints << " V, " << ch.m_nTriangles << " T" << endl;
              logger_.Log(msg.str().c_str());
          }
          foutCH.close();
      }
    }
    else
    {
      logger_.Log("Decomposition cancelled by user!\n");
    }


    interfaceVHACD->Clean();
    interfaceVHACD->Release();

    return true;
  }


protected:

  void printParameters(const Parameters& params)
  {
    std::stringstream msg;
    msg << "+ Parameters" << std::endl;
    msg << "\t input                                       " << params.m_fileNameIn                            << endl;
    msg << "\t resolution                                  " << params.m_paramsVHACD.m_resolution              << endl;
    msg << "\t max. depth                                  " << params.m_paramsVHACD.m_depth                   << endl;
    msg << "\t max. concavity                              " << params.m_paramsVHACD.m_concavity               << endl;
    msg << "\t plane down-sampling                         " << params.m_paramsVHACD.m_planeDownsampling       << endl;
    msg << "\t convex-hull down-sampling                   " << params.m_paramsVHACD.m_convexhullDownsampling  << endl;
    msg << "\t alpha                                       " << params.m_paramsVHACD.m_alpha                   << endl;
    msg << "\t beta                                        " << params.m_paramsVHACD.m_beta                    << endl;
    msg << "\t gamma                                       " << params.m_paramsVHACD.m_gamma                   << endl;
    msg << "\t delta                                       " << params.m_paramsVHACD.m_delta                   << endl;
    msg << "\t pca                                         " << params.m_paramsVHACD.m_pca                     << endl;
    msg << "\t mode                                        " << params.m_paramsVHACD.m_mode                    << endl;
    msg << "\t max. vertices per convex-hull               " << params.m_paramsVHACD.m_maxNumVerticesPerCH     << endl;
    msg << "\t min. volume to add vertices to convex-hulls " << params.m_paramsVHACD.m_minVolumePerCH          << endl;
    msg << "\t convex-hull approximation                   " << params.m_paramsVHACD.m_convexhullApproximation << endl;
    msg << "\t OpenCL acceleration                         " << params.m_paramsVHACD.m_oclAcceleration         << endl;
    msg << "\t OpenCL platform ID                          " << params.m_oclPlatformID                         << endl;
    msg << "\t OpenCL acceleration                         " << params.m_oclDeviceID                           << endl;
    msg << "\t output                                      " << params.m_fileNameOut                           << endl;
    msg << "\t log                                         " << params.m_fileNameLog                           << endl;
    msg << "+ Load mesh" << std::endl;

    std::cout << msg.str();
  }


  void parseParameters(int argc,char** argv,Parameters & params)
  {
    for (int i = 1; i < argc; ++i)
    {
      if      (!strcmp(argv[i], "--input"                  ))
      {
        if (++i < argc) params.m_fileNameIn                            = argv[i];
      }

      else if (!strcmp(argv[i], "--output"                 )) { if (++i < argc) params.m_fileNameOut                           = argv[i];       }
      else if (!strcmp(argv[i], "--log"                    )) { if (++i < argc) params.m_fileNameLog                           = argv[i];       }
      else if (!strcmp(argv[i], "--resolution"             )) { if (++i < argc) params.m_paramsVHACD.m_resolution              = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--depth"                  )) { if (++i < argc) params.m_paramsVHACD.m_depth                   = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--concavity"              )) { if (++i < argc) params.m_paramsVHACD.m_concavity               = atof(argv[i]); }
      else if (!strcmp(argv[i], "--planeDownsampling"      )) { if (++i < argc) params.m_paramsVHACD.m_planeDownsampling       = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--convexhullDownsampling" )) { if (++i < argc) params.m_paramsVHACD.m_convexhullDownsampling  = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--alpha"                  )) { if (++i < argc) params.m_paramsVHACD.m_alpha                   = atof(argv[i]); }
      else if (!strcmp(argv[i], "--beta"                   )) { if (++i < argc) params.m_paramsVHACD.m_beta                    = atof(argv[i]); }
      else if (!strcmp(argv[i], "--gamma"                  )) { if (++i < argc) params.m_paramsVHACD.m_gamma                   = atof(argv[i]); }
      else if (!strcmp(argv[i], "--delta"                  )) { if (++i < argc) params.m_paramsVHACD.m_delta                   = atof(argv[i]); }
      else if (!strcmp(argv[i], "--pca"                    )) { if (++i < argc) params.m_paramsVHACD.m_pca                     = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--mode"                   )) { if (++i < argc) params.m_paramsVHACD.m_mode                    = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--maxNumVerticesPerCH"    )) { if (++i < argc) params.m_paramsVHACD.m_maxNumVerticesPerCH     = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--minVolumePerCH"         )) { if (++i < argc) params.m_paramsVHACD.m_minVolumePerCH          = atof(argv[i]); }
      else if (!strcmp(argv[i], "--convexhullApproximation")) { if (++i < argc) params.m_paramsVHACD.m_convexhullApproximation = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--oclAcceleration"        )) { if (++i < argc) params.m_paramsVHACD.m_oclAcceleration         = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--oclPlatformID"          )) { if (++i < argc) params.m_oclPlatformID                         = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--oclDeviceID"            )) { if (++i < argc) params.m_oclDeviceID                           = atoi(argv[i]); }
      else if (!strcmp(argv[i], "--help"                   )) { params.m_run = false;                                                           }
    }

    params.m_paramsVHACD.m_resolution             = (params.m_paramsVHACD.m_resolution             < 64) ? 0 : params.m_paramsVHACD.m_resolution;
    params.m_paramsVHACD.m_planeDownsampling      = (params.m_paramsVHACD.m_planeDownsampling      <  1) ? 1 : params.m_paramsVHACD.m_planeDownsampling;
    params.m_paramsVHACD.m_convexhullDownsampling = (params.m_paramsVHACD.m_convexhullDownsampling <  1) ? 1 : params.m_paramsVHACD.m_convexhullDownsampling;
  }

  void getFileExtension(const string & fileName, string & fileExtension)
  {
      size_t lastDotPosition = fileName.find_last_of(".");
      if (lastDotPosition == string::npos)
      {
          fileExtension = "";
      }
      else
      {
          fileExtension = fileName.substr(lastDotPosition, fileName.size());
          transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), ::toupper);
      }
  }

  void computeRandomColor(Material & mat)
  {
      mat.m_diffuseColor[0] = mat.m_diffuseColor[1] = mat.m_diffuseColor[2] = 0.0f;
      while (mat.m_diffuseColor[0] == mat.m_diffuseColor[1] ||
          mat.m_diffuseColor[2] == mat.m_diffuseColor[1] ||
          mat.m_diffuseColor[2] == mat.m_diffuseColor[0])
      {
          mat.m_diffuseColor[0] = (rand() % 100) / 100.0f;
          mat.m_diffuseColor[1] = (rand() % 100) / 100.0f;
          mat.m_diffuseColor[2] = (rand() % 100) / 100.0f;
      }
  }

  void usage(const Parameters & params)
  {
      std::ostringstream msg;
      msg << "V-HACD V" << VHACD_VERSION_MAJOR << "." << VHACD_VERSION_MINOR << endl;
      msg << "Syntax: testVHACD [options] --input infile.obj --output outfile.wrl --log logfile.txt" << endl << endl;
      msg << "Options:" << endl;
      msg << "       --input                     Wavefront .obj input file name"<< endl;
      msg << "       --output                    VRML 2.0 output file name" << endl;
      msg << "       --log                       Log file name" << endl;
      msg << "       --resolution                Maximum number of voxels generated during the voxelization stage (default=100,000, range=10,000-16,000,000)" << endl;
      msg << "       --depth                     Maximum number of clipping stages. During each split stage, parts with a concavity higher than the user defined threshold are clipped according the \"best\" clipping plane (default=20, range=1-32)" << endl;
      msg << "       --concavity                 Maximum allowed concavity (default=0.0025, range=0.0-1.0)" << endl;
      msg << "       --planeDownsampling         Controls the granularity of the search for the \"best\" clipping plane (default=4, range=1-16)" << endl;
      msg << "       --convexhullDownsampling    Controls the precision of the convex-hull generation process during the clipping plane selection stage (default=4, range=1-16)" << endl;
      msg << "       --alpha                     Controls the bias toward clipping along symmetry planes (default=0.05, range=0.0-1.0)" << endl;
      msg << "       --beta                      Controls the bias toward clipping along revolution axes (default=0.05, range=0.0-1.0)" << endl;
      msg << "       --gamma                     Controls the maximum allowed concavity during the merge stage (default=0.00125, range=0.0-1.0)" << endl;
      msg << "       --delta                     Controls the bias toward maximaxing local concavity (default=0.05, range=0.0-1.0)" << endl;
      msg << "       --pca                       Enable/disable normalizing the mesh before applying the convex decomposition (default=0, range={0,1})" << endl;
      msg << "       --mode                      0: voxel-based approximate convex decomposition, 1: tetrahedron-based approximate convex decomposition (default=0, range={0,1})" << endl;
      msg << "       --maxNumVerticesPerCH       Controls the maximum number of triangles per convex-hull (default=64, range=4-1024)" << endl;
      msg << "       --minVolumePerCH            Controls the adaptive sampling of the generated convex-hulls (default=0.0001, range=0.0-0.01)" << endl;
      msg << "       --convexhullApproximation   Enable/disable approximation when computing convex-hulls (default=1, range={0,1})" << endl;
      msg << "       --oclAcceleration           Enable/disable OpenCL acceleration (default=0, range={0,1})" << endl;
      msg << "       --oclPlatformID             OpenCL platform id (default=0, range=0-# OCL platforms)" << endl;
      msg << "       --oclDeviceID               OpenCL device id (default=0, range=0-# OCL devices)" << endl;
      msg << "       --help                      Print usage" << endl << endl;

      msg << "Usage: "<<std::endl;
      msg << "./mesh_convex_decomposition fileName.off resolution maxDepth maxConcavity planeDownsampling convexhullDownsampling alpha beta gamma pca mode maxCHVertices minVolumePerCH outFileName.wrl log.txt"<<std::endl<<std::endl;
      msg << "Examples:" << endl;
      msg << "\t./mesh_convex_decomposition --input bunny.obj --output bunny_acd.wrl --log log.txt" << endl << endl;
      msg << "Recommended parameters:"<<std::endl;
      msg << "--input mesh.off --resolution 1000 --depth 20 --concavity 0.001 --planeDownsampling 4 --convexhullDownsampling 4 "<<
          "--alpha 0.05 --beta 0.05 --gamma 0.0001 --pca 0 --mode 0 --maxNumVerticesPerCH 256 --minVolumePerCH 0.0001 "<<
          "--output output.wrl --log log.txt"<<std::endl;

      cout << msg.str();
      if (params.m_paramsVHACD.m_logger)
      {
          params.m_paramsVHACD.m_logger->Log(msg.str().c_str());
      }
  }

  bool loadOFF(const string              & fileName,
                     vector< float >     & points,
                     vector< int>        & triangles,
                     IVHACD::IUserLogger & logger)
  {
      FILE * fid = fopen(fileName.c_str(), "r");
      if (fid)
      {
          const string strOFF("OFF");
          char temp[1024];
          fscanf(fid, "%s", temp);
          if (string(temp) != strOFF)
          {
              logger.Log("Loading error: format not recognized \n");
              fclose(fid);
              return false;
          }
          else
          {
              int nv = 0;
              int nf = 0;
              int ne = 0;
              fscanf(fid, "%i", &nv);
              fscanf(fid, "%i", &nf);
              fscanf(fid, "%i", &ne);
              points.resize(nv*3);
              triangles.resize(nf*3);
              const int np = nv * 3;
              for (int p = 0; p < np; p++)
              {
                  fscanf(fid, "%f", &(points[p]));
              }
              int s;
              for (int t = 0, r = 0; t < nf; ++t) {
                  fscanf(fid, "%i", &s);
                  if (s == 3)
                  {
                      fscanf(fid, "%i", &(triangles[r++]));
                      fscanf(fid, "%i", &(triangles[r++]));
                      fscanf(fid, "%i", &(triangles[r++]));
                  }
                  else            // Fix me: support only triangular meshes
                  {
                      for (int h = 0; h < s; ++h) fscanf(fid, "%i", &s);
                  }
              }
              fclose(fid);
          }
      }
      else
      {
          logger.Log("Loading error: file not found \n");
          return false;
      }
      return true;
  }
  bool loadOBJ(const string              & fileName,
                     vector< float >     & points,
                     vector< int >       & triangles,
                     IVHACD::IUserLogger & logger)
  {
      const char ObjDelimiters[] = " /";
      const unsigned int BufferSize = 1024;
      FILE * fid = fopen(fileName.c_str(), "r");

      if (fid)
      {
          char buffer[BufferSize];
          int  ip[3];
          int  in[3];
          int  it[3];
          float x[3];
          char * pch;
          char * str;
          size_t nn = 0;
          size_t nt = 0;
          while (!feof(fid))
          {
              if (!fgets(buffer, BufferSize, fid))
              {
                  break;
              }
              else if (buffer[0] == 'v')
              {
                  if (buffer[1] == ' ')
                  {
                      str = buffer + 2;
                      for (int k = 0; k < 3; ++k)
                      {
                          pch = strtok(str, " ");
                          if (pch) x[k] = (float)atof(pch);
                          else
                          {
                              return false;
                          }
                          str = NULL;
                      }
                      points.push_back(x[0]);
                      points.push_back(x[1]);
                      points.push_back(x[2]);
                  }
                  else if (buffer[1] == 'n')
                  {
                      ++nn;
                  }
                  else if (buffer[1] == 't')
                  {
                      ++nt;
                  }
              }
              else if (buffer[0] == 'f')
              {

                  str = buffer + 2;
                  for (int k = 0; k < 3; ++k)
                  {
                      pch = strtok(str, ObjDelimiters);
                      if (pch) ip[k] = atoi(pch) - 1;
                      else
                      {
                          return false;
                      }
                      str = NULL;
                      if (nt > 0)
                      {
                          pch = strtok(NULL, ObjDelimiters);
                          if (pch)  it[k] = atoi(pch) - 1;
                          else
                          {
                              return false;
                          }
                      }
                      if (nn > 0)
                      {
                          pch = strtok(NULL, ObjDelimiters);
                          if (pch)  in[k] = atoi(pch) - 1;
                          else
                          {
                              return false;
                          }
                      }
                  }
                  triangles.push_back(ip[0]);
                  triangles.push_back(ip[1]);
                  triangles.push_back(ip[2]);
              }
          }
          fclose(fid);
      }
      else
      {
          logger.Log("File not found\n");
          return false;
      }
      return true;
  }
  bool saveOFF(const string              & fileName,
               const float * const       & points,
               const int * const         & triangles,
               const unsigned int        & nPoints,
               const unsigned int        & nTriangles,
                     IVHACD::IUserLogger & logger)
  {
      ofstream fout(fileName.c_str());
      if (fout.is_open())
      {
          size_t nV = nPoints * 3;
          size_t nT = nTriangles * 3;
          fout << "OFF" << std::endl;
          fout << nPoints << " " << nTriangles << " " << 0 << std::endl;
          for (size_t v = 0; v < nV; v+=3)
          {
              fout << points[v+0] << " "
                   << points[v+1] << " "
                   << points[v+2] << std::endl;
          }
          for (size_t f = 0; f < nT; f+=3)
          {
              fout << "3 " << triangles[f+0] << " "
                           << triangles[f+1] << " "
                           << triangles[f+2] << std::endl;
          }
          fout.close();
          return true;
      }
      else
      {
          logger.Log("Can't open file\n");
          return false;
      }
  }
  bool saveVRML2(      ofstream            & fout,
                 const double * const& points,
                 const int * const   & triangles,
                 const unsigned int  & nPoints,
                 const unsigned int  & nTriangles,
                 const Material      & material,
                 IVHACD::IUserLogger & logger)
  {
      if (fout.is_open())
      {
          fout.setf(std::ios::fixed, std::ios::floatfield);
          fout.setf(std::ios::showpoint);
          fout.precision(6);
          size_t nV = nPoints*3;
          size_t nT = nTriangles*3;
          fout << "#VRML V2.0 utf8" << std::endl;
          fout << "" << std::endl;
          fout << "# Vertices: " << nPoints << std::endl;
          fout << "# Triangles: " << nTriangles << std::endl;
          fout << "" << std::endl;
          fout << "Group {" << std::endl;
          fout << "    children [" << std::endl;
          fout << "        Shape {" << std::endl;
          fout << "            appearance Appearance {" << std::endl;
          fout << "                material Material {" << std::endl;
          fout << "                    diffuseColor " << material.m_diffuseColor[0] << " "
                                                      << material.m_diffuseColor[1] << " "
                                                      << material.m_diffuseColor[2] << std::endl;
          fout << "                    ambientIntensity " << material.m_ambientIntensity << std::endl;
          fout << "                    specularColor " << material.m_specularColor[0] << " "
                                                       << material.m_specularColor[1] << " "
                                                       << material.m_specularColor[2] << std::endl;
          fout << "                    emissiveColor " << material.m_emissiveColor[0] << " "
                                                       << material.m_emissiveColor[1] << " "
                                                       << material.m_emissiveColor[2] << std::endl;
          fout << "                    shininess " << material.m_shininess << std::endl;
          fout << "                    transparency " << material.m_transparency << std::endl;
          fout << "                }" << std::endl;
          fout << "            }" << std::endl;
          fout << "            geometry IndexedFaceSet {" << std::endl;
          fout << "                ccw TRUE" << std::endl;
          fout << "                solid TRUE" << std::endl;
          fout << "                convex TRUE" << std::endl;
          if (nV > 0)
          {
              fout << "                coord DEF co Coordinate {" << std::endl;
              fout << "                    point [" << std::endl;
              for (size_t v = 0; v < nV; v+=3)
              {
                  fout << "                        " << points[v+0] << " "
                                                     << points[v+1] << " "
                                                     << points[v+2] << "," << std::endl;
              }
              fout << "                    ]" << std::endl;
              fout << "                }" << std::endl;
          }
          if (nT > 0)
          {
              fout << "                coordIndex [ " << std::endl;
              for (size_t f = 0; f < nT; f+=3)
              {
                  fout << "                        " << triangles[f+0] << ", "
                                                     << triangles[f+1] << ", "
                                                     << triangles[f+2] << ", -1," << std::endl;
              }
              fout << "                ]" << std::endl;
          }
          fout << "            }" << std::endl;
          fout << "        }" << std::endl;
          fout << "    ]" << std::endl;
          fout << "}" << std::endl;
          return true;
      }
      else
      {
          logger.Log("Can't open file\n");
          return false;
      }
  }


protected:

  Parameters params_;
  ProgressCallback progress_callback_;
  Logger logger_;


};

int main(int argc, char** argv)
{
  srand(time(0));
  MeshConvexApproximation mca;
  if(!mca.run(argc,argv))
  {
    return -1;
  }

  return 0;
}
