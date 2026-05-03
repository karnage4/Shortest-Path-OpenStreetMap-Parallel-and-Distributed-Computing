#include <iostream>
#include <string>
#include <iomanip>
#include <random>
#include <vector>
#include "graph.hpp"
#include "osm_parser.hpp"
#include "dijkstra.hpp"
#include "benchmark.hpp"
#include "delta_stepping.hpp"
#include "bidirectional_dijkstra.hpp"
#include "parallel_dijkstra.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <osm_file> [threads=4] [delta_m=150] [queries=20]\n";
        return 1;
    }
    uint32_t num_threads = (argc > 2) ? std::stoul(argv[2]) : 4;
    uint32_t delta_m     = (argc > 3) ? std::stoul(argv[3]) : 150;
    uint32_t num_queries = (argc > 4) ? std::stoul(argv[4]) : 20;
    uint32_t delta_mm    = delta_m * 1000;

    Graph g; OsmParser parser;
    { Timer t("OSM Parse"); parser.parse(argv[1], g, "distance"); }
    std::cout << "Graph: " << g.num_nodes() << " nodes, " << g.num_edges() << " edges\n";

    Graph g_rev;
    { Timer t("Build Reverse Graph"); g_rev = build_reverse_graph(g); }

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> rnd(0, g.num_nodes() - 1);
    std::vector<uint32_t> sources(num_queries), targets(num_queries);
    for (auto& s : sources) s = rnd(rng);
    std::mt19937 rng2(99);
    for (auto& t : targets) t = rnd(rng2);

    // Sequential
    double seq_total = 0;
    std::vector<std::vector<uint32_t>> seq_dists(num_queries);
    for (uint32_t i = 0; i < num_queries; ++i) {
        auto r = dijkstra(g, sources[i]);
        seq_total += r.wall_ms; seq_dists[i] = r.dist;
    }
    double seq_avg = seq_total / num_queries;

    // Delta-Stepping
    double ds_total = 0; int ds_ok = 0;
    for (uint32_t i = 0; i < num_queries; ++i) {
        auto r = delta_stepping(g, sources[i], delta_mm, num_threads);
        ds_total += r.wall_ms;
        bool ok = true;
        for (uint32_t v = 0; v < g.num_nodes(); ++v)
            if (r.dist[v] != seq_dists[i][v]) { ok = false; break; }
        if (ok) ++ds_ok;
    }
    double ds_avg = ds_total / num_queries;

    // Bidirectional
    double bd_total = 0; int bd_ok = 0;
    for (uint32_t i = 0; i < num_queries; ++i) {
        auto r = bidirectional_dijkstra(g, g_rev, sources[i], targets[i]);
        bd_total += r.wall_ms;
        if (r.best_dist == seq_dists[i][targets[i]]) ++bd_ok;
    }
    double bd_avg = bd_total / num_queries;

    // Shared-Queue
    double pq_total = 0; int pq_ok = 0; uint64_t pq_cont = 0;
    for (uint32_t i = 0; i < num_queries; ++i) {
        auto r = parallel_dijkstra(g, sources[i], num_threads);
        pq_total += r.wall_ms; pq_cont += r.lock_contentions;
        bool ok = true;
        for (uint32_t v = 0; v < g.num_nodes(); ++v)
            if (r.dist[v] != seq_dists[i][v]) { ok = false; break; }
        if (ok) ++pq_ok;
    }
    double pq_avg = pq_total / num_queries;

    // Results table
    std::cout << "\n============================================================\n";
    std::cout << "  MILESTONE 2 - PARALLEL BENCHMARK\n";
    std::cout << "  Queries: " << num_queries << "  |  Threads: " << num_threads
              << "  |  Delta: " << delta_m << " m\n";
    std::cout << "============================================================\n";
    std::cout << std::left
              << std::setw(30) << "Algorithm"
              << std::setw(12) << "Avg (ms)"
              << std::setw(12) << "Speedup"
              << std::setw(14) << "Correct"
              << "Contentions/q\n";
    std::cout << std::string(82, '-') << "\n";

    auto row = [&](const std::string& name, double avg, int ok, uint64_t cont) {
        std::cout << std::left << std::setw(30) << name
                  << std::setw(12) << std::fixed << std::setprecision(3) << avg
                  << std::setw(12) << std::fixed << std::setprecision(2) << (seq_avg/avg)
                  << std::setw(14) << (std::to_string(ok)+"/"+std::to_string(num_queries))
                  << cont << "\n";
    };
    row("Sequential Dijkstra",   seq_avg, (int)num_queries, 0);
    row("Delta-Stepping ("+std::to_string(num_threads)+"T)", ds_avg, ds_ok, 0);
    row("Bidirectional (2T)",    bd_avg,  bd_ok,            0);
    row("Shared-Queue ("+std::to_string(num_threads)+"T)",   pq_avg, pq_ok, pq_cont/num_queries);
    std::cout << std::string(82, '-') << "\n";

    // Thread scaling
    std::cout << "\n=== Thread Scaling (Delta-Stepping vs Shared-Queue) ===\n";
    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(14) << "DS Avg (ms)"
              << std::setw(14) << "SQ Avg (ms)"
              << "DS Speedup\n";
    std::cout << std::string(50, '-') << "\n";

    for (uint32_t t : {1u, 2u, 4u, 8u}) {
        double ds_s = 0, pq_s = 0;
        std::mt19937 r3(42);
        std::uniform_int_distribution<uint32_t> rd(0, g.num_nodes()-1);
        for (uint32_t i = 0; i < num_queries; ++i) {
            uint32_t src = rd(r3);
            ds_s += delta_stepping(g, src, delta_mm, t).wall_ms;
            pq_s += parallel_dijkstra(g, src, t).wall_ms;
        }
        ds_s /= num_queries; pq_s /= num_queries;
        std::cout << std::left << std::setw(10) << t
                  << std::setw(14) << std::fixed << std::setprecision(3) << ds_s
                  << std::setw(14) << std::fixed << std::setprecision(3) << pq_s
                  << std::fixed << std::setprecision(2) << (seq_avg/ds_s) << "\n";
    }
    std::cout << std::string(50, '-') << "\n";
    return 0;
}
