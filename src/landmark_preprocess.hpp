#pragma once
#include "graph.hpp"
#include "batch_query.hpp"
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Landmark Preprocessing — ALT Algorithm
// (A* with Landmarks and Triangle inequality)
//
// HOW IT WORKS:
//   1. Select K landmark nodes (well-spread across the graph)
//   2. Run Dijkstra FROM each landmark → dist_from[k][v] for all v
//   3. Run Dijkstra TO   each landmark → dist_to[k][v]   for all v
//      (= Dijkstra from landmark on REVERSE graph)
//
// QUERY TIME:
//   For query s→t, the lower bound via landmark k is:
//     h(v) = max over all k of:
//       |dist_from[k][t] - dist_from[k][v]|
//       |dist_to[k][s]   - dist_to[k][v]  |
//   This h(v) is admissible → A* with this heuristic is optimal.
//
// BENEFIT:
//   Reduces nodes explored by ~2-10x on road networks.
//   Preprocessing cost: K × 2 × Dijkstra (paid once, amortized over queries).
//
// PDC ANGLE:
//   K landmark Dijkstras run in PARALLEL — perfect task parallelism.
//   Preprocessing time is amortized over many queries.
// ---------------------------------------------------------------------------

class LandmarkIndex
{
public:
    // Build landmark index with K landmarks using num_threads parallel Dijkstras
    void build(const Graph &g, const Graph &g_rev,
               uint32_t K = 16,
               uint32_t num_threads = 4);

    // A* lower bound heuristic from v to target t
    uint32_t lower_bound(uint32_t v, uint32_t target) const;

    // A* shortest path using landmark heuristic
    // Returns distance and path
    std::pair<uint32_t, std::vector<uint32_t>>
    astar_query(const Graph &g, uint32_t source, uint32_t target) const;

    uint32_t num_landmarks() const
    {
        return static_cast<uint32_t>(landmarks_.size());
    }

    double build_time_ms() const { return build_time_ms_; }

private:
    std::vector<uint32_t> landmarks_;              // landmark node IDs
    std::vector<std::vector<uint32_t>> dist_from_; // dist_from[k][v]
    std::vector<std::vector<uint32_t>> dist_to_;   // dist_to[k][v]
    double build_time_ms_ = 0.0;

    // Select landmarks: farthest-point sampling for good spread
    void select_landmarks(const Graph &g, uint32_t K);
};

// ---------------------------------------------------------------------------
// Batch processor using landmark A* instead of plain Dijkstra
// ---------------------------------------------------------------------------
struct LandmarkBatchStats
{
    double avg_latency_ms;
    double throughput_qps;
    double speedup_vs_dijkstra;
    uint32_t total_queries;
};

LandmarkBatchStats run_landmark_batch(
    const Graph &g,
    const LandmarkIndex &idx,
    const std::vector<Query> &queries,
    uint32_t num_threads);