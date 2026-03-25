#include <iostream>
#include <string>
#include <random>
#include "graph.hpp"
#include "osm_parser.hpp"
#include "dijkstra.hpp"
#include "benchmark.hpp"
#include "validator.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <osm_file.osm> [source_osm_id] [target_osm_id]\n";
        return 1;
    }

    // ------------------------------------------------------------------
    // 1. Parse
    // ------------------------------------------------------------------
    Graph g;
    OsmParser parser;
    {
        Timer t("OSM Parse");
        parser.parse(argv[1], g, "distance");
    }
    std::cout << "Graph: " << g.num_nodes() << " nodes, "
              << g.num_edges() << " edges\n";

    // ------------------------------------------------------------------
    // Correctness Validation (Milestone 1 requirement)
    // ------------------------------------------------------------------
    #ifndef BENCHMARK_MODE
        Validator::run(g);
    #endif

    // ------------------------------------------------------------------
    // 2. Single-query shortest path (if OSM IDs provided)
    // ------------------------------------------------------------------
    if (argc == 4) {
        int64_t src_osm = std::stoll(argv[2]);
        int64_t tgt_osm = std::stoll(argv[3]);

        auto& nm = parser.node_map();
        if (!nm.count(src_osm) || !nm.count(tgt_osm)) {
            std::cerr << "OSM node ID not found in graph.\n";
            return 1;
        }
        uint32_t src = nm.at(src_osm);
        uint32_t tgt = nm.at(tgt_osm);

        DijkstraResult res;
        {
            Timer t("Dijkstra (single query)");
            res = dijkstra(g, src);
        }

        if (res.dist[tgt] == INF_DIST) {
            std::cout << "No path found.\n";
        } else {
            double dist_km = res.dist[tgt] / 1e6; // mm → km
            std::cout << "Shortest distance: " << dist_km << " km\n";
            auto path = reconstruct_path(res, src, tgt);
            std::cout << "Path nodes: " << path.size() << "\n";
        }
        return 0;
    }

    // ------------------------------------------------------------------
    // 3. Benchmark: N random queries (no target → full SSSP)
    // ------------------------------------------------------------------
    #ifdef BENCHMARK_MODE
        constexpr int BENCH_QUERIES = 100;  // more queries = stable average
    #else
        constexpr int BENCH_QUERIES = 10;
    #endif
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist_node(0, g.num_nodes() - 1);

    std::cout << "\n--- Benchmark: " << BENCH_QUERIES << " random SSSP queries ---\n";
    double total_ms = 0;
    for (int i = 0; i < BENCH_QUERIES; ++i) {
        uint32_t src = dist_node(rng);
        Timer t("  Query " + std::to_string(i));
        auto res = dijkstra(g, src);
        total_ms += t.elapsed_ms();
    }
    std::cout << "Avg query time: " << total_ms / BENCH_QUERIES << " ms\n";
    std::cout << "Throughput:     " << 1000.0 * BENCH_QUERIES / total_ms
              << " queries/sec\n";

    return 0;
}