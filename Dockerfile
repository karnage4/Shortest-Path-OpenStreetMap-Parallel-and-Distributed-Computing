# Stage 1 - Builder
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        libomp-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt .
COPY src/ src/

RUN cmake -B build \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG" && \
    cmake --build build --parallel $(nproc)

# Stage 2 - Runtime
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        libgomp1 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /src/build/osm_routing  ./bin/osm_routing
COPY --from=builder /src/build/osm_bench    ./bin/osm_bench
COPY --from=builder /src/build/osm_parallel ./bin/osm_parallel
COPY --from=builder /src/build/osm_m3       ./bin/osm_m3

ENV PATH="/app/bin:$PATH"

VOLUME ["/app/data"]

CMD ["/bin/bash"]