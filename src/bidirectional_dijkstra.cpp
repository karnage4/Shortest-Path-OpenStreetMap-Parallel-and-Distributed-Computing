#include "bidirectional_dijkstra.hpp"
#include <queue>
#include <thread>
#include <chrono>
#include <functional>
#include <algorithm>

Graph build_reverse_graph(const Graph& g) {
    Graph rev;
    rev.reserve(g.num_nodes(), g.num_edges());
    rev.coords = g.coords;
    for (uint32_t u = 0; u < g.num_nodes(); ++u)
        for (const Edge* e = g.edge_begin(u); e != g.edge_end(u); ++e)
            rev.add_edge(e->to, u, e->weight);
    rev.finalize();
    return rev;
}

static void dijkstra_worker(
    const Graph& g, uint32_t source,
    std::vector<std::atomic<uint32_t>>& dist_mine,
    std::vector<std::atomic<uint32_t>>& dist_other,
    std::atomic<uint32_t>& best_path,
    std::atomic<bool>& done,
    std::vector<uint32_t>& prev)
{
    using Entry = std::pair<uint32_t, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    dist_mine[source].store(0, std::memory_order_relaxed);
    pq.emplace(0u, source);

    while (!pq.empty()) {
        if (done.load(std::memory_order_acquire)) break;
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist_mine[u].load(std::memory_order_relaxed)) continue;
        if (d >= best_path.load(std::memory_order_acquire)) {
            done.store(true, std::memory_order_release); break;
        }
        for (const Edge* e = g.edge_begin(u); e != g.edge_end(u); ++e) {
            uint32_t nd = d + e->weight;
            uint32_t od = dist_mine[e->to].load(std::memory_order_relaxed);
            while (nd < od)
                if (dist_mine[e->to].compare_exchange_weak(od, nd,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    prev[e->to] = u; pq.emplace(nd, e->to); break;
                }
            uint32_t other = dist_other[e->to].load(std::memory_order_acquire);
            if (other != INF_DIST) {
                uint32_t tot = nd + other;
                uint32_t cur = best_path.load(std::memory_order_relaxed);
                while (tot < cur)
                    if (best_path.compare_exchange_weak(cur, tot,
                            std::memory_order_release, std::memory_order_relaxed)) break;
            }
        }
    }
}

BiDijkstraResult bidirectional_dijkstra(const Graph& g, const Graph& g_rev,
                                         uint32_t source, uint32_t target) {
    auto t0 = std::chrono::steady_clock::now();
    const uint32_t N = g.num_nodes();

    std::vector<std::atomic<uint32_t>> df(N), db(N);
    for (uint32_t i = 0; i < N; ++i) {
        df[i].store(INF_DIST, std::memory_order_relaxed);
        db[i].store(INF_DIST, std::memory_order_relaxed);
    }
    std::vector<uint32_t> pf(N, NO_NODE), pb(N, NO_NODE);
    std::atomic<uint32_t> best{INF_DIST};
    std::atomic<bool> done{false};

    std::thread fwd([&]() { dijkstra_worker(g,     source, df, db, best, done, pf); });
    std::thread bwd([&]() { dijkstra_worker(g_rev, target, db, df, best, done, pb); });
    fwd.join(); bwd.join();

    BiDijkstraResult res;
    res.dist_fwd.resize(N); res.dist_bwd.resize(N);
    for (uint32_t i = 0; i < N; ++i) {
        res.dist_fwd[i] = df[i].load(std::memory_order_relaxed);
        res.dist_bwd[i] = db[i].load(std::memory_order_relaxed);
    }
    res.best_dist = best.load(std::memory_order_relaxed);

    uint32_t meet = NO_NODE;
    for (uint32_t v = 0; v < N; ++v) {
        if (res.dist_fwd[v] == INF_DIST || res.dist_bwd[v] == INF_DIST) continue;
        if (res.dist_fwd[v] + res.dist_bwd[v] == res.best_dist) { meet = v; break; }
    }
    if (meet != NO_NODE) {
        std::vector<uint32_t> fp, bp;
        for (uint32_t v = meet; v != NO_NODE; v = pf[v]) fp.push_back(v);
        std::reverse(fp.begin(), fp.end());
        for (uint32_t v = meet; v != NO_NODE; v = pb[v]) bp.push_back(v);
        res.path = fp;
        for (size_t i = 1; i < bp.size(); ++i) res.path.push_back(bp[i]);
    }
    res.wall_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
    return res;
}
