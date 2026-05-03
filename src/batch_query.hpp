#pragma once
#include "graph.hpp"
#include "dijkstra.hpp"
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <chrono>
#include <fstream>
#include <string>
#include <algorithm>
#include <numeric>

// ---------------------------------------------------------------------------
// A single routing query: source → target
// ---------------------------------------------------------------------------
struct Query
{
    uint32_t source;
    uint32_t target;
};

// ---------------------------------------------------------------------------
// Result of a single query
// ---------------------------------------------------------------------------
struct QueryResult
{
    uint32_t source;
    uint32_t target;
    uint32_t distance;   // INF_DIST if unreachable
    uint32_t path_nodes; // number of nodes in path
    double latency_ms;   // wall-clock time for this query
};

// ---------------------------------------------------------------------------
// Aggregate statistics over a batch
// ---------------------------------------------------------------------------
struct BatchStats
{
    uint32_t total_queries;
    uint32_t reachable;
    uint32_t unreachable;
    double total_ms;
    double avg_latency_ms;
    double min_latency_ms;
    double max_latency_ms;
    double p95_latency_ms; // 95th percentile
    double throughput_qps; // queries per second

    void print(const std::string &label) const;
    void save_csv_row(std::ofstream &f, const std::string &label,
                      uint32_t threads) const;
};

// ---------------------------------------------------------------------------
// BatchQueryProcessor
//
// DESIGN:
//   - Graph is shared read-only across ALL threads - zero graph locks
//   - Thread pool processes queries from a shared work queue
//   - Only the work queue needs a mutex (one lock per query dispatch)
//   - Each thread owns its own DijkstraResult (dist[], prev[]) - no sharing
//
// This achieves near-linear throughput scaling because:
//   - Critical section = popping one query from queue (~nanoseconds)
//   - Work = full Dijkstra (~milliseconds)
//   - Lock contention is negligible vs work done
// ---------------------------------------------------------------------------
class BatchQueryProcessor
{
public:
    explicit BatchQueryProcessor(const Graph &g) : graph_(g) {}

    // Process all queries using num_threads worker threads
    // Returns per-query results and aggregate stats
    std::pair<std::vector<QueryResult>, BatchStats>
    process(const std::vector<Query> &queries, uint32_t num_threads);

private:
    const Graph &graph_; // read-only - no locks needed
};

// ---------------------------------------------------------------------------
// Generate random query pairs
// ---------------------------------------------------------------------------
std::vector<Query> generate_queries(const Graph &g,
                                    uint32_t count,
                                    uint32_t seed = 42);

// ---------------------------------------------------------------------------
// Throughput scaling experiment
// Runs the same query set with 1,2,4,8,16 threads and records results
// ---------------------------------------------------------------------------
void run_scaling_experiment(
    const Graph &g,
    const std::vector<Query> &queries,
    const std::string &dataset_label,
    std::ofstream &csv_file);