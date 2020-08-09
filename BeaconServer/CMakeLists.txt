# CMakeList.txt : CMake project for BeaconServer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

include_directories(${Boost_INCLUDE_DIR})
# Add source to this project's executable.
add_executable (BeaconServer "BeaconServer.cpp" )

target_link_libraries(BeaconServer PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# TODO: Add tests and install targets if needed.
