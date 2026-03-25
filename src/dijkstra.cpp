#include "dijkstra.hpp"
#include <queue>
#include <functional>  // std::greater

DijkstraResult dijkstra(const Graph& g, uint32_t source) {
    const uint32_t N = g.num_nodes();

    DijkstraResult res;
    res.dist.assign(N, INF_DIST);
    res.prev.assign(N, NO_NODE);
    res.dist[source] = 0;

    // Min-heap: (distance, node)
    // std::greater makes it a min-heap (smallest distance on top).
    using Entry = std::pair<uint32_t, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    pq.emplace(0u, source);

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        // Lazy deletion: if we already found a better path, skip this entry.
        // This is O(1) and avoids a decrease-key structure entirely.
        if (d > res.dist[u]) continue;

        // Relax all outgoing edges of u.
        // edge_begin/end return raw pointers into a flat vector → sequential
        // memory access, excellent cache behaviour.
        for (const Edge* e = g.edge_begin(u); e != g.edge_end(u); ++e) {
            // Guard against overflow: INF_DIST + anything overflows uint32_t
            if (res.dist[u] == INF_DIST) continue;

            uint32_t new_dist = res.dist[u] + e->weight;
            if (new_dist < res.dist[e->to]) {
                res.dist[e->to] = new_dist;
                res.prev[e->to] = u;
                pq.emplace(new_dist, e->to);
            }
        }
    }
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