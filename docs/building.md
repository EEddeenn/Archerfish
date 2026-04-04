# Building Archerfish

## Prerequisites

- C++23 compiler: Clang 17+ (macOS), GCC 13+ (Linux)
- CMake 3.25+
- pkg-config

## Installing Dependencies

### macOS (Homebrew)

```bash
brew install fmt spdlog cli11 catch2 nlohmann-json libsamplerate pkg-config
# Optional: USRP support
brew install uhd
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config \
    libfmt-dev libspdlog-dev nlohmann-json3-dev libcatch2-3-dev \
    libsamplerate0-dev libcli11-dev
# Optional: USRP support
sudo apt-get install -y libuhd-dev
```

## Building

### Basic

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

### CMake Presets

```bash
cmake --preset dev          # Debug build with compile_commands.json
cmake --preset release      # Release build
cmake --preset dev-sanitize # Debug with ASAN + UBSAN
cmake --build --preset dev
ctest --preset dev
```

### Sanitizer Builds

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Testing

```bash
ctest --test-dir build --output-on-failure
# Or with presets
ctest --preset dev
```

137 tests covering unit, integration, and end-to-end.

## Installing

```bash
cmake --install build --prefix /usr/local
```

Schemas are installed to `${prefix}/share/archerfish/schemas/`. Installed binaries look for schemas in the install location first, then fall back to the source tree.

## Cross-Compilation Notes

- RPATH is configured for both macOS (`@executable_path`) and Linux (`$ORIGIN`)
- `FindSampleRate.cmake` module handles libsamplerate discovery via pkg-config fallback
- UHD is optional. The build continues without it (stub device used for testing)
- Compiler warnings are gated by compiler ID (GCC/Clang/AppleClang only)
- Sanitizer flags are gated by compiler ID (MSVC would be ignored)

## Dependency Table

| Dependency | macOS (Homebrew) | Ubuntu/Debian | Discovery |
|------------|------------------|---------------|-----------|
| fmt | `fmt` | `libfmt-dev` | find_package |
| spdlog | `spdlog` | `libspdlog-dev` | find_package |
| CLI11 | `cli11` | `libcli11-dev` | find_package |
| nlohmann_json | `nlohmann-json` | `nlohmann-json3-dev` | find_package |
| Catch2 | `catch2` | `libcatch2-3-dev` | find_package |
| libsamplerate | `libsamplerate` | `libsamplerate0-dev` | FindSampleRate.cmake (pkg-config fallback) |
| json-schema-validator | N/A (fetched) | N/A (fetched) | FetchContent from GitHub |
| UHD | `uhd` (optional) | `libuhd-dev` (optional) | find_package (QUIET) |

## Troubleshooting

**"SampleRate not found"**
Make sure pkg-config is installed. Verify with `pkg-config --exists samplerate`.

**"UHD not found"**
Optional. Builds without USRP support using a stub device.

**"C++23 features not compiling"**
Need Clang 17+ or GCC 13+. Check your compiler version with `clang++ --version` or `g++ --version`.

**"json-schema-validator fetch fails"**
Network issue. Check proxy/VPN settings. The tag `2.4.0` is fetched from GitHub.
