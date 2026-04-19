#pragma once
#include "graph.hpp"
#include <vector>
#include <atomic>

struct BiDijkstraResult {
    std::vector<uint32_t> dist_fwd;
    std::vector<uint32_t> dist_bwd;
    uint32_t best_dist = INF_DIST;
    std::vector<uint32_t> path;
    double wall_ms = 0.0;
};

Graph build_reverse_graph(const Graph& g);

BiDijkstraResult bidirectional_dijkstra(
    const Graph& g, const Graph& g_rev,
    uint32_t source, uint32_t target);
