# EBU ADM Toolbox

The EAT is a set of tools for processing ADM (Audio Definition Model) files. It can convert ADM files between profiles, validate them, render them, fix common issues, and more.

It contains:
- A framework for building processing graphs that operate on audio and associated data.
- A set of ADM-related processes that sit within this framework.
- A command-line tool `eat-process`, which processes ADM files using the processes from the framework, as defined in a configuration file.
- A set of example configuration files for the tool, for doing things like profile conversion, loudness measurement, fixing common issues, validation etc.

For more information, see the documentation at https://ebu-adm-toolbox.readthedocs.io/

## Usage

The `eat-process` tool requires only one argument: the configuration file which
specifies the processes to run and how to connect them.

```bash
eat-process /path/to/configuration_file.json
```

Configuration files can specify everything required to perform some processing, but normally they leave parameters such as paths to input and output files unspecified. These can be passed on the command-line with the `-o` option, which takes a process and parameter name, and a value to set it to. For example:

```bash
eat-process example_configs/conform_block_timing.json -o input.path in.wav -o output.path out.wav
```

This will enforce a minimum block duration on `in.wav`, writing to `out.wav`.

For information on the config file format, options and available processes, see the documentation at https://ebu-adm-toolbox.readthedocs.io/

## Compiling
To compile the toolbox you will require a working C++20 development environment. It has been successfully compiled under:

* Windows using Visual Studio 2022
* MacOS 12.4 with Apple Clang 13.1.6
* Debian using GCC 12 (see CI)
* Debian using Clang 13 (see CI)

CMake version 3.21 or higher is required to configure and build the project.

If building via vcpkg/the supplied CMake presets, the Ninja build system is required along with the following:
```
// on macOS/linux
pkg-config

// on linux (macOS has these by default)
curl 
zip
unzip
tar
```

To build:
```bash
git clone git@git.ebu.io:as/adm-toolbox.git --recursive
cd adm-toolbox
# This step may not be neccessary and should only be required once
./external/vcpkg/bootstrap-vcpkg.sh # bootstrap_vcpkg.bat if on windows
# This step configures and builds using vcpkg, see CMakePresets.json for details
cmake --preset release && cmake --build --preset release
```

Replace the `git@...` url with the https equivalent if not using ssh, and replace release with debug if a debug build is required.

On Windows a 64 bit developer command prompt should be used, or the preset can be built directly from Visual Studio using its inbuilt CMake support.

The CMake preset uses vcpkg to build the project's dependencies, alternatively these can be manually built by the user
```
libadm v0.13
libbw64 v0.10
libear v0.9
libebur128 v1.2.6
cli11 v2.2.0
nlohmann/json v3.11.2

Catch2 v3.0.1 // Only required for tests
```

## Development Guidelines

### General approach
Try and adhere to the suggestions in the [pitchfork layout](https://vector-of-bool.github.io/psl.html) 
Using 'separate header placement' with merged tests (i.e. separate src/include paths but tests in src)

library is called 'eat' for EBU ADM Toolbox, it comprises of 
* framework (graph generation and execution) 
* processes (i/o, various metadata and audio manipulation)
These components are split into eat::framework and eat::process namespaces, also reflected in the directory structure

### Dependency management

* Use CPM.cmake for header only libraries, cmake config or small / quick to build dependencies?
* Document dependencies and attempt to find in exported config but do not force any management system on users?

### Formatting
* Use clang format and cmake formatting via Format.cmake

### Testing
* Catch2 v3 built as a static library and using minimal includes (start with catch/test_macros.hpp)
* Use built in cmake for test registration
* Start with one test binary for binary, probably add separate ones for algorithms as we add them

### Packaging
* export a cmake config for ease of inclusion

## License

Apache 2.0, see `LICENSE.txt`.
