cmake_minimum_required(VERSION 2.8.3)
project(convex_decomposition)

find_package(catkin REQUIRED COMPONENTS
  roscpp
)

set(CONVEX_DECOMPOSITION_SRC 
  src/vhacd/btAlignedAllocator.cpp
  src/vhacd/btConvexHullComputer.cpp  
  src/vhacd/VHACD.cpp  
  src/vhacd/vhacdICHull.cpp  
  src/vhacd/vhacdManifoldMesh.cpp  
  src/vhacd/vhacdMesh.cpp  
  src/vhacd/vhacdVolume.cpp)


###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## INCLUDE_DIRS: uncomment this if you package contains header files
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need
catkin_package(
  INCLUDE_DIRS include
  LIBRARIES convex_decomposition
  CATKIN_DEPENDS roscpp
#  DEPENDS system_lib
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
# include_directories(include)
include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

## Declare a cpp library
 add_library(${PROJECT_NAME}
   ${CONVEX_DECOMPOSITION_SRC}
 )
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})

add_executable(mesh_convex_decomposition src/mesh_convex_decomposition_node.cpp)
target_link_libraries(mesh_convex_decomposition ${PROJECT_NAME} ${catkin_LIBRARIES})


#############
## Install ##
#############


#############
## Testing ##
#############
