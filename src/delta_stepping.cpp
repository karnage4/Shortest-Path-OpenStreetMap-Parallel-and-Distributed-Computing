#include "delta_stepping.hpp"
#include <algorithm>
#include <chrono>
#include <omp.h>

static bool relax_atomic(std::vector<std::atomic<uint32_t>>& dist,
                          uint32_t v, uint32_t new_dist) {
    uint32_t old_dist = dist[v].load(std::memory_order_relaxed);
    while (new_dist < old_dist)
        if (dist[v].compare_exchange_weak(old_dist, new_dist,
                std::memory_order_release, std::memory_order_relaxed))
            return true;
    return false;
}

DeltaSteppingResult delta_stepping(const Graph& g, uint32_t source,
                                    uint32_t delta, uint32_t num_threads) {
    auto t0 = std::chrono::steady_clock::now();
    const uint32_t N = g.num_nodes();

    std::vector<std::atomic<uint32_t>> dist(N);
    for (auto& d : dist) d.store(INF_DIST, std::memory_order_relaxed);
    dist[source].store(0, std::memory_order_relaxed);

    std::vector<uint32_t> prev(N, NO_NODE);
    const uint32_t NB = 1024;
    std::vector<std::vector<uint32_t>> buckets(NB);
    std::vector<std::mutex> blk(NB);
    buckets[0].push_back(source);

    omp_set_num_threads(num_threads);

    uint32_t cb = 0;
    while (true) {
        while (cb < NB && buckets[cb % NB].empty()) ++cb;
        if (cb >= NB) break;
        uint32_t b = cb % NB;

        // Phase 1: light edges
        while (!buckets[b].empty()) {
            std::vector<uint32_t> nodes;
            { std::lock_guard<std::mutex> lk(blk[b]); nodes.swap(buckets[b]); }

            #pragma omp parallel for schedule(dynamic, 64)
            for (int i = 0; i < (int)nodes.size(); ++i) {
                uint32_t u = nodes[i];
                uint32_t du = dist[u].load(std::memory_order_acquire);
                if (du == INF_DIST || du / delta != cb) continue;
                for (const Edge* e = g.edge_begin(u); e != g.edge_end(u); ++e) {
                    if (e->weight > delta) continue;
                    uint32_t nd = du + e->weight;
                    if (relax_atomic(dist, e->to, nd)) {
                        prev[e->to] = u;
                        uint32_t tb = (nd / delta) % NB;
                        std::lock_guard<std::mutex> lk(blk[tb]);
                        buckets[tb].push_back(e->to);
                    }
                }
            }
        }

        // Phase 2: heavy edges
        std::vector<uint32_t> hs;
        for (uint32_t v = 0; v < N; ++v) {
            uint32_t dv = dist[v].load(std::memory_order_relaxed);
            if (dv != INF_DIST && dv / delta == cb) hs.push_back(v);
        }

        #pragma omp parallel for schedule(dynamic, 64)
        for (int i = 0; i < (int)hs.size(); ++i) {
            uint32_t u = hs[i];
            uint32_t du = dist[u].load(std::memory_order_acquire);
            for (const Edge* e = g.edge_begin(u); e != g.edge_end(u); ++e) {
                if (e->weight <= delta) continue;
                uint32_t nd = du + e->weight;
                if (relax_atomic(dist, e->to, nd)) {
                    prev[e->to] = u;
                    uint32_t tb = (nd / delta) % NB;
                    std::lock_guard<std::mutex> lk(blk[tb]);
                    buckets[tb].push_back(e->to);
                }
            }
        }
        ++cb;
    }

    DeltaSteppingResult res;
    res.dist.resize(N); res.prev = prev;
    for (uint32_t v = 0; v < N; ++v)
        res.dist[v] = dist[v].load(std::memory_order_relaxed);
    res.wall_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
    return res;
}

std::vector<uint32_t> delta_reconstruct_path(const DeltaSteppingResult& res,
                                              uint32_t source, uint32_t target) {
    if (res.dist[target] == INF_DIST) return {};
    std::vector<uint32_t> path;
    for (uint32_t v = target; v != NO_NODE; v = res.prev[v]) path.push_back(v);
    std::reverse(path.begin(), path.end());
    return path;
}
