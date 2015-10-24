cmake_minimum_required (VERSION 2.6)
project (xslib)
set (xslib_sources main.cpp minilzo-2.09/minilzo.c xcomreader.cpp)

include_directories("minilzo-2.09")
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_executable (xslib ${xslib_sources})
set_target_properties (xslib PROPERTIES LINKER_LANGUAGE CXX)


