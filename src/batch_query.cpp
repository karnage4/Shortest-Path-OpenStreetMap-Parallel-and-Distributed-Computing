#include "batch_query.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>

// ---------------------------------------------------------------------------
// BatchStats printing
// ---------------------------------------------------------------------------
void BatchStats::print(const std::string &label) const
{
    std::cout << "\n[" << label << "]\n"
              << "  Queries:      " << total_queries
              << " (" << reachable << " reachable, "
              << unreachable << " unreachable)\n"
              << "  Throughput:   " << std::fixed << std::setprecision(1)
              << throughput_qps << " queries/sec\n"
              << "  Latency avg:  " << std::setprecision(3)
              << avg_latency_ms << " ms\n"
              << "  Latency min:  " << min_latency_ms << " ms\n"
              << "  Latency p95:  " << p95_latency_ms << " ms\n"
              << "  Latency max:  " << max_latency_ms << " ms\n"
              << "  Total time:   " << total_ms << " ms\n";
}

void BatchStats::save_csv_row(std::ofstream &f,
                              const std::string &label,
                              uint32_t threads) const
{
    f << label << ","
      << threads << ","
      << total_queries << ","
      << reachable << ","
      << std::fixed << std::setprecision(3)
      << throughput_qps << ","
      << avg_latency_ms << ","
      << min_latency_ms << ","
      << p95_latency_ms << ","
      << max_latency_ms << ","
      << total_ms << "\n";
}

// ---------------------------------------------------------------------------
// Generate random source-target pairs
// ---------------------------------------------------------------------------
std::vector<Query> generate_queries(const Graph &g,
                                    uint32_t count,
                                    uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<uint32_t> dist(0, g.num_nodes() - 1);
    std::vector<Query> queries(count);
    for (auto &q : queries)
    {
        q.source = dist(rng);
        q.target = dist(rng);
        // Ensure source != target
        while (q.target == q.source)
            q.target = dist(rng);
    }
    return queries;
}

// ---------------------------------------------------------------------------
// Core batch processor
// ---------------------------------------------------------------------------
std::pair<std::vector<QueryResult>, BatchStats>
BatchQueryProcessor::process(const std::vector<Query> &queries,
                             uint32_t num_threads)
{
    const uint32_t N = static_cast<uint32_t>(queries.size());
    std::vector<QueryResult> results(N);

    // Shared work queue - only one mutex for the entire batch
    // Lock held for nanoseconds (just pop index), work done lock-free
    std::atomic<uint32_t> next_query{0}; // atomic counter - no mutex needed!

    auto t_batch_start = std::chrono::steady_clock::now();

    // Worker lambda - each thread loops pulling queries atomically
    auto worker = [&]()
    {
        while (true)
        {
            // Atomically grab the next query index - lock-free!
            uint32_t idx = next_query.fetch_add(1, std::memory_order_relaxed);
            if (idx >= N)
                break; // no more work

            const Query &q = queries[idx];

            // Each thread runs its own Dijkstra - no shared mutable state
            // graph_ is read-only → zero synchronization on graph access
            auto t_start = std::chrono::steady_clock::now();
            auto res = dijkstra(graph_, q.source);
            auto t_end = std::chrono::steady_clock::now();

            double latency = std::chrono::duration<double, std::milli>(
                                 t_end - t_start)
                                 .count();

            // Reconstruct path length
            uint32_t path_len = 0;
            if (res.dist[q.target] != INF_DIST)
            {
                uint32_t v = q.target;
                while (v != NO_NODE)
                {
                    ++path_len;
                    v = res.prev[v];
                }
            }

            results[idx] = QueryResult{
                q.source,
                q.target,
                res.dist[q.target],
                path_len,
                latency};
        }
    };

    // Launch thread pool
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (uint32_t t = 0; t < num_threads; ++t)
        threads.emplace_back(worker);
    for (auto &t : threads)
        t.join();

    auto t_batch_end = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(
                          t_batch_end - t_batch_start)
                          .count();

    // Compute statistics
    BatchStats stats;
    stats.total_queries = N;
    stats.total_ms = total_ms;
    stats.throughput_qps = 1000.0 * N / total_ms;
    stats.reachable = 0;
    stats.unreachable = 0;

    std::vector<double> latencies(N);
    for (uint32_t i = 0; i < N; ++i)
    {
        latencies[i] = results[i].latency_ms;
        if (results[i].distance != INF_DIST)
            ++stats.reachable;
        else
            ++stats.unreachable;
    }

    std::sort(latencies.begin(), latencies.end());
    stats.min_latency_ms = latencies.front();
    stats.max_latency_ms = latencies.back();
    stats.avg_latency_ms = std::accumulate(latencies.begin(),
                                           latencies.end(), 0.0) /
                           N;
    stats.p95_latency_ms = latencies[static_cast<size_t>(0.95 * N)];

    return {results, stats};
}

// ---------------------------------------------------------------------------
// Thread scaling experiment
// ---------------------------------------------------------------------------
void run_scaling_experiment(
    const Graph &g,
    const std::vector<Query> &queries,
    const std::string &dataset_label,
    std::ofstream &csv_file)
{
    std::cout << "\n========================================\n";
    std::cout << "  SCALING EXPERIMENT: " << dataset_label << "\n";
    std::cout << "  Graph: " << g.num_nodes() << " nodes, "
              << g.num_edges() << " edges\n";
    std::cout << "  Queries: " << queries.size() << "\n";
    std::cout << "========================================\n";

    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(16) << "Throughput(q/s)"
              << std::setw(14) << "Avg(ms)"
              << std::setw(14) << "P95(ms)"
              << std::setw(14) << "Max(ms)"
              << std::setw(12) << "Speedup"
              << "\n"
              << std::string(80, '-') << "\n";

    BatchQueryProcessor processor(g);
    double baseline_throughput = 0.0;

    for (uint32_t t : {1u, 2u, 4u, 8u, 16u})
    {
        // Skip if more threads than queries
        if (t > static_cast<uint32_t>(queries.size()))
            continue;

        auto [results, stats] = processor.process(queries, t);

        if (t == 1)
            baseline_throughput = stats.throughput_qps;
        double speedup = stats.throughput_qps / baseline_throughput;

        std::cout << std::left
                  << std::setw(10) << t
                  << std::setw(16) << std::fixed << std::setprecision(1)
                  << stats.throughput_qps
                  << std::setw(14) << std::setprecision(3)
                  << stats.avg_latency_ms
                  << std::setw(14) << stats.p95_latency_ms
                  << std::setw(14) << stats.max_latency_ms
                  << std::setw(12) << std::setprecision(2) << speedup
                  << "\n";

        // Save to CSV
        stats.save_csv_row(csv_file, dataset_label, t);
    }
    std::cout << std::string(80, '-') << "\n";
}