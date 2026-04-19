#pragma once
#include "graph.hpp"
#include <vector>
#include <atomic>
#include <cstdint>

struct ParallelDijkstraResult {
    std::vector<uint32_t> dist;
    std::vector<uint32_t> prev;
    double wall_ms = 0.0;
    uint64_t lock_contentions = 0;
};

ParallelDijkstraResult parallel_dijkstra(
    const Graph& g, uint32_t source, uint32_t num_threads);
