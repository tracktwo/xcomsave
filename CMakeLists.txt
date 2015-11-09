cmake_minimum_required (VERSION 2.6)
project (xslib)
set (xslib_sources minilzo-2.09/minilzo.c xcomreader.cpp xcomwriter.cpp util.cpp)
set (xslib_headers xcomreader.h xcomwriter.h xcom.h util.h xslib_internal.h)

include_directories("minilzo-2.09" "json11")
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
add_library (xslib ${xslib_sources} ${xslib_headers})
set_target_properties (xslib PROPERTIES LINKER_LANGUAGE CXX)

set (xcom2json_sources xcom2json.cpp)
set (xcom2json_headers)
add_executable (xcom2json ${xcom2json_sources} ${xcom2json_headers})
set_target_properties (xcom2json PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(xcom2json xslib)

set (json2xcom_sources json2xcom.cpp json11/json11.cpp)
set (json2xcom_headers json11/json11.hpp)
add_executable (json2xcom ${json2xcom_sources} ${json2xcom_headers})
set_target_properties (json2xcom PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(json2xcom xslib)

