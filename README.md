# opentibiaunity-datspr-converter

### Building

The converter is written in C++ (`src/`). It needs a C++20 compiler, CMake 3.16+, protobuf (library and `protoc`), liblzma, libpng and nlohmann-json, all free software packaged by the usual distributions. On Debian or Ubuntu:

```sh
sudo apt install g++ cmake libprotobuf-dev protobuf-compiler liblzma-dev libpng-dev nlohmann-json3-dev
cmake -B build
cmake --build build -j
```

`proto/` holds the OpenTibiaUnity output schema, `proto/tibia/` the Tibia 11+ input schema; C++ code for both is generated at build time.

#### Original C# version

The C# sources are still included and need the .NET SDK 8.0 or later. Dependencies are restored from NuGet on build.

```sh
dotnet build -c Release opentibiaunity_datspr_converter.csproj
```

### How to use?

Paths are relative to the directory you run the converter from. For legacy clients (< 11), put Tibia.dat and Tibia.spr in a directory named after the client version (e.g. `1098/Tibia.dat`). For Tibia 11+ clients, put the client assets (catalog-content.json, appearances and sprite files) in `<version>/<build-version>/assets`. The converted appearances.otud and assets.otus are written to a `result` directory.

#### Available Options
| Option   |      Usage    |
|----------|:-------------:|
| --version* |  specify the version of the client |
| --build-version | specify the build version of the client (required for 11+) |
| --alpha | specify whether to use transparency channel or not |
| --convert-to | convert Tibia.dat/Tibia.spr to another client version instead |

> Note, options with `*` are required.

#### Example
``./build/opentibiaunity_datspr_converter --version=1098 --alpha=1``
