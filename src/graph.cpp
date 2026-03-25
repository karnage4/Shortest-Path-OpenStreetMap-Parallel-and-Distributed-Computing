#include "graph.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>

void Graph::reserve(uint32_t num_nodes, uint32_t num_edges) {
    num_nodes_ = num_nodes;
    raw_edges_.reserve(num_edges);
    coords.resize(num_nodes);
}

void Graph::add_edge(uint32_t from, uint32_t to, uint32_t weight_mm) {
    assert(from < num_nodes_ && to < num_nodes_);
    raw_edges_.push_back({from, to, weight_mm});
}

void Graph::finalize() {
    // Count out-degree of each node
    head_.assign(num_nodes_ + 1, 0);
    for (auto& e : raw_edges_)
        ++head_[e.from + 1];          // offset by 1 for prefix-sum trick

    // Prefix sum → head_[u] = start index of u's edges in edges_
    for (uint32_t u = 1; u <= num_nodes_; ++u)
        head_[u] += head_[u - 1];

    // Fill edges_ in CSR order using a copy of head_ as write cursors
    edges_.resize(raw_edges_.size());
    std::vector<uint32_t> cursor(head_.begin(), head_.end());
    for (auto& e : raw_edges_) {
        auto idx = cursor[e.from]++;
        edges_[idx] = {e.to, e.weight};
    }

    raw_edges_.clear();
    raw_edges_.shrink_to_fit();   // release build memory
}