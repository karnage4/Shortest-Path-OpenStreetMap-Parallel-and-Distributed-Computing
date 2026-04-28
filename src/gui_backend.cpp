#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include "graph.hpp"
#include "osm_parser.hpp"
#include "dijkstra.hpp"
#include "bidirectional_dijkstra.hpp"
#include "delta_stepping.hpp"
#include "parallel_dijkstra.hpp"
#include "landmark_preprocess.hpp"
#include "benchmark.hpp"

// Unified backend for the Python GUI
// Outputs results in a format the Python script can easily parse
int main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <osm_file> <algo> <src_id> <tgt_id> <threads> [delta]\n";
        return 1;
    }

    std::string osm_file = argv[1];
    std::string algo = argv[2];
    int64_t src_osm = std::stoll(argv[3]);
    int64_t tgt_osm = std::stoll(argv[4]);
    uint32_t threads = std::stoul(argv[5]);
    uint32_t delta_mm = (argc > 6) ? std::stoul(argv[6]) * 1000 : 150000;

    uint32_t num_landmarks = (argc > 7) ? std::stoul(argv[7]) : 16;

    Graph g;
    OsmParser parser;
    parser.parse(osm_file, g, "distance");

    auto& nm = parser.node_map();
    if (!nm.count(src_osm) || !nm.count(tgt_osm)) {
        std::cout << "RESULT_ERROR: OSM ID not found in current map.\n";
        return 0;
    }
    uint32_t src = nm.at(src_osm);
    uint32_t tgt = nm.at(tgt_osm);

    uint32_t dist = INF_DIST;
    double time_ms = 0;
    std::vector<uint32_t> path_nodes;

    if (algo == "dijkstra") {
        auto r = dijkstra(g, src);
        dist = r.dist[tgt];
        time_ms = r.wall_ms;
        if (dist != INF_DIST) path_nodes = reconstruct_path(r, src, tgt);
    } else if (algo == "bidirectional") {
        Graph g_rev = build_reverse_graph(g);
        auto r = bidirectional_dijkstra(g, g_rev, src, tgt);
        dist = r.best_dist;
        time_ms = r.wall_ms;
        if (dist != INF_DIST) path_nodes = r.path;
    } else if (algo == "delta_stepping") {
        auto r = delta_stepping(g, src, delta_mm, threads);
        dist = r.dist[tgt];
        time_ms = r.wall_ms;
        if (dist != INF_DIST) path_nodes = delta_reconstruct_path(r, src, tgt);
    } else if (algo == "parallel_dijkstra") {
        auto r = parallel_dijkstra(g, src, threads);
        dist = r.dist[tgt];
        time_ms = r.wall_ms;
        if (dist != INF_DIST) {
            uint32_t curr = tgt;
            while (curr != src && curr != NO_NODE) {
                path_nodes.push_back(curr);
                curr = r.prev[curr];
            }
            if (curr == src) path_nodes.push_back(src);
            std::reverse(path_nodes.begin(), path_nodes.end());
        }
    } else if (algo == "landmark_astar") {
        Graph g_rev = build_reverse_graph(g);
        LandmarkIndex l_idx;
        l_idx.build(g, g_rev, num_landmarks, threads); 
        Timer t("A* Query");
        auto [distance, path] = l_idx.astar_query(g, src, tgt);
        time_ms = t.elapsed_ms();
        dist = distance;
        path_nodes = path;
    }

    if (dist == INF_DIST) {
        std::cout << "RESULT_NOT_FOUND\n";
    } else {
        std::cout << "RESULT_DIST: " << std::fixed << std::setprecision(3) << (dist / 1e6) << " km\n";
        std::cout << "RESULT_TIME: " << time_ms << " ms\n";
        
        // Output path coordinates for Python visualization
        std::cout << "PATH_COORDS_START\n";
        for (uint32_t node_id : path_nodes) {
            std::cout << g.coords[node_id].lat << "," << g.coords[node_id].lon << "\n";
        }
        std::cout << "PATH_COORDS_END\n";
    }

    return 0;
}
