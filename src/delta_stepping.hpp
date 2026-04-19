#pragma once
#include "graph.hpp"
#include <vector>
#include <atomic>
#include <mutex>

struct DeltaSteppingResult {
    std::vector<uint32_t> dist;
    std::vector<uint32_t> prev;
    double wall_ms = 0.0;
};

DeltaSteppingResult delta_stepping(
    const Graph& g, uint32_t source,
    uint32_t delta, uint32_t num_threads);

std::vector<uint32_t> delta_reconstruct_path(
    const DeltaSteppingResult& res,
    uint32_t source, uint32_t target);
