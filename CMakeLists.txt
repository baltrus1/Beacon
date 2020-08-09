# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("Beacon")

add_compile_definitions(BOOST_ALL_NO_LIB)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS system)

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

include_directories(${Boost_INCLUDE_DIR})

# Include sub-projects.
add_subdirectory ("BeaconServer")
add_subdirectory ("BeaconClient")
