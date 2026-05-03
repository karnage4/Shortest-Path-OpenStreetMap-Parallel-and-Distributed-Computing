# Shortest Path on OpenStreetMap (Parallel & Distributed Computing)

This project implements and compares various shortest-path algorithms on real-world OpenStreetMap (OSM) data. It includes a high-performance sequential Dijkstra implementation and multiple parallel approaches using OpenMP, such as Delta-Stepping and Bidirectional Dijkstra.

## Features
- **Custom Streaming OSM Parser**: Efficiently parses large XML files without loading the entire DOM into memory.
- **Sequential Dijkstra**: Baseline implementation for correctness and performance comparison.
- **Parallel Algorithms**:
  - **Delta-Stepping**: A parallel shortest-path algorithm that balances work across threads using buckets.
  - **Parallel Dijkstra**: Shared-priority-queue approach with lock contention tracking.
  - **Bidirectional Dijkstra**: Concurrent search from both source and target.
- **Performance Benchmarking**: Automated testing across different thread counts and query sizes.

---

## 1. Prerequisites

Before building, ensure you have the following installed:
- **CMake** (v3.18+)
- **C++ Compiler** (C++17 support required, e.g., GCC 9+, Clang 10+, or MSVC 2019+)
- **OpenMP** (usually included with GCC/Clang)

---

## 2. Building the Project

The project uses CMake for cross-platform builds.

### Windows (MinGW/GCC)
```pwsh
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

### Linux / macOS
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

After building, the following executables will be available in the `build` directory:
- `osm_routing`: Main entry point for single queries and basic benchmarks.
- `osm_parallel`: Benchmark suite for comparing parallel algorithms.
- `osm_bench`: Specialized benchmarking mode for sequential Dijkstra.

---

## 3. Getting OSM Data

This project works with `.osm` (XML) files. You can obtain data for any region from the following sources:

1.  **[Geofabrik Extracts](https://download.geofabrik.de/)** (Recommended)
    - Download country or region extracts (e.g., `monaco-latest.osm.bz2`).
    - Note: You must decompress the `.bz2` or `.pbf` files to raw `.osm` XML format first.
2.  **[BBBike Extracts](https://extract.bbbike.org/)**
    - Excellent for getting specific city extracts in XML format.
3.  **[OpenStreetMap Export](https://www.openstreetmap.org/export)**
    - Best for very small areas (bounding boxes).

### Usage with Data
Place your `.osm` files in a `data/` directory. For example:
```bash
./osm_routing ../data/monaco.osm
```

---

## 4. Running Benchmarks

To run these commands, you need to be in the `build/Release/` directory.

### Sequential Single Query
To find the path between two specific OSM node IDs:
```bash
./osm_routing ../../data/monaco.osm <source_id> <target_id>
```

### Parallel Performance Comparison
Compare Sequential vs. Delta-Stepping vs. Bidirectional Dijkstra:
```bash
# Usage: ./osm_parallel <osm_file> [threads] [delta_m] [queries]
./osm_parallel ../../data/monaco.osm 8 150 50
```
- `threads`: Number of OpenMP threads (default: 4).
- `delta_m`: The $\Delta$ parameter for Delta-Stepping in meters (default: 150).
- `queries`: Number of random source-target pairs to test (default: 20).

---

## 5. Project Structure
- `src/`: Source code including algorithm implementations.
- `data/`: Directory for storing `.osm` files.
- `CMakeLists.txt`: Build configuration.
- `PDC_ProjectOSM_M1.pdf`: Project documentation and milestone details.
- `PDC_ProjectOSM_M1.pdf`: Project documentation and milestone details.

## OSM Datasets

| File | Size | Milestone | Description |
|------|------|-----------|-------------|
| `monaco.osm` | ~22 MB | Milestone 1 & 2 | Small dataset for initial testing and benchmarking of algorithms. |
| `brussels.osm` | ~1.37 GB | Milestone 3 | Large‑scale dataset used for performance scaling and parallel benchmarking. |

## Running Executables

### osm_routing (Sequential / Single Query)

```
./osm_routing <osm_file> <source_id> <target_id>
```
- `<osm_file>`: Path to the `.osm` dataset (e.g., `../data/monaco.osm`).
- `<source_id>` and `<target_id>`: OSM node IDs defining the start and end points of the route.

### osm_parallel (Parallel Benchmark)

```
./osm_parallel <osm_file> [threads] [delta_m] [queries]
```
- `<osm_file>`: Path to the dataset.
- `threads` (optional): Number of OpenMP threads (default = 4).
- `delta_m` (optional): Δ‑parameter for the Delta‑Stepping algorithm in meters (default = 150).
- `queries` (optional): Number of random source‑target pairs to benchmark (default = 20).

### osm_bench (Sequential Benchmark)

```
./osm_bench <osm_file> [queries]
```
- `<osm_file>`: Path to the dataset.
- `queries` (optional): Number of random queries (default = 20).

### osm_m3 (Landmark-based)

```
./osm_m3 <osm_file> [num_queries] [num_landmarks]
```
- `<osm_file>`: Path to the dataset.
- `num_queries` (optional): Number of random queries (default = 1000).
- `num_landmarks` (optional): Number of landmarks to use (default = 16).

## 🐳 Docker

A pre-built Docker image is available on Docker Hub — no need to compile from source.

### Pull the Image

```bash
docker pull ammarkhan05/shortest-path-osm:latest
```

### Run

```bash
# interactive terminal mode
docker run --rm -it -v "$PWD/data:/app/data"
```

These commands run directly on your local machine after pulling the image.

```bash
# Sequential single query
docker run --rm -it -v "$PWD/data:/app/data" osm-pdc:latest \
    osm_routing /app/data/monaco.osm <source_id> <target_id>

# Parallel benchmark (8 threads, delta=150, 50 queries)
docker run --rm -it -v "$PWD/data:/app/data" osm-pdc:latest \
    osm_parallel /app/data/monaco.osm 8 150 50

# Sequential benchmark
docker run --rm -it -v "$PWD/data:/app/data" osm-pdc:latest \
    osm_bench /app/data/monaco.osm 50

# Milestone-3 batch
docker run --rm -it -v "$PWD/data:/app/data" osm-pdc:latest \
    osm_m3 /app/data/monaco.osm
```

### Available Tags

| Tag | Description |
|-----|-------------|
| `latest` | Most recent stable build |