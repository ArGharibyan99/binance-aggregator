# Binance Aggregator

C++20 Linux service that connects to Binance's public WebSocket market data stream, aggregates real-time trading statistics per symbol over fixed exchange-time windows, and writes them to a file on a fixed local-time interval.

## Current Status

The full pipeline described below is implemented and wired together in `main.cpp`: it connects to live Binance, parses trades, aggregates them into windows, and writes formatted output, with reconnect-with-backoff and graceful shutdown.

```text
- CMake build system with automatic Conan dependency bootstrap
- Fixed-point Decimal type and TradeEvent model (no floating point for prices/volume)
- Binance trade JSON parser (raw and combined-stream messages)
- Exchange-time window aggregation (MarketDataAggregator)
- Statistics serialization and file output (StatsSerializer, FileOutputSink)
- Configuration loading and validation (ConfigLoader), wired into main.cpp via a --config flag
- Bounded, shutdown-aware blocking queues used between pipeline stages
- Offline processing pipeline (parser -> aggregation -> writer stages)
- Binance WebSocket client (Boost.Beast/Asio + TLS)
- Reconnect policy: exponential backoff with jitter, based on config
- Graceful shutdown on SIGINT/SIGTERM, plus a systemd unit for deployment
- Generated Doxygen documentation and Graphviz architecture diagrams
- Unit and integration test suites (GoogleTest)
- Docker multi-stage build (base/deps/build/runtime) with layer-cached Conan deps
- Docker distributable image bundle (dist/docker)
```

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
│   ├── docker.cmake
│   └── documentation.cmake
├── config/
│   └── config.json
├── app/
│   ├── main.cpp
│   └── include/agg/ and src/ (mirrored layout), covering:
│       ├── config/       Config, ConfigLoader
│       ├── util/         Decimal (fixed-point price/quantity/volume)
│       ├── model/        TradeEvent
│       ├── parse/        BinanceTradeParser
│       ├── aggregation/  WindowStats, MarketDataAggregator
│       ├── output/       StatsSnapshot, StatsSerializer, OutputSink, FileOutputSink
│       ├── net/          ConnectionError, BinanceWebSocketClient, ReconnectPolicy
│       └── runtime/      CliOptions, BoundedQueue, MarketDataPipeline, NetworkStage
├── tests/
│   ├── unit/          one test file per component, e.g. DecimalTest.cpp
│   └── integration/   MarketDataPipelineIntegrationTest.cpp,
│                       BinanceWebSocketClientIntegrationTest.cpp,
│                       NetworkStageIntegrationTest.cpp
├── docs/
│   ├── Doxyfile.in
│   ├── runtime_pipeline.gv
│   └── object_dependencies.gv
├── service/
│   └── binance-aggregator.service
└── dist/
    └── docker/          (generated: distributable Docker image tarball)
```

## Architecture

### Runtime pipeline and thread ownership

```text
Binance WebSocket
    ↓
Network thread (NetworkStage: BinanceWebSocketClient + ReconnectPolicy)
    ↓ raw message queue (BoundedQueue<string>)
Parser thread (BinanceTradeParser)
    ↓ trade event queue (BoundedQueue<TradeEvent>)
Aggregation thread (MarketDataAggregator)
    ↓ (shared aggregator, guarded by a mutex)
Writer thread (StatsSerializer + OutputSink, woken by a local steady-clock timer)
    ↓
Output file
```

A rendered version of this diagram is available via the `docs` target (see Documentation Generation below).

Four threads run concurrently, each owned by one of two orchestrating classes:

```text
NetworkStage          owns the network thread: runs BinanceWebSocketClient::run() in a loop,
                       retrying via ReconnectPolicy on failure, until stop().
MarketDataPipeline     owns the parser, aggregation, and writer threads: parser pops raw
                       messages and pushes parsed trades; aggregation pops trades into the
                       shared MarketDataAggregator; writer wakes on flush_interval_ms and
                       extracts/serializes/writes completed windows.
```

The aggregator is the only object shared between two threads (aggregation writes to it, the
writer reads/extracts from it); access is serialized by a single mutex inside
`MarketDataPipeline`. Everything else communicates only through the bounded queues.

### Clock separation

The service deliberately uses two independent clocks:

```text
Exchange timestamp   decides which aggregation window a trade belongs to.
                     window_start_ms = trade_time_ms - trade_time_ms % window_ms

Local/hardware time  decides when the writer thread wakes up to flush completed
                     windows to disk (flush_interval_ms), via a steady-clock-based
                     condition_variable wait — unaffected by system clock adjustments.
```

These are independent settings: `window_ms` controls how trades are bucketed by
exchange time, `flush_interval_ms` controls how often the writer wakes up by local
time. For example, `window_ms=1000` with `flush_interval_ms=5000` aggregates trades
into 1-second exchange-time windows but only writes them to disk every 5 real
seconds; several completed windows can be written together in one flush. The
`timestamp=` field in the output always reflects the exchange-time window start,
never the local write time.

### Late-trade behavior

At the aggregator level, a trade whose window was already flushed (extracted)
starts a fresh bucket for that window rather than being merged back into the
already-written stats — that bucket would appear as a second, partial window on
the next flush. This is an intentionally simple policy, exercised by
`MarketDataAggregatorTest.LateTradeAfterExtractionStartsFreshWindow`.

In practice this is avoided for the periodic flush: the writer thread's periodic
tick withholds whatever window is still "current" by wall clock
(`MarketDataAggregator::extract_completed_windows`), only extracting windows
strictly older than the one presently in progress. This means a window is never
flushed while it could still receive more trades under normal message delay, so
a `timestamp=` line is not duplicated in ordinary operation. The one exception is
the final flush performed on shutdown (`stop()`), which is unconditional — it
extracts every remaining window, including the current one, since no further
trades can arrive after that point.

### Failure handling

```text
WebSocket connection failures   Classified into one of: DNS failure, TCP connect
                                 failure, TLS handshake failure, WebSocket handshake
                                 failure, read failure, timeout, or server disconnect
                                 (agg::net::ConnectionErrorKind). Logged by main.cpp.

Reconnect                       NetworkStage retries with exponential backoff
                                 (ReconnectPolicy): delay doubles from
                                 initial_backoff_ms up to max_backoff_ms, randomized
                                 by jitter_ratio. The backoff resets once a
                                 connection gets far enough to read at least one
                                 message, so a later drop of a healthy connection
                                 does not inherit an escalated delay.

Malformed market messages       BinanceTradeParser returns std::nullopt for
                                 malformed JSON, missing/invalid fields, or
                                 unsupported event types. These are silently
                                 dropped; a single bad message never disrupts the
                                 pipeline.

Graceful shutdown                SIGINT/SIGTERM set an atomic flag (signal-safe:
                                 only a lock-free store happens in the handler).
                                 main.cpp then stops the network stage first
                                 (closing the connection, halting reconnect), then
                                 the pipeline (draining anything already queued and
                                 performing a final flush), before exiting.
```

## Targets

```text
agg_core             static library with all application logic
binance_aggregator    executable entry point
unit_tests            GoogleTest unit tests (when BUILD_TESTING=ON)
integration_tests     GoogleTest integration tests, e.g. the full offline pipeline
                      and network-failure/reconnect behavior (when BUILD_TESTING=ON)
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

Boost.Asio and Boost.Beast implement the Binance WebSocket client
(`agg::net::BinanceWebSocketClient`): TLS handshake (with SNI), WebSocket
handshake, and the synchronous read loop that feeds raw messages into the
pipeline.

### OpenSSL

Required because Binance WebSocket uses secure WebSocket connections:

```text
wss://stream.binance.com:9443
```

`wss://` means WebSocket over TLS.

### nlohmann_json

Used for:

```text
- reading config/config.json
- parsing Binance JSON trade messages (BinanceTradeParser)
```

Using one JSON library for both config and market data messages keeps the project simple.

### fmt

Used for clean string formatting, including spdlog's own formatting of log lines like:

```text
symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72
```

### spdlog

Used for service logging: startup, configuration errors, connection attempts,
reconnects, malformed messages, file write errors, and shutdown.

### GTest

Used for unit and integration testing. Coverage includes decimal arithmetic,
trade parsing, window aggregation, statistics serialization, bounded queues,
WebSocket connection-failure classification and reconnect backoff, the full
offline processing pipeline, and network-stage retry/stop behavior. Network
tests never require live Binance access (they use loopback connections that
fail immediately, or pure logic with synthetic error codes); a live connection
is manually runnable but not required by the automated suite.

## Build System

The project uses:

```text
CMake
Conan
Docker
```

CMake defines the project targets.

Conan resolves and installs third-party C++ dependencies.

Docker provides a clean Linux build/test environment and produces the runnable service image.

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

Pass a config file path with `--config <path>` (defaults to `config/config.json`).

## Config Loading and Validation

Configuration is loaded by `agg::config::ConfigLoader::load_from_file()`.

The loader validates the file and throws descriptive errors instead of starting with a broken configuration:

```text
- all required fields must be present
- symbols must be a non-empty array of non-empty alphanumeric strings
- symbols are normalized to uppercase (btcusdt → BTCUSDT)
- numeric fields must be positive unsigned integers
- reconnect.initial_backoff_ms must not exceed reconnect.max_backoff_ms
```

Config loading is covered by unit tests in `tests/unit/ConfigLoaderTest.cpp`.

## Config Fields

### symbols

List of trading pairs to subscribe to. Converted to lowercase Binance combined-stream
names by `agg::net::build_combined_stream_target`, e.g. `["BTCUSDT", "ETHUSDT"]` becomes:

```text
/stream?streams=btcusdt@trade/ethusdt@trade
```

### window_ms

Aggregation window size in milliseconds (exchange time). Example: `1000` means trades
are aggregated into 1-second exchange-time windows.

### flush_interval_ms

How often the writer thread wakes up (local/hardware time) to flush completed windows
to file. Example: `1000` means it wakes up every second.

### output_file

Output file path, appended to by `FileOutputSink`.

### ws_host / ws_port

Binance WebSocket host and TLS port, e.g. `stream.binance.com` / `9443`.

### reconnect

Reconnect backoff settings used by `agg::net::ReconnectPolicy`:

```json
"reconnect": {
  "initial_backoff_ms": 500,
  "max_backoff_ms": 30000,
  "jitter_ratio": 0.2
}
```

## Example Output

```text
timestamp=2026-01-12T14:23:20Z
symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72
symbol=ETHUSDT trades=231 volume=112.7 min=2289.2 max=2301.8 buy=120 sell=111
```

The timestamp is the exchange-time window start (UTC, ISO-8601). Symbols with no
trades in a window are skipped entirely — an empty window is never written.

## Docker Flow

The Dockerfile has four stages:

```text
base       Ubuntu 24.04: install build tools (cmake, ninja, git), Doxygen/Graphviz,
           and Conan
deps       Copies conanfile.txt and runs `conan install` (cached, layer-separated
           from source so dependency downloads are skipped when only source changes)
build      Copies the full source, configures with CMake, builds, runs unit and
           integration tests (ctest), generates and verifies Doxygen/Graphviz
           documentation, and installs to /install
runtime    Ubuntu 24.04 image containing only the installed files from the build
           stage, running as a non-root user, with `binance_aggregator` as the
           ENTRYPOINT
```

Building the image runs the full test suite and documentation generation as part
of the build layer, then produces a runnable service image (docs are not copied
into the runtime image, which stays lean).

### Build the image (runs tests and docs generation, produces a runnable image)

```text
docker build --build-arg BUILD_TYPE=Release -t binance_aggregator:Release .
```

This checks:

```text
- CMake configure
- Conan dependency installation
- C++ build
- Unit and integration tests (ctest, verbose)
- Doxygen HTML docs and Graphviz SVG diagrams generated and verified to exist
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

Run tests (unit and integration):

```text
ctest --test-dir build --output-on-failure
```

Install locally:

```text
cmake --install build --prefix dist/local
```

Run installed binary:

```text
./dist/local/bin/binance_aggregator --config config/config.json
```

## Systemd Deployment

`service/binance-aggregator.service` is written for the following layout:

```text
/opt/binance-aggregator/bin/binance_aggregator   installed binary (matches the
                                                  Docker runtime image's layout)
/etc/binance-aggregator/config.json               host-editable configuration
/var/lib/binance-aggregator/                      output/state directory, created
                                                   and managed automatically by
                                                   systemd's StateDirectory=
```

Install the binary to `/opt/binance-aggregator` (e.g. `cmake --install build --prefix /opt/binance-aggregator`),
place a config at `/etc/binance-aggregator/config.json`, create the `binance-aggregator`
system user/group, then:

```text
sudo cp service/binance-aggregator.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now binance-aggregator
```

The unit restarts the process on failure (`Restart=on-failure`) as a coarse safety
net on top of the application's own WebSocket reconnect logic, and applies standard
sandboxing (`NoNewPrivileges`, `ProtectSystem=strict`, `ProtectHome`, `PrivateTmp`).
Sending `SIGTERM` (what `systemctl stop` sends) triggers the same graceful shutdown
path described above.

## Documentation Generation

Doxygen (API reference from header/source comments) and Graphviz (hand-written
architecture diagrams under `docs/*.gv`) are optional locally: normal application
and test builds work with neither installed, since the `docs` target is only
defined when both are found.

Generate documentation locally (requires `doxygen` and `graphviz` installed):

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_DOCS=ON
cmake --build build --target docs
```

This runs both `doxygen_docs` (Doxygen HTML) and `graphviz_diagrams` (renders
every `docs/*.gv` file to SVG); each can also be built individually via
`--target doxygen_docs` or `--target graphviz_diagrams`.

Generated output is written under the build directory (not committed to the repo):

```text
build/documentation/doxygen/html/index.html
build/documentation/graphviz/runtime_pipeline.svg
build/documentation/graphviz/object_dependencies.svg
```

The Docker `build` stage always has Doxygen/Graphviz installed and always
generates and verifies these three files exist before installing, so a plain
`docker build` doubles as a documentation-generation check.

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

Recommended Docker validation (builds, tests, and generates/verifies docs):

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

## Notes

All components described above — trade model, parser, aggregator, serializer/output
sink, configuration wiring, bounded queues, the offline processing pipeline, the
Binance WebSocket client, reconnect handling, graceful shutdown, and generated
documentation — are implemented, tested, and wired together in `main.cpp`. The
service has been manually verified against live Binance: it connects, aggregates
real trade data, writes correctly formatted output, and shuts down cleanly on
`SIGTERM`.
