# Binance Aggregator

A C++20 Linux service that connects to Binance's public WebSocket market-data
stream, aggregates real-time trading statistics per symbol over fixed
exchange-time windows, and writes them to a file on a fixed local-time
interval — with automatic reconnect, multi-threaded aggregation, and graceful
shutdown.

```text
timestamp=2026-01-12T14:23:20Z
symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72
symbol=ETHUSDT trades=231 volume=112.7 min=2289.2 max=2301.8 buy=120 sell=111
```

## Quick Start

```bash
# 1. Configure + build (Conan dependencies are bootstrapped automatically)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 2. Run the tests
ctest --test-dir build --output-on-failure

# 3. Run the service against live Binance
./build/binance_aggregator --config config/config.json

# 4. In another terminal, watch the output live, colorized
./scripts/watch_stats.sh
```

Prefer Docker? See [Docker](#docker).

## Table of Contents

- [Features](#features)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Architecture](#architecture)
- [Multi-Threaded Aggregation (Sharding)](#multi-threaded-aggregation-sharding)
- [Output Format](#output-format)
- [Logging](#logging)
- [Build, Test, Run](#build-test-run)
- [Docker](#docker)
- [Systemd Deployment](#systemd-deployment)
- [Documentation Generation](#documentation-generation)
- [Dependencies](#dependencies)
- [Testing Strategy](#testing-strategy)

## Features

| Area | What it does |
|---|---|
| **Live data** | Connects to Binance's combined-stream WebSocket over TLS (Boost.Beast/Asio) and subscribes to configured symbols |
| **Parsing** | Handles both raw and combined-stream trade messages; silently drops malformed/unsupported messages without disrupting the service |
| **Aggregation** | Buckets trades into fixed exchange-time windows (count, volume, min/max price, buy/sell split); optionally sharded across multiple threads by symbol |
| **Precision** | Prices, quantities, and volume use a fixed-point `Decimal` type — never `double` — so no floating-point drift across millions of trades |
| **Output** | Deterministic, human-readable text file, one block per flush, sorted by symbol, blank-line separated |
| **Resilience** | Reconnects with exponential backoff + jitter on any connection failure (classified: DNS, TCP, TLS, WebSocket handshake, read, timeout, disconnect) |
| **Shutdown** | `SIGINT`/`SIGTERM` triggers an ordered, lossless shutdown: stop network → drain queues → final flush → exit |
| **Logging** | Color-coded console logs (green/yellow/red by level) that auto-detect real terminals vs. redirected output |
| **Viewing** | `scripts/watch_stats.sh` colorizes the output file live, without touching the plain-text file itself |
| **Deployment** | Docker multi-stage build (with docs generation + verification baked into `docker build`), or a systemd unit for bare-metal |
| **Docs** | Doxygen API reference + Graphviz architecture diagrams, generated on demand, never required for a normal build |
| **Testing** | Unit + integration tests (GoogleTest); network tests never require live Binance access |

## Project Structure

```text
binance-aggregator/
├── CMakeLists.txt
├── conanfile.txt
├── Dockerfile
├── README.md
├── config/
│   └── config.json
├── scripts/
│   └── watch_stats.sh          colorized live viewer for the output file
├── app/
│   ├── main.cpp                 wires everything together, signal handling
│   └── include/agg/ + src/ (mirrored layout):
│       ├── config/       Config, ConfigLoader
│       ├── util/         Decimal (fixed-point price/quantity/volume)
│       ├── model/        TradeEvent
│       ├── parse/        BinanceTradeParser
│       ├── aggregation/  WindowStats, MarketDataAggregator
│       ├── output/       StatsSnapshot, StatsSerializer, OutputSink, FileOutputSink
│       ├── net/          ConnectionError, BinanceWebSocketClient, ReconnectPolicy
│       └── runtime/      CliOptions, BoundedQueue, MarketDataPipeline, NetworkStage
├── tests/
│   ├── unit/          one test file per component
│   └── integration/   full-pipeline + network-failure/reconnect/sharding tests
├── docs/              Doxyfile.in, runtime_pipeline.gv, object_dependencies.gv
├── service/
│   └── binance-aggregator.service
├── cmake/             conan.cmake, docker.cmake, documentation.cmake
└── dist/docker/       (generated) distributable Docker image tarball
```

## Configuration

Edit `config/config.json`:

```json
{
  "symbols": ["BTCUSDT", "ETHUSDT"],
  "window_ms": 1000,
  "flush_interval_ms": 1000,
  "aggregator_threads": 1,
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

Pass a different path with `--config <path>` (defaults to `config/config.json`).

| Field | Required | Default | Meaning |
|---|---|---|---|
| `symbols` | yes | — | Trading pairs to subscribe to, e.g. `["BTCUSDT","ETHUSDT"]`. Normalized to uppercase; converted to lowercase Binance stream names internally (`btcusdt@trade`). |
| `window_ms` | yes | — | Aggregation window size in **exchange time**, milliseconds. |
| `flush_interval_ms` | yes | — | How often (in **local/wall-clock time**) the writer thread flushes completed windows to disk. |
| `aggregator_threads` | no | `1` | Number of independent aggregation shards/threads. See [Multi-Threaded Aggregation](#multi-threaded-aggregation-sharding). |
| `output_file` | yes | — | Path the output is appended to. |
| `ws_host` / `ws_port` | yes | — | Binance WebSocket host/port, e.g. `stream.binance.com` / `9443`. |
| `reconnect.initial_backoff_ms` | no | `500` | First reconnect delay after a connection failure. |
| `reconnect.max_backoff_ms` | no | `30000` | Reconnect delay cap (backoff doubles up to this). |
| `reconnect.jitter_ratio` | no | `0.2` | Randomizes each backoff delay by ±this fraction, to avoid thundering-herd reconnects. |

Validation (`ConfigLoader`) throws a descriptive error and refuses to start rather
than run with a broken config: missing/empty required fields, non-positive
numeric fields, non-alphanumeric symbols, or `initial_backoff_ms > max_backoff_ms`.

## Architecture

```text
Binance WebSocket
    │
    ▼
Network thread (NetworkStage: BinanceWebSocketClient + ReconnectPolicy)
    │  raw message queue (BoundedQueue<string>)
    ▼
Parser thread (BinanceTradeParser)
    │  routes each trade to a shard by hash(symbol) % aggregator_threads
    ▼
Aggregation thread(s) (MarketDataAggregator) — 1 or more, see sharding below
    │  writer merges all shards' completed windows
    ▼
Writer thread (StatsSerializer + OutputSink), woken by a local steady-clock timer
    │
    ▼
Output file
```

A rendered version of this diagram is available via the `docs` target (see
[Documentation Generation](#documentation-generation)).

**Thread ownership** — two classes own all worker threads:

- **`NetworkStage`** owns the network thread: loops `BinanceWebSocketClient::run()`,
  retrying via `ReconnectPolicy` on failure, until `stop()`.
- **`MarketDataPipeline`** owns the parser thread, one aggregation thread per
  shard, and the writer thread.

**Clock separation** — the service deliberately uses two independent clocks:

- **Exchange timestamp** (the `T` field on each trade) decides which window a
  trade belongs to: `window_start_ms = trade_time_ms - trade_time_ms % window_ms`.
- **Local/wall-clock time** decides when the writer wakes up to flush
  (`flush_interval_ms`), via a steady-clock-based wait — unaffected by system
  clock adjustments.

  These are independent knobs. `window_ms=1000` with `flush_interval_ms=5000`
  aggregates 1-second exchange-time windows but only writes every 5 real
  seconds, bundling several completed windows into one flush. The `timestamp=`
  field always reflects the exchange-time window start, never the write time.

**Avoiding split windows** — the writer's periodic tick withholds whatever
window is still "current" by wall clock (`MarketDataAggregator::extract_completed_windows`),
only flushing windows strictly older than the one presently in progress. This
means a window is never written while it could still receive more trades under
normal message delay, so a `timestamp=` block is not duplicated in ordinary
operation. The one exception is the final flush on shutdown, which is
unconditional (drains everything, since no further trades can arrive after
that point).

**Failure handling**:

| Failure | Handling |
|---|---|
| WebSocket connection failure | Classified into DNS / TCP connect / TLS handshake / WebSocket handshake / read / timeout / server disconnect (`ConnectionErrorKind`); logged. |
| Reconnect | Exponential backoff (`initial_backoff_ms` doubling up to `max_backoff_ms`, randomized by `jitter_ratio`). Resets once a connection successfully reads at least one message, so a later drop of a healthy connection doesn't inherit an escalated delay. |
| Malformed market message | `BinanceTradeParser` returns "no result" — silently dropped, never crashes the pipeline. |
| Shutdown (`SIGINT`/`SIGTERM`) | Signal handler only sets an atomic flag (signal-safe). Main thread then stops the network stage first (closing the connection, halting reconnect), then the pipeline (drains everything queued, final flush), then exits. Nothing submitted before shutdown is lost. |

## Multi-Threaded Aggregation (Sharding)

Aggregation can be split across multiple independent threads via
`"aggregator_threads"` in `config.json` (default `1`, i.e. single-threaded).

```text
symbol ──hash(symbol) % aggregator_threads──▶ shard N
```

Each shard is a fully independent `(BoundedQueue<TradeEvent>, mutex,
MarketDataAggregator)` triple with its own thread. **A given symbol always
routes to the same shard for the life of the process** — there is no
cross-shard ordering or reconciliation logic, because aggregation within one
`(symbol, window)` bucket is commutative and a symbol never splits across
shards. The writer thread merges completed windows from every shard, sorts
them by symbol, and serializes — so **output is byte-for-byte identical
regardless of `aggregator_threads`**; sharding only changes throughput, never
behavior (`MarketDataPipelineIntegrationTest.ShardedAggregationProducesIdenticalOutputToSingleShard`).

When to increase it: if you subscribe to many symbols and a single aggregation
thread becomes a bottleneck (rare — aggregation work per trade is trivial: a
hash-map lookup plus a few `Decimal` operations). For a couple of symbols at
typical trade volume, `1` is fine.

Verify how many threads are actually running:

```bash
ps -T -p $(pgrep -f binance_aggregator)
# or
cat /proc/$(pgrep -f binance_aggregator)/status | grep Threads
```

Expected thread count = `3 + aggregator_threads` (network + parser + writer +
N aggregation threads), plus the idle main thread.

## Output Format

```text
timestamp=2026-01-12T14:23:20Z
symbol=BTCUSDT trades=154 volume=23.51 min=43012.1 max=43189.4 buy=82 sell=72
symbol=ETHUSDT trades=231 volume=112.7 min=2289.2 max=2301.8 buy=120 sell=111

```

- `timestamp` is the exchange-time window start (UTC, ISO-8601).
- Symbols are sorted alphabetically within a block.
- Symbols with no trades in a window are skipped entirely — an empty window
  is never written.
- Each block ends with a **blank line**, separating it from the next one.
- `volume` is notional value (`sum(price * quantity)`), not raw token quantity.

To watch it live with colors, without modifying the file itself:

```bash
./scripts/watch_stats.sh                 # follow market_stats.log
./scripts/watch_stats.sh path/to/file    # follow a different file
./scripts/watch_stats.sh -c path/to/file # print once, no follow
```

`timestamp=` lines are bold cyan, `symbol=NAME` is yellow, `buy=N` is green,
`sell=N` is red. It uses `sed -u` (explicitly unbuffered) rather than `awk`,
since some `awk` implementations (e.g. `mawk`) buffer internally in a way that
silently stalls live output in a `tail -f | awk` pipeline.

## Logging

Console logs are colorized by level (spdlog `stdout_color_mt`): green `info`,
yellow `warning`, red `error`. Color is applied automatically only when stdout
is a real terminal — redirected or piped output (e.g. `> app.log`) stays plain
text, so log files are never polluted with ANSI escape codes.

## Build, Test, Run

Required host tools: CMake, Conan 2.x, a C++20 compiler, Git, Python/pipx.

```bash
# Configure (bootstraps Conan dependencies automatically)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j

# Run unit + integration tests
ctest --test-dir build --output-on-failure

# Install locally
cmake --install build --prefix dist/local

# Run
./build/binance_aggregator --config config/config.json
# or the installed copy:
./dist/local/bin/binance_aggregator --config config/config.json
```

Binary locations: `build/binance_aggregator` (host build), `dist/local/bin/binance_aggregator`
(installed), `dist/docker/binance_aggregator-Release.image.tgz` (Docker distributable).

## Docker

Four-stage `Dockerfile`:

```text
base       Ubuntu 24.04: build tools, Doxygen/Graphviz, Conan
deps       Copies conanfile.txt, runs `conan install` (cached, layer-separated
           from source so dependency downloads survive source-only changes)
build      Copies full source, builds, runs unit+integration tests, generates
           and verifies Doxygen/Graphviz docs, installs to /install
runtime    Lean Ubuntu 24.04 image, non-root user, `binance_aggregator` ENTRYPOINT
           (docs are not copied into this image)
```

```bash
# Build the image (runs the full test suite + doc generation/verification)
docker build --build-arg BUILD_TYPE=Release -t binance_aggregator:Release .

# Run it
docker run --rm binance_aggregator:Release
```

Docker commands are also exposed as CMake targets (via `cmake/docker.cmake` in
script mode):

```bash
cmake -S . -B build-docker -DCMAKE_BUILD_TYPE=Release
cmake --build build-docker --target docker           # docker build only
cmake --build build-docker --target release_docker    # + export dist/docker/*.tgz
```

## Systemd Deployment

`service/binance-aggregator.service` follows this layout:

```text
/opt/binance-aggregator/bin/binance_aggregator   installed binary (matches Docker's layout)
/etc/binance-aggregator/config.json               host-editable config
/var/lib/binance-aggregator/                      output/state, managed by systemd's StateDirectory=
```

```bash
cmake --install build --prefix /opt/binance-aggregator
# place a config at /etc/binance-aggregator/config.json, create the
# binance-aggregator system user/group, then:
sudo cp service/binance-aggregator.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now binance-aggregator
```

`Restart=on-failure` is a coarse safety net on top of the app's own reconnect
logic. Standard sandboxing is applied (`NoNewPrivileges`, `ProtectSystem=strict`,
`ProtectHome`, `PrivateTmp`). `systemctl stop` sends `SIGTERM`, triggering the
same graceful shutdown described above.

## Documentation Generation

Doxygen (API reference) and Graphviz (the two hand-written `docs/*.gv`
architecture diagrams) are optional locally — a normal build/test cycle needs
neither installed.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_DOCS=ON
cmake --build build --target docs   # or: doxygen_docs / graphviz_diagrams individually
```

Generated (not committed) output:

```text
build/documentation/doxygen/html/index.html
build/documentation/graphviz/runtime_pipeline.svg
build/documentation/graphviz/object_dependencies.svg
```

View them by opening those paths directly in a browser:

```bash
xdg-open build/documentation/doxygen/html/index.html
xdg-open build/documentation/graphviz/runtime_pipeline.svg
xdg-open build/documentation/graphviz/object_dependencies.svg
```

### Without Doxygen/Graphviz installed locally

The Docker `build` stage always has both tools installed and always generates
+ verifies these three files, so a plain `docker build` doubles as a
documentation-generation check. If you don't want to install Doxygen/Graphviz
on the host, extract the generated docs from that stage instead:

```bash
docker build --target build --build-arg BUILD_TYPE=Release -t binance_aggregator:build-stage .

docker create --name docs_extract binance_aggregator:build-stage
docker cp docs_extract:/src/build/documentation ./build/documentation
docker rm -f docs_extract
docker rmi binance_aggregator:build-stage

xdg-open build/documentation/doxygen/html/index.html
```

This places the files at the exact same paths a local `-DBUILD_DOCS=ON` build
would have used (`build/` is already git-ignored).

## Dependencies

Managed by Conan 2.x:

| Package | Used for |
|---|---|
| `boost/1.85.0` | Boost.Asio/Beast — the WebSocket client's TLS + WebSocket handshakes and read loop |
| `openssl/3.3.1` | TLS for `wss://stream.binance.com:9443` |
| `nlohmann_json/3.11.3` | Parsing `config.json` and Binance trade messages |
| `fmt/10.2.1` | String formatting (also used internally by spdlog) |
| `spdlog/1.14.1` | Service logging, with color support |
| `gtest/1.14.0` | Unit and integration tests |

## Testing Strategy

Unit tests (`tests/unit/`, one file per component) cover decimal arithmetic,
trade parsing, window aggregation, statistics serialization, bounded queues,
config validation, connection-failure classification, and reconnect backoff.

Integration tests (`tests/integration/`) cover the full offline pipeline
(fixture messages → exact expected output), sharded-vs-single-shard output
equivalence, the duplicate-timestamp fix, and network-stage retry/stop
behavior — using loopback connections that fail immediately, never live
Binance. A live connection is manually runnable but never required by the
automated suite.

```bash
ctest --test-dir build --output-on-failure
```

All components described above are implemented, tested, and wired together in
`main.cpp`, and have been manually verified against live Binance: the service
connects, aggregates real trade data, writes correctly formatted output, and
shuts down cleanly on `SIGTERM`.
