#pragma once
#include "graph.hpp"
#include <vector>
#include <utility>

// ---------------------------------------------------------------------------
// Result of a single-source shortest-path query.
// ---------------------------------------------------------------------------
struct DijkstraResult {
    std::vector<uint32_t> dist;  // dist[v] = shortest distance from source
    std::vector<uint32_t> prev;  // prev[v] = predecessor on shortest path
};

// ---------------------------------------------------------------------------
// Sequential Dijkstra using std::priority_queue (binary heap).
//
// Heap entry: (distance, node). Min-heap on distance.
// Lazy deletion: stale entries are skipped when popped (dist[u] < d check).
// This avoids implementing a decrease-key operation and is standard practice
// for road networks where edges >> nodes.
// ---------------------------------------------------------------------------
DijkstraResult dijkstra(const Graph& g, uint32_t source);

// Reconstruct path from source to target using prev[] array.
// Returns empty vector if no path exists.
std::vector<uint32_t> reconstruct_path(const DijkstraResult& result,
                                        uint32_t source, uint32_t target);