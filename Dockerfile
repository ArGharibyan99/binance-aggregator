# syntax=docker/dockerfile:1.7
ARG BUILD_TYPE=Release

FROM ubuntu:24.04 AS base
ARG BUILD_TYPE
ENV DEBIAN_FRONTEND=noninteractive

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git \
        ca-certificates pkg-config \
        doxygen graphviz fontconfig fonts-dejavu-core fonts-liberation \
        python3-pip python3-venv && \
    rm -rf /var/lib/apt/lists/*

RUN --mount=type=cache,target=/root/.cache/pip,sharing=locked \
    PIP_BREAK_SYSTEM_PACKAGES=1 pip3 install "conan==2.3.2"

FROM base AS deps
ARG BUILD_TYPE
WORKDIR /src

COPY conanfile.txt ./

RUN --mount=type=cache,target=/root/.conan2,sharing=locked \
    conan profile detect --name default --force && \
    conan install . -of build/conan --build=missing \
          -s build_type=${BUILD_TYPE} \
          -s compiler.cppstd=20

FROM deps AS build
ARG BUILD_TYPE
WORKDIR /src

COPY . .

RUN --mount=type=cache,target=/root/.conan2,sharing=locked \
    cmake -S . -B build -GNinja \
          -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
          -DBUILD_TESTING=ON \
          -DBUILD_DOCS=ON && \
    cmake --build build -j$(nproc) && \
    ctest --test-dir build --output-on-failure && \
    cmake --build build --target docs && \
    test -f build/documentation/doxygen/html/index.html && \
    test -f build/documentation/graphviz/runtime_pipeline.svg && \
    test -f build/documentation/graphviz/object_dependencies.svg && \
    cmake --install build --prefix /install

FROM ubuntu:24.04 AS runtime
RUN useradd --create-home --shell /bin/bash aggregator

# This install tree was built inside the Docker build stage above. No local
# build output from the host is copied into the runtime image.
COPY --from=build /install /opt/binance-aggregator
ENV PATH="/opt/binance-aggregator/bin:${PATH}"

USER aggregator
WORKDIR /home/aggregator

ENTRYPOINT ["binance_aggregator"]

LABEL org.opencontainers.image.title="Binance Aggregator"
LABEL org.opencontainers.image.description="C++20 service that aggregates Binance public trade data"
