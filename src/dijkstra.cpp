#include "dijkstra.hpp"
#include <queue>
#include <functional>
#include <chrono>
#include <algorithm>

DijkstraResult dijkstra(const Graph& g, uint32_t source) {
    auto t_start = std::chrono::steady_clock::now();
    const uint32_t N = g.num_nodes();
    DijkstraResult res;
    res.dist.assign(N, INF_DIST);
    res.prev.assign(N, NO_NODE);
    res.dist[source] = 0;

    using Entry = std::pair<uint32_t, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    pq.emplace(0u, source);

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > res.dist[u]) continue;
        for (const Edge* e = g.edge_begin(u); e != g.edge_end(u); ++e) {
            if (res.dist[u] == INF_DIST) continue;
            uint32_t new_dist = res.dist[u] + e->weight;
            if (new_dist < res.dist[e->to]) {
                res.dist[e->to] = new_dist;
                res.prev[e->to] = u;
                pq.emplace(new_dist, e->to);
            }
        }
    }
    auto t_end = std::chrono::steady_clock::now();
    res.wall_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    return res;
}

std::vector<uint32_t> reconstruct_path(const DijkstraResult& res,
                                        uint32_t source, uint32_t target) {
    if (res.dist[target] == INF_DIST) return {};
    std::vector<uint32_t> path;
    for (uint32_t v = target; v != NO_NODE; v = res.prev[v])
        path.push_back(v);
    std::reverse(path.begin(), path.end());
    return path;
}
