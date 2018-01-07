cmake_minimum_required (VERSION 3.0)

project (xcomsave)
set (xcomsave_sources minilzo-2.09/minilzo.c xcomio.cpp xcomreader.cpp xcomwriter.cpp util.cpp)
set (xcomsave_headers xcomio.h xcom.h util.h)

# Linux-specific configuration
if (UNIX)
set (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++14)
endif (UNIX)

# External dependencies - json, lzo, zlib.
# They're all fairly small and simple enough to statically link into
# the project rather than adding them as cmake subdirectories. Zlib in
# particular is quirky as a subdirectory and despite following the thread
# at https://github.com/madler/zlib/issues/133 I can't get a build of
# as described without a polluted submodule repo.
include_directories("minilzo-2.09" "json11" "zlib")
add_definitions("-DSTDC -DNO_GZIP")
file(GLOB zlib_sources zlib/*.c zlib/*.h)

if (WIN32)
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -EHsc")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -EHsc")
endif (WIN32)

add_library (xcomsave ${xcomsave_sources} ${zlib_sources} ${xcomsave_headers})
set_target_properties(xcomsave PROPERTIES LINKER_LANGUAGE CXX)

set (xcom2json_sources xcom2json.cpp)
set (xcom2json_headers)
add_executable (xcom2json ${xcom2json_sources} ${xcom2json_headers})
set_target_properties (xcom2json PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(xcom2json xcomsave)

set (json2xcom_sources json2xcom.cpp json11/json11.cpp)
set (json2xcom_headers json11/json11.hpp)
add_executable (json2xcom ${json2xcom_sources} ${json2xcom_headers})
set_target_properties (json2xcom PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(json2xcom xcomsave)

