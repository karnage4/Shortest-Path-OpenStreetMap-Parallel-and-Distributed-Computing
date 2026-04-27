#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include "graph.hpp"
#include "osm_parser.hpp"
#include "benchmark.hpp"
#include "batch_query.hpp"
#include "landmark_preprocess.hpp"
#include "bidirectional_dijkstra.hpp"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <osm_file> [num_queries] [num_landmarks]\n"
                  << "  num_queries:   default 200\n"
                  << "  num_landmarks: default 16\n";
        return 1;
    }

    uint32_t num_queries = (argc > 2) ? std::stoul(argv[2]) : 200;
    uint32_t num_landmarks = (argc > 3) ? std::stoul(argv[3]) : 16;

    // ── Parse graph ───────────────────────────────────────────────────────
    Graph g;
    OsmParser parser;
    {
        Timer t("OSM Parse");
        parser.parse(argv[1], g, "distance");
    }
    std::cout << "Graph: " << g.num_nodes() << " nodes, "
              << g.num_edges() << " edges\n";

    // Build reverse graph for landmarks + bidirectional
    Graph g_rev;
    {
        Timer t("Reverse Graph");
        g_rev = build_reverse_graph(g);
    }

    // ── Generate queries ──────────────────────────────────────────────────
    auto queries = generate_queries(g, num_queries, 42);
    std::cout << "Generated " << num_queries << " random queries\n";

    // ── Open CSV output ───────────────────────────────────────────────────
    std::string csv_path = "milestone3_results.csv";
    std::ofstream csv(csv_path);
    csv << "dataset,threads,queries,reachable,throughput_qps,"
        << "avg_ms,min_ms,p95_ms,max_ms,total_ms\n";

    std::string label = std::string(argv[1]);
    // Trim path — keep filename only
    auto slash = label.find_last_of("/\\");
    if (slash != std::string::npos)
        label = label.substr(slash + 1);

    // ── Experiment 1: Thread scaling (plain Dijkstra batch) ───────────────
    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║  EXPERIMENT 1: Thread Scaling (Batch Dijkstra)║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";
    run_scaling_experiment(g, queries, label, csv);

    // ── Experiment 2: Landmark preprocessing ─────────────────────────────
    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║  EXPERIMENT 2: Landmark A* Preprocessing      ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    LandmarkIndex landmark_idx;
    {
        Timer t("Landmark Build");
        landmark_idx.build(g, g_rev, num_landmarks, 4);
    }

    std::cout << "\nLandmark A* vs Plain Dijkstra comparison:\n";
    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(20) << "Dijkstra(q/s)"
              << std::setw(20) << "LandmarkA*(q/s)"
              << std::setw(12) << "Speedup"
              << "\n"
              << std::string(62, '-') << "\n";

    BatchQueryProcessor batch_proc(g);
    for (uint32_t t : {1u, 2u, 4u, 8u})
    {
        auto [res_d, stats_d] = batch_proc.process(queries, t);
        auto stats_l = run_landmark_batch(g, landmark_idx, queries, t);

        double speedup = stats_l.throughput_qps / stats_d.throughput_qps;
        std::cout << std::left
                  << std::setw(10) << t
                  << std::setw(20) << std::fixed << std::setprecision(1)
                  << stats_d.throughput_qps
                  << std::setw(20) << stats_l.throughput_qps
                  << std::setw(12) << std::setprecision(2) << speedup
                  << "\n";
    }
    std::cout << std::string(62, '-') << "\n";

    // ── Experiment 3: Latency distribution analysis ───────────────────────
    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║  EXPERIMENT 3: Latency Distribution           ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    auto [res_full, stats_full] = batch_proc.process(queries, 4);

    // Save per-query latencies to CSV for plotting
    std::ofstream lat_csv("milestone3_latencies.csv");
    lat_csv << "query_idx,source,target,distance_km,latency_ms,reachable\n";
    for (uint32_t i = 0; i < static_cast<uint32_t>(res_full.size()); ++i)
    {
        auto &r = res_full[i];
        double dist_km = (r.distance == INF_DIST) ? -1.0 : r.distance / 1e6;
        lat_csv << i << ","
                << r.source << ","
                << r.target << ","
                << std::fixed << std::setprecision(3) << dist_km << ","
                << r.latency_ms << ","
                << (r.distance != INF_DIST ? 1 : 0) << "\n";
    }
    std::cout << "Per-query latencies saved to: milestone3_latencies.csv\n";
    stats_full.print("Latency Distribution (4 threads, " +
                     std::to_string(num_queries) + " queries)");

    // ── Experiment 4: Amortization analysis ──────────────────────────────
    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║  EXPERIMENT 4: Preprocessing Amortization     ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    double build_ms = landmark_idx.build_time_ms();
    auto stats_lm = run_landmark_batch(g, landmark_idx, queries, 4);
    double astar_total = stats_lm.avg_latency_ms * num_queries;
    double dijk_total = stats_full.avg_latency_ms * num_queries;
    double breakeven = build_ms / (stats_full.avg_latency_ms - stats_lm.avg_latency_ms);

    std::cout << "\n  Landmark build time:      "
              << std::fixed << std::setprecision(1) << build_ms << " ms\n"
              << "  Dijkstra avg per query:   "
              << std::setprecision(3) << stats_full.avg_latency_ms << " ms\n"
              << "  A* avg per query:         "
              << stats_lm.avg_latency_ms << " ms\n"
              << "  Speedup per query:        "
              << std::setprecision(2)
              << stats_full.avg_latency_ms / stats_lm.avg_latency_ms << "x\n"
              << "  Break-even at:            "
              << std::setprecision(0) << breakeven << " queries\n"
              << "  With " << num_queries << " queries, amortized overhead: "
              << std::setprecision(3) << build_ms / num_queries
              << " ms/query\n";

    std::cout << "\nResults saved to: " << csv_path << "\n";
    std::cout << "Latencies saved to: milestone3_latencies.csv\n";

    return 0;
}