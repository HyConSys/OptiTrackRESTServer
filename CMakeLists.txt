cmake_minimum_required(VERSION 3.10)

# set the project name
project(OptiTrackRestServer VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# include vcpkg packages (change path if needed)
set(VCPKG_PATH ${PROJECT_SOURCE_DIR}/../vcpkg)
include("${VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake")

# required packages
find_package(cpprestsdk CONFIG REQUIRED)    

# add the executable
add_executable(OptiTrackRestServer 
    "${PROJECT_SOURCE_DIR}/src/main.cpp"
)

# add include dirs
target_include_directories(OptiTrackRestServer PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${VCPKG_PATH}/installed/x64-windows/include"
)

# additional link libraries
target_link_libraries(OptiTrackRestServer PRIVATE 
    cpprestsdk::cpprest 
    cpprestsdk::cpprestsdk_zlib_internal 
    cpprestsdk::cpprestsdk_brotli_internal
)