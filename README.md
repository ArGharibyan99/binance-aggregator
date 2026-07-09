# Binance Aggregator

C++ Linux service for collecting Binance public WebSocket market data and aggregating real-time trading statistics.

The project is currently in the infrastructure/setup stage. The build system, dependency management, Docker build/test flow, Docker binary export flow, host build flow, and configuration loading are prepared before implementing the Binance connection and aggregation logic.

## Current Status

Implemented so far:

```text
- New project structure
- CMake build system
- Conan dependency management
- Automatic Conan bootstrap from CMake
- Core library target (agg_core) and executable target
- Configuration loading and validation (ConfigLoader)
- Unit tests with GoogleTest (config loading)
- Runtime configuration file
- Docker multi-stage build (base/deps/build/runtime) with layer-cached Conan deps
- Docker distributable image bundle (dist/docker)
- Host build instructions
```

Application logic such as WebSocket connection, trade parsing, aggregation, file writing, and reconnect handling will be implemented in the next steps.

## Project Structure

```text
binance-aggregator/
├── CMakeLists.txt
├── conanfile.txt
├── Dockerfile
├── README.md
├── LICENSE
├── .dockerignore
├── .gitignore
├── cmake/
│   ├── conan.cmake
│   └── docker.cmake
├── config/
│   └── config.json
├── app/
│   ├── main.cpp
│   ├── include/agg/
│   │   └── config/
│   │       ├── Config.hpp
│   │       └── ConfigLoader.hpp
│   └── src/
│       └── config/
│           └── ConfigLoader.cpp
├── tests/
│   └── unit/
│       └── ConfigLoaderTest.cpp
├── docs/
│   └── runtime_pipeline.gv
├── service/
│   └── binance-aggregator.service
└── dist/
    └── docker/          (generated: distributable Docker image tarball)
```

New component directories (model, parse, aggregation, net, output, runtime, util) will be added under `app/include/agg/` and `app/src/` as the corresponding logic is implemented.

## Targets

The build defines two main targets:

```text
agg_core             static library with application logic (currently config loading)
binance_aggregator   executable entry point
unit_tests           GoogleTest unit tests (when BUILD_TESTING=ON)
```

The executable links `agg_core` together with Boost, OpenSSL, fmt, and spdlog.

## Dependencies

Dependencies are managed with Conan 2.x.

Current dependencies:

```text
boost/1.85.0
openssl/3.3.1
nlohmann_json/3.11.3
fmt/10.2.1
spdlog/1.14.1
gtest/1.14.0
```

## Why These Libraries Are Used

### Boost

Boost is used for networking infrastructure.

Later, the service will use:

```text
Boost.Asio
Boost.Beast
```

These are suitable for implementing the Binance WebSocket client.

### OpenSSL

OpenSSL is required because Binance WebSocket uses secure WebSocket connections:

```text
wss://stream.binance.com:9443
```

`wss://` means WebSocket over TLS.

### nlohmann_json

Used for:

```text
- reading config/config.json
- parsing Binance JSON trade messages later
```

Using one JSON library for both config and market data messages keeps the project simple.

### fmt

Used for clean string formatting.

Later it will be useful for formatting output lines like:

```text
symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72
```

### spdlog

Used for service logging.

A 24/7 service needs logs for:

```text
- startup
- configuration errors
- connection attempts
- reconnects
- malformed messages
- file write errors
- shutdown
```

### GTest

Used for unit testing.

Current tests cover config loading. The project will also include tests for:

```text
- trade parsing
- aggregation logic
- output formatting
- runtime behavior
```

## Build System

The project uses:

```text
CMake
Conan
Docker
```

CMake defines the project targets.

Conan resolves and installs third-party C++ dependencies.

Docker provides a clean Linux build/test environment and produces the exported binary.

## Conan and CMake Flow

The project uses automatic Conan bootstrap from CMake.

The flow is:

```text
CMake configure starts
    ↓
cmake/conan.cmake runs
    ↓
Conan profile is checked/detected
    ↓
Conan installs dependencies
    ↓
Conan generates CMake package files
    ↓
CMake find_package() finds dependencies
    ↓
Project builds normally
```

This means the developer usually does not need to run `conan install` manually.

## Configuration

The runtime configuration is located at:

```text
config/config.json
```

Current example:

```json
{
  "symbols": [
    "BTCUSDT",
    "ETHUSDT"
  ],
  "window_ms": 1000,
  "flush_interval_ms": 1000,
  "output_file": "market_stats.log",
  "ws_host": "stream.binance.com",
  "ws_port": "9443",
  "reconnect": {
    "initial_backoff_ms": 500,
    "max_backoff_ms": 30000,
    "jitter_ratio": 0.2
  }
}
```

## Config Loading and Validation

Configuration is loaded by `agg::config::ConfigLoader::load_from_file()`.

The loader validates the file and throws descriptive errors instead of starting with a broken configuration:

```text
- all required fields must be present
- symbols must be a non-empty array of non-empty alphanumeric strings
- symbols are normalized to uppercase (btcusdt → BTCUSDT)
- numeric fields must be positive unsigned integers
```

Config loading is covered by unit tests in `tests/unit/ConfigLoaderTest.cpp`.

## Config Fields

### symbols

List of trading pairs to subscribe to.

Example:

```json
"symbols": ["BTCUSDT", "ETHUSDT"]
```

Later these will be converted to Binance stream names:

```text
btcusdt@trade
ethusdt@trade
```

### window_ms

Aggregation window size in milliseconds.

Example:

```json
"window_ms": 1000
```

This means trades are aggregated into 1-second exchange-time windows.

### flush_interval_ms

How often the service writes completed statistics to file using local hardware time.

Example:

```json
"flush_interval_ms": 1000
```

This means the service writes completed windows every second.

### output_file

Output file path.

Example:

```json
"output_file": "market_stats.log"
```

### ws_host

Binance WebSocket host.

Example:

```json
"ws_host": "stream.binance.com"
```

### ws_port

Binance WebSocket TLS port.

Example:

```json
"ws_port": "9443"
```

### reconnect

Reconnect backoff settings.

Example:

```json
"reconnect": {
  "initial_backoff_ms": 500,
  "max_backoff_ms": 30000,
  "jitter_ratio": 0.2
}
```

This will be used later for reconnect logic.

## Timing Model

The service uses two different clocks.

### Exchange timestamp

Used for aggregation windows.

Each Binance trade message contains an exchange timestamp. That timestamp decides which aggregation window the trade belongs to.

Example:

```text
window_ms = 1000
trade timestamp = 14:23:20.735
window start = 14:23:20.000
```

### Hardware/local time

Used for deciding when to write completed windows to file.

Example:

```text
window_ms = 1000
flush_interval_ms = 5000
```

This means:

```text
Aggregate trades into 1-second exchange-time windows.
Write completed windows to file every 5 seconds by local time.
```

The output timestamp represents the exchange-time window start, not the local write time.

## Planned Runtime Architecture

The first implementation will use a simple staged architecture:

```text
Binance WebSocket
    ↓
Network thread
    ↓
Parser
    ↓
Aggregation shard
    ↓
Writer thread
    ↓
Output file
```

Initial implementation:

```text
1 network thread
1 parser
1 aggregation shard
1 writer thread
```

The parser and aggregation shard are not configurable at this stage. This keeps the first implementation simple and easier to test.

The design can later be extended to multiple parser workers and multiple aggregation shards if higher throughput is required.

## Planned Data Flow

```text
Raw Binance JSON message
    ↓
Trade parser
    ↓
Trade object
    ↓
Window aggregator
    ↓
Window statistics
    ↓
Stats writer
    ↓
Output file
```

Example output:

```text
timestamp=2026-01-12T14:23:20Z
symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72
symbol=ETHUSDT trades=231 volume=112.7 min=2289.2 max=2301.8 buy=120 sell=111
```

Symbols without trades in a window will be skipped.

## Docker Flow

The Dockerfile has four stages:

```text
base       Ubuntu 24.04: install build tools (cmake, ninja, git) and Conan
deps       Copies conanfile.txt and runs `conan install` (cached, layer-separated
           from source so dependency downloads are skipped when only source changes)
build      Copies the full source, configures with CMake, builds, runs unit tests
           (ctest), and installs to /install
runtime    Ubuntu 24.04 image containing only the installed files from the build
           stage, running as a non-root user, with `binance_aggregator` as the
           ENTRYPOINT
```

Building the image runs the full test suite as part of the build layer, then produces a runnable service image.

### Build the image (runs tests, produces a runnable image)

```text
docker build --build-arg BUILD_TYPE=Release -t binance_aggregator:Release .
```

This checks:

```text
- CMake configure
- Conan dependency installation
- C++ build
- Unit tests (ctest, verbose)
- CMake install step
```

### Run the built image

```text
docker run --rm binance_aggregator:Release
```

### Export a distributable image tarball

```text
cmake --build build-docker --target release_docker
```

This writes a gzipped `docker save` archive of the image to:

```text
dist/docker/binance_aggregator-Release.image.tgz
```

## Docker Targets Through CMake

The Docker commands are also exposed as CMake targets. Each target invokes `cmake/docker.cmake` in script mode (`cmake -P`), which performs the actual `docker build` / `docker save` calls.

Configure the wrapper build directory:

```text
cmake -S . -B build-docker -DCMAKE_BUILD_TYPE=Release
```

Build the image inside Docker:

```text
cmake --build build-docker --target docker
```

Build the image and export the distributable tarball to `dist/docker` (the directory is cleaned first):

```text
cmake --build build-docker --target release_docker
```

These CMake targets are wrappers around Docker commands. The application itself is built inside Docker.

## Host Build

The project can also be built directly on the host machine.

Required host tools:

```text
CMake
Conan 2.x
C++ compiler
Git
Python/pipx
```

Configure:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Build:

```text
cmake --build build -j
```

Run tests:

```text
ctest --test-dir build --output-on-failure
```

Install locally:

```text
cmake --install build --prefix dist/local
```

Run installed binary:

```text
./dist/local/bin/binance_aggregator
```

## Binary Locations

Host build binary:

```text
build/binance_aggregator
```

Host installed binary:

```text
dist/local/bin/binance_aggregator
```

Docker distributable image tarball:

```text
dist/docker/binance_aggregator-Release.image.tgz
```

## Current Validation Commands

Recommended Docker validation:

```text
docker build --build-arg BUILD_TYPE=Release -t binance_aggregator:Release .
```

Recommended Docker distributable export:

```text
cmake --build build-docker --target release_docker
```

Recommended host validation:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build --prefix dist/local
```

## Next Development Steps

Planned next steps:

```text
1. Trade model
2. Symbol registry
3. Binance trade JSON parser
4. Window aggregator
5. Stats writer
6. Runtime application (wire config loading into main)
7. Binance WebSocket client
8. Reconnect and failure handling
9. Integration tests
10. Final architecture documentation
```

## Notes

This project intentionally sets up build, dependency, test, and Docker infrastructure before implementing the market-data logic.

This makes later development easier because every new component can be tested both locally and inside a clean Docker environment.
