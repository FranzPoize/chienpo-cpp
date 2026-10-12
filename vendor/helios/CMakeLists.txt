# Standalone source-only SDK build; top-level integration uses cmake/Helios.cmake.
cmake_minimum_required(VERSION 3.23)
project(ChienpoHelios LANGUAGES CXX)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
find_package(Threads REQUIRED)
add_library(HeliosDACAPI SHARED HeliosDac.cpp shared_library/HeliosDacAPI.cpp idn/idn.cpp idn/idnServerList.cpp idn/plt-posix.cpp)
target_compile_features(HeliosDACAPI PRIVATE cxx_std_14)
target_link_libraries(HeliosDACAPI PRIVATE PkgConfig::LIBUSB Threads::Threads)
