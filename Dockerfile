# syntax=docker/dockerfile:1.7

ARG BUILD_TYPE=Release

FROM ubuntu:24.04 AS build

ARG BUILD_TYPE

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="/root/.local/bin:${PATH}"

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

RUN ctest --test-dir build --output-on-failure

RUN cmake --install build --prefix /install


FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd \
    --system \
    --create-home \
    --home-dir /var/lib/binance-aggregator \
    --shell /usr/sbin/nologin \
    binance

COPY --from=build /install /opt/binance-aggregator
COPY config /etc/binance-aggregator

RUN mkdir -p /var/lib/binance-aggregator && \
    chown -R binance:binance \
        /var/lib/binance-aggregator \
        /etc/binance-aggregator \
        /opt/binance-aggregator

ENV PATH="/opt/binance-aggregator/bin:${PATH}"

USER binance

WORKDIR /var/lib/binance-aggregator

ENTRYPOINT ["binance_aggregator"]
CMD ["--config", "/etc/binance-aggregator/config.json"]