
# cppscanner

`cppscanner` is a standalone command-line utility to create snapshots of C++ programs.
It is based on clang's [LibTooling](https://clang.llvm.org/docs/LibTooling.html).

Snapshots are saved as a SQLite database, making them easily usable in other programs.

## Compiling the project

The following dependencies are required to build the project:
- Clang 18
- SQLite 3

This project uses the CMake build system.
The following options are available:
- `BUILD_TESTS`: whether the tests should be built.

A C++17 compiler is required to build this project.

**Getting the clang development binaries (Debian/Ubuntu)**

Use the "Automatic installation script" from [apt.llvm.org](https://apt.llvm.org/), 
then install the clang development binaries.

```
apt-get install libclang-18-dev
```

The `node:21-bookworm`-based `Dockerfile` provided in the `docker` directory ([here](docker/Dockerfile))
shows how to use this method on a Debian system, and can be used to build a Docker image and compile 
the project in a container.

**Getting the clang development binaries (other systems)**

Some binaries can be downloaded directly from [GitHub](https://github.com/llvm/llvm-project/releases)
("clang+llvm" release). 
Note for Windows users: you won't be able to build in debug mode as only the release binaries are
provided; which will be penalizing if you intend to work on the project rather than just build it.

For all other systems, on for Windows users who want to build in Debug mode, 
the correct way to go seems to be to build LLVM yourself (as I did).
This may help: [Building LLVM with CMake](https://llvm.org/docs/CMake.html).
Building LLVM isn't hard, it just takes a lot of time and produces a lot of binaries
(my personal Release+Debug builds take around 13 gigabytes of disk space).

## Using the tool

Running `cppscanner` with no arguments or with the `--help` option prints the help.

The `run` command is the main command; and it is used to create a snapshot of
a program. Snapshots are saved as a SQLite database.

Syntax for creating a snapshot from a `compile_commands.json` compilation database:
```
cppscanner run --compile-commands <compile_commands.json> --output <snapshot.db> [options]
```

Syntax for creating a snapshot from a single file:
```
cppscanner run -i <source.cpp> --output <snapshot.db> [options] -- [compiler arguments]
```

Syntax for creating a snapshot from a CMake target (see below for prerequisites):
```
cppscanner run --build <cmake-build-dir> [--config <cmake-config>] --target <cmake-target-name> --output <snapshot.db> [options]
```

### Getting a `compile_commands.json` with CMake

Getting a `compile_commands.json` is relative easy if you are using CMake: set the 
`CMAKE_EXPORT_COMPILE_COMMANDS` variable to `ON` when generating the project.

A minimal working example is available in [tests/testprojects/hello_world](tests/testprojects/hello_world).

On Windows, if you are using a "Visual Studio" generator, you will have to switch
to the "NMake Makefiles" generator. <br/>
The `setup_test_project` macro in [tests/CMakeLists.txt](tests/CMakeLists.txt)
demonstrates how this can be done using the "-G" option of CMake.

### CMake support

The `run` command can be made to work on a CMake project if you generated the project
through the scanner beforehand.
This can be done using the `cmake` command.

```
cppscanner cmake -B <cmake-build-dir> -S <cmake-source-dir> [cmake-options]
```

This command will make the scanner use the CMake file-based API to get information 
about the build system and targets.
Ultimately, the `cmake` executable is invoked with the arguments passed as-is.

### `run` options

The following optional arguments can be passed to the `run` command.

`--overwrite`: overwrites any existing output file.

`--home <home>`: specifies a "home" directory (defaults to the current working directory); 
by default only files within the home directory are indexed. 
This is usually set to the root of the project that is being indexed.

`--index-external-files`: specifies that files outside of the home directory should
also be indexed.

`--index-local-symbols`: specifies that local symbols (that is variables in function
bodies) should be indexed; this is false by default (function bodies are indexed).

`-f <pattern>`: specifies a glob pattern for filtering the files to index (only the files
matching the pattern are indexed).

`-f:tu <pattern>`: specifies a glob pattern for filtering the translation unit to index
(only the translation units matching the pattern are indexed).
There is usually on translation unit per source file (e.g., `main.cpp`).

`--threads <count>`: specifies a number of dedicated threads to use for parsing C++.
The default is zero (the program runs in a single-threaded mode and parsing is done in 
the main thread). 
A value of 1 means that all the parsing is done in a (single) secondary thread while 
the output database is written in the main thread. The performance benefit should be
small because parsing takes most of the time.
The recommended minimum when using this option is therefore 2.

### Unsupported language features & toolchains

Although the parser may be able to process some other C++20 constructs (such as
the spaceship operator), **C++20 modules** are not supported.

Precompiled headers are not supported.
