cmake_minimum_required (VERSION 2.6)

project (xcomsave)
set (xcomsave_sources minilzo-2.09/minilzo.c xcomio.cpp xcomreader.cpp xcomwriter.cpp util.cpp)
set (xcomsave_headers xcomio.h xcom.h util.h)

# Linux-specific configuration
if (UNIX)
set (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++14)
endif (UNIX)

if (WIN32)
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -EHsc")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -EHsc")
endif (WIN32)

include_directories("minilzo-2.09" "json11" "zlib" "build/zlib")
add_library (xcomsave ${xcomsave_sources} ${xcomsave_headers})
target_link_libraries(xcomsave zlibstatic)
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

add_subdirectory(zlib)
