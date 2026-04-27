#include "landmark_preprocess.hpp"
#include "batch_query.hpp"
#include "dijkstra.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <queue>
#include <functional>
#include <algorithm>
#include <numeric>

// ---------------------------------------------------------------------------
// Farthest-point landmark selection
// Guarantees landmarks are well-spread across the graph.
// ---------------------------------------------------------------------------
void LandmarkIndex::select_landmarks(const Graph &g, uint32_t K)
{
    const uint32_t N = g.num_nodes();
    landmarks_.clear();
    landmarks_.reserve(K);

    // Start from a random node
    std::mt19937 rng(12345);
    uint32_t first = std::uniform_int_distribution<uint32_t>(0, N - 1)(rng);
    landmarks_.push_back(first);

    // Iteratively pick the farthest node from the current set
    std::vector<uint32_t> min_dist(N, INF_DIST);

    for (uint32_t k = 0; k < K; ++k)
    {
        uint32_t last = landmarks_.back();
        auto res = dijkstra(g, last);

        // Update min distance to any landmark
        for (uint32_t v = 0; v < N; ++v)
            min_dist[v] = std::min(min_dist[v], res.dist[v]);

        if (k + 1 < K)
        {
            // Pick node with maximum min-distance to landmarks
            uint32_t farthest = 0;
            uint32_t farthest_dist = 0;
            for (uint32_t v = 0; v < N; ++v)
            {
                if (min_dist[v] != INF_DIST && min_dist[v] > farthest_dist)
                {
                    farthest_dist = min_dist[v];
                    farthest = v;
                }
            }
            landmarks_.push_back(farthest);
        }
    }

    std::cout << "[Landmarks] Selected " << K << " landmarks "
              << "(farthest-point sampling)\n";
}

// ---------------------------------------------------------------------------
// Build: run K*2 Dijkstras in parallel
// ---------------------------------------------------------------------------
void LandmarkIndex::build(const Graph &g, const Graph &g_rev,
                          uint32_t K, uint32_t num_threads)
{
    auto t_start = std::chrono::steady_clock::now();
    const uint32_t N = g.num_nodes();

    std::cout << "[Landmarks] Building index: K=" << K
              << " landmarks, " << num_threads << " threads\n";

    // Step 1: select landmarks
    select_landmarks(g, K);

    // Step 2: parallel Dijkstra from/to each landmark
    dist_from_.assign(K, std::vector<uint32_t>(N, INF_DIST));
    dist_to_.assign(K, std::vector<uint32_t>(N, INF_DIST));

    // Use atomic index for work stealing
    std::atomic<uint32_t> next_landmark{0};

    auto worker = [&]()
    {
        while (true)
        {
            uint32_t k = next_landmark.fetch_add(1, std::memory_order_relaxed);
            if (k >= K)
                break;

            uint32_t lm = landmarks_[k];

            // Forward: dist FROM landmark k to all nodes
            auto res_fwd = dijkstra(g, lm);
            dist_from_[k] = res_fwd.dist;

            // Backward: dist TO landmark k from all nodes
            // = Dijkstra from landmark on REVERSE graph
            auto res_bwd = dijkstra(g_rev, lm);
            dist_to_[k] = res_bwd.dist;
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t t = 0; t < num_threads; ++t)
        threads.emplace_back(worker);
    for (auto &t : threads)
        t.join();

    auto t_end = std::chrono::steady_clock::now();
    build_time_ms_ = std::chrono::duration<double, std::milli>(
                         t_end - t_start)
                         .count();

    std::cout << "[Landmarks] Index built in "
              << std::fixed << std::setprecision(1)
              << build_time_ms_ << " ms\n";
}

// ---------------------------------------------------------------------------
// Lower bound heuristic: max over all landmarks of triangle inequality
// ---------------------------------------------------------------------------
uint32_t LandmarkIndex::lower_bound(uint32_t v, uint32_t target) const
{
    uint32_t best = 0;
    const uint32_t K = static_cast<uint32_t>(landmarks_.size());

    for (uint32_t k = 0; k < K; ++k)
    {
        uint32_t dft = dist_from_[k][target];
        uint32_t dfv = dist_from_[k][v];
        uint32_t dts = dist_to_[k][v]; // dist from v to landmark
        uint32_t dtg = dist_to_[k][target];

        // Triangle inequality bounds:
        // dist(v, t) >= dist(lm, t) - dist(lm, v)
        if (dft != INF_DIST && dfv != INF_DIST && dft > dfv)
            best = std::max(best, dft - dfv);

        // dist(v, t) >= dist(v, lm) - dist(t, lm)
        if (dts != INF_DIST && dtg != INF_DIST && dts > dtg)
            best = std::max(best, dts - dtg);
    }
    return best;
}

// ---------------------------------------------------------------------------
// A* query using landmark heuristic
// ---------------------------------------------------------------------------
std::pair<uint32_t, std::vector<uint32_t>>
LandmarkIndex::astar_query(const Graph &g,
                           uint32_t source,
                           uint32_t target) const
{
    const uint32_t N = g.num_nodes();
    std::vector<uint32_t> dist(N, INF_DIST);
    std::vector<uint32_t> prev(N, NO_NODE);
    dist[source] = 0;

    // f(v) = g(v) + h(v) where h is landmark lower bound
    using Entry = std::pair<uint32_t, uint32_t>; // (f, node)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    pq.emplace(lower_bound(source, target), source);

    while (!pq.empty())
    {
        auto [f, u] = pq.top();
        pq.pop();

        if (u == target)
            break; // found shortest path

        if (dist[u] == INF_DIST)
            continue;
        // Lazy deletion check
        uint32_t h_u = lower_bound(u, target);
        if (f > dist[u] + h_u + 1)
            continue; // stale entry

        for (const Edge *e = g.edge_begin(u); e != g.edge_end(u); ++e)
        {
            uint32_t new_g = dist[u] + e->weight;
            if (new_g < dist[e->to])
            {
                dist[e->to] = new_g;
                prev[e->to] = u;
                uint32_t h = lower_bound(e->to, target);
                if (new_g != INF_DIST && h != INF_DIST)
                    pq.emplace(new_g + h, e->to);
            }
        }
    }

    // Reconstruct path
    std::vector<uint32_t> path;
    if (dist[target] != INF_DIST)
    {
        for (uint32_t v = target; v != NO_NODE; v = prev[v])
            path.push_back(v);
        std::reverse(path.begin(), path.end());
    }

    return {dist[target], path};
}

// ---------------------------------------------------------------------------
// Landmark batch processing
// ---------------------------------------------------------------------------
LandmarkBatchStats run_landmark_batch(
    const Graph &g,
    const LandmarkIndex &idx,
    const std::vector<Query> &queries,
    uint32_t num_threads)
{
    const uint32_t N = static_cast<uint32_t>(queries.size());
    std::vector<double> latencies(N);
    std::atomic<uint32_t> next{0};

    auto t_start = std::chrono::steady_clock::now();

    auto worker = [&]()
    {
        while (true)
        {
            uint32_t i = next.fetch_add(1, std::memory_order_relaxed);
            if (i >= N)
                break;
            auto t0 = std::chrono::steady_clock::now();
            idx.astar_query(g, queries[i].source, queries[i].target);
            auto t1 = std::chrono::steady_clock::now();
            latencies[i] = std::chrono::duration<double, std::milli>(
                               t1 - t0)
                               .count();
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t t = 0; t < num_threads; ++t)
        threads.emplace_back(worker);
    for (auto &t : threads)
        t.join();

    auto t_end = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(
                          t_end - t_start)
                          .count();

    double avg = std::accumulate(latencies.begin(),
                                 latencies.end(), 0.0) /
                 N;

    LandmarkBatchStats s;
    s.total_queries = N;
    s.avg_latency_ms = avg;
    s.throughput_qps = 1000.0 * N / total_ms;
    return s;
}