# xcomsave

An open-source, portable library for reading XCOM Enemy Within save files, plus some small utilities to convert an
XCOM save to and from a somewhat readable JSON format.

Currently only strategy (geoscape) saves are supported. Tactical saves have a slightly different format that I can't
completely parse yet. I don't have exhaustive save samples for most languages, but I have used to this to successfully
read and write English, German, and Russian saves. If you encounter errors reading or writing a save with another language
please let me know.

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

Use `git clone --recursive` to obtain the source otherwise the required `json11` and `zlib` submodules will not be populated
and will need to be manually updated after cloning.

### Windows

Windows builds require Visual Studio 2017. Community Edition is free and will work fine, but older versions of Visual 
Studio will not.

### Linux

Requires GCC (or Clang), libiconv, and GNU make. Tested on Debian 8.

### MacOS X

The C++ compilers bundled with Xcode as of version 9.2 are too old to build this project. A more recent build of GCC or Clang
is required, for example from Homebrew.

## Contributors

- [tracktwo](github.com/tracktwo), who created it;
- [shaygover](github.com/shaygover), who added install instructions;
- [skywalkerytx](github.com/skywalkerytx), who fixed date problems;
- [golinski](github.com/golinski), who made the Enemy Unknown version;
- [Anders1232](github.com/Anders1232), who merged the Enemy Unknown version;
