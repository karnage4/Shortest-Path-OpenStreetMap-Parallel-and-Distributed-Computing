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

### Sequential Single Query
To find the path between two specific OSM node IDs:
```bash
./osm_routing ../data/monaco.osm <source_id> <target_id>
```

### Parallel Performance Comparison
Compare Sequential vs. Delta-Stepping vs. Bidirectional Dijkstra:
```bash
# Usage: ./osm_parallel <osm_file> [threads] [delta_m] [queries]
./osm_parallel ../data/monaco.osm 8 150 50
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
