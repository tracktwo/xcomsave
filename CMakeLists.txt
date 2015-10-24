cmake_minimum_required (VERSION 2.6)
project (xslib)

include_directories("minilzo-2.09")
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_executable (xslib main.cpp platform_windows.cpp minilzo-2.09/minilzo.c )


