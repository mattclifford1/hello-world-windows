cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

# set the project name
project(ims)

# torch library
find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

#opencv
#find_package(OpenCV REQUIRED)
#include_directories( ${OpenCV_INCLUDE_DIRS} )

# get all cpp source files
file(GLOB SOURCES "src/*.cpp")

# for library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# OFX headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include-ofx)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include-local)


add_library(ims SHARED ${SOURCES})
SET_TARGET_PROPERTIES(ims PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(ims PROPERTIES SUFFIX ".ofx")
target_link_libraries(ims "${TORCH_LIBRARIES}")
#target_link_libraries(ims ${OpenCV_LIBS} )
target_link_libraries(ims)


set_property(TARGET ims PROPERTY CXX_STANDARD 14)
