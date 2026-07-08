# syntax=docker/dockerfile:1.7

ARG BUILD_TYPE=Release

FROM ubuntu:24.04 AS test

ARG BUILD_TYPE

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="/root/.local/bin:${PATH}"
ENV CTEST_OUTPUT_ON_FAILURE=1

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    git \
    pipx \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    && rm -rf /var/lib/apt/lists/*

RUN pipx install conan

WORKDIR /src

COPY . .

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DBUILD_TESTING=ON

RUN cmake --build build -j$(nproc)

RUN ctest \
    --test-dir build \
    --output-on-failure \
    --verbose

RUN cmake --install build --prefix /install


FROM scratch AS artifact

COPY --from=test /install /