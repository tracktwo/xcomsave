cmake_minimum_required (VERSION 2.6)
project (xslib)
set (xslib_sources main.cpp minilzo-2.09/minilzo.c xcomreader.cpp crc32.cpp jsonwriter.cpp json11/json11.cpp)
set (xslib_headers xcomreader.h xcom.h util.h jsonwriter.h json11/json11.hpp)

include_directories("minilzo-2.09" "json11")
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
add_executable (xslib ${xslib_sources} ${xslib_headers})
set_target_properties (xslib PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(xslib crypt32)

