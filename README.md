
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

Settings `BUILD_TESTS` to `OFF` will likely be required on systems other than Windows
as the CMake files have been written with that system in mind. <br/>
The rest of the project should okay as long as the dependencies are found in the 
CMake search paths.

A C++17 compiler is required to build this project.

## Using the tool

Running `cppscanner` with no arguments or with the `--help` option prints the help.

Currently, `run` is the only valid command; and it is used to create a snapshot of
a program.

Syntax:
```
cppscanner run --compile-commands <compile_commands.json> --output <snapshot.db> [options]
```

The input is taken as a `compile_commands.json` file and the output is a SQLite database.

### Getting a `compile_commands.json` with CMake

Getting a `compile_commands.json` is relative easy if you are using CMake: set the 
`CMAKE_EXPORT_COMPILE_COMMANDS` variable to `ON` when generating the project.

A minimal working example is available in [tests/simple_project](tests/simple_project).

On Windows, if you are using a "Visual Studio" generator, you will have to switch
to the "NMake Makefiles" generator. <br/>
The `setup_test_project` macro in [tests/CMakeLists.txt](tests/CMakeLists.txt)
demonstrates how this can be done using the "-G" option of CMake.

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

`-f`: specifies a glob pattern for filtering the files to index (only the files
matching the pattern are indexed).

`-f:tu`: specifies a glob pattern for filtering the translation unit to index
(only the translation units matching the pattern are indexed).
There is usually on translation unit per source file (e.g., `main.cpp`).
