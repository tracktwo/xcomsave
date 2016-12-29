# xcomsave

An open-source, portable library for reading XCOM Enemy Within save files, plus some small utilities to convert an
XCOM save to and from a somewhat readable JSON format.

Currently only strategy (geoscape) saves are supported. Tactical saves have a slightly different format that I can't
completely parse yet. This has also only been tested with the English version of the game. Other locales may or
may not successfully load. Please send me a save file if you have trouble and I can attempt a fix!

For Enemy Unknown saves, please see the fork by golinski: https://github.com/golinski/xcomsave. 

## JSON Utilities

The xcom2json and json2xcom tools convert to and from a JSON representation of the save file. No real processing is done
on the save, it's almost entirely a 1:1 translation from the binary format into a JSON representation. This makes it 
easy to map the .json file contents to the classes found in the XComGame.upk and XComStrategyGame.upk package files,
but some fields are non-obvious.

Binary versions of the utilities are provided for Windows in the "Releases" tab. For other platforms, please build from
source.

## Portable Save Library

The json utilities are built on top of a portable library xcomsave that does all the actual reading and writing of the
xcom save. Developers looking to build save reading or modifying utilities can use this tool to handle the dirty work
of managing the save file format. Please clone the library to build the xcomsave library for your platform. It's been
tested to work on Windows and Linux. Mac support should be straightforward to add if it doesn't work out-of-the-box.

## Building from Source

### All Platforms

CMake is needed for all platforms. Run it on the provided CMakeLists.txt file to build the project/build files for your 
platform. I recommend running it from a subdirectory (e.g. build) so it doesn't pollute the main repo with build
artifacts.

### Windows

Windows builds require Visual Studio 2015. Community Edition is free and will work fine, but older versions of Visual 
Studio will not.

### Linux

Requires GCC (or Clang), libiconv, and GNU make. Tested on Debian 8.


