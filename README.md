# opentibiaunity-datspr-converter

### Building

Requires the .NET SDK 8.0 or later (Linux, macOS or Windows). Dependencies are restored from NuGet on build; none are Windows-specific.

```sh
dotnet build -c Release opentibiaunity_datspr_converter.csproj
```

### How to use?

For legacy clients (< 11), put Tibia.dat and Tibia.spr in a directory named after the client version (e.g. `1098/Tibia.dat`) alongside the application. For Tibia 11+ clients, put the client assets (catalog-content.json, appearances and sprite files) in `<version>/<build-version>/assets`. The converted appearances.otud and assets.otus are written to a `result` directory.

#### Available Options
| Option   |      Usage    |
|----------|:-------------:|
| --version* |  specify the version of the client |
| --build-version | specify the build version of the client (required for 11+) |
| --alpha | specify whether to use transparency channel or not |
| --convert-to | convert Tibia.dat/Tibia.spr to another client version instead |

> Note, options with `*` are required.

#### Example
``./bin/Release/net8.0/opentibiaunity_datspr_converter --version=1098 --alpha=1``
