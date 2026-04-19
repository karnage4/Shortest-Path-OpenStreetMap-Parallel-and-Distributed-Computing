#include "parallel_dijkstra.hpp"
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>

ParallelDijkstraResult parallel_dijkstra(const Graph& g, uint32_t source,
                                          uint32_t num_threads) {
    auto t0 = std::chrono::steady_clock::now();
    const uint32_t N = g.num_nodes();

    std::vector<std::atomic<uint32_t>> dist(N);
    for (auto& d : dist) d.store(INF_DIST, std::memory_order_relaxed);
    dist[source].store(0, std::memory_order_relaxed);

    std::vector<std::atomic<uint32_t>> prev_a(N);
    for (auto& p : prev_a) p.store(NO_NODE, std::memory_order_relaxed);

    using Entry = std::pair<uint32_t, uint32_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    std::mutex pq_mutex;
    pq.emplace(0u, source);

    std::atomic<uint64_t> contentions{0};
    // pending = nodes popped but not yet finished processing
    std::atomic<int> pending{0};

    auto worker = [&]() {
        while (true) {
            Entry top{INF_DIST, 0};
            {
                std::unique_lock<std::mutex> lk(pq_mutex);
                // Exit only when queue empty AND no node is being processed
                if (pq.empty()) {
                    if (pending.load() == 0) return;
                    lk.unlock();
                    std::this_thread::yield();
                    continue;
                }
                top = pq.top(); pq.pop();
                pending.fetch_add(1);
            }

            auto [d, u] = top;
            if (d > dist[u].load(std::memory_order_relaxed)) {
                pending.fetch_sub(1);
                continue;
            }

            for (const Edge* e = g.edge_begin(u); e != g.edge_end(u); ++e) {
                uint32_t nd = d + e->weight;
                uint32_t od = dist[e->to].load(std::memory_order_relaxed);
                while (nd < od) {
                    if (dist[e->to].compare_exchange_weak(od, nd,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        prev_a[e->to].store(u, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lk(pq_mutex);
                        pq.emplace(nd, e->to);
                        break;
                    }
                }
            }
            pending.fetch_sub(1);
        }
    };

    // Count contentions separately
    std::atomic<bool> done{false};
    std::thread contention_worker([&]() {
        while (!done.load()) {
            if (!pq_mutex.try_lock()) { ++contentions; }
            else { pq_mutex.unlock(); }
            std::this_thread::yield();
        }
    });

    std::vector<std::thread> threads;
    for (uint32_t t = 0; t < num_threads; ++t) threads.emplace_back(worker);
    for (auto& t : threads) t.join();
    done.store(true);
    contention_worker.join();

    ParallelDijkstraResult res;
    res.dist.resize(N); res.prev.resize(N);
    for (uint32_t v = 0; v < N; ++v) {
        res.dist[v] = dist[v].load(std::memory_order_relaxed);
        res.prev[v] = prev_a[v].load(std::memory_order_relaxed);
    }
    res.lock_contentions = contentions.load();
    res.wall_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
    return res;
}
