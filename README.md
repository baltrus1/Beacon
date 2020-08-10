# Beacon
BeaconServer broadcasts a message periodically and per each client request it acts as a proxy to retrieve server list from external server and pass it on to a client.
BeaconClient is an example of such client that listens to a specific broadcast message and asks it to get some data from external server.

## Instructions to build on Windows Visual studio
Make sure you have CMake installed, if not, install it with vcpkg.
Open project with Visual studio as CMake project. In VS2019 its File->Open->Cmake and navigate to project root, open CMakeLists.txt.

Download boost (https://www.boost.org/users/download/) and unzip it to 
i.e. C:\Program Files\boost (if you move somewhere else change the path variable BOOST_ROOT in CMakeSetting.json accordingly.

Install boost, jsoncpp and openssl:
  vcpkg.exe install boost jsoncpp openssl

Build with Right click on CMakeLists.txt -> build.
If some dlls are missing, copy them from <vcpkg_root>\installed\x86-windows\bin\x.dll to Beacon\out\build\x86-Debug\BeaconServer\x.dll
