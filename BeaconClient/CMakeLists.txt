# CMakeList.txt : CMake project for BeaconClient, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable (BeaconClient "BeaconClient.cpp" )

find_package(jsoncpp CONFIG REQUIRED)

target_link_libraries(BeaconClient PUBLIC jsoncpp_lib)
target_link_libraries(BeaconClient PRIVATE OpenSSL::SSL OpenSSL::Crypto)

