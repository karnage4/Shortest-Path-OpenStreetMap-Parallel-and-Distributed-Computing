//cache friendly CSR graph representation
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <limits>

// ---------------------------------------------------------------------------
// Edge weight: stored as uint32_t (millimetres). Max ~4295 km per edge — fine
// for OSM road segments. Keeps the struct at 8 bytes → two fit in a cache line.
// ---------------------------------------------------------------------------
struct Edge {
    uint32_t to;      // destination node (dense ID)
    uint32_t weight;  // distance in mm
};

// ---------------------------------------------------------------------------
// CSR (Compressed Sparse Row) adjacency list.
//
// Memory layout (illustrative, 4-node graph):
//   head_:  [0, 2, 5, 5, 7]   — head_[u]..head_[u+1] is the edge range for u
//   edges_: [e0, e1, e2, e3, e4, e5, e6]
//
// This stores all edges contiguously. Iterating neighbours of node u only
// touches head_[u], head_[u+1], and edges_[head_[u]..head_[u+1]-1] — a tight,
// predictable memory region that the hardware prefetcher loves.
// ---------------------------------------------------------------------------
class Graph {
public:
    // Build phase: call add_edge() repeatedly, then finalize().
    void reserve(uint32_t num_nodes, uint32_t num_edges);
    void add_edge(uint32_t from, uint32_t to, uint32_t weight_mm);
    void finalize();   // constructs CSR from edge list

    // Query
    uint32_t num_nodes() const { return static_cast<uint32_t>(head_.size()) - 1; }
    uint32_t num_edges() const { return static_cast<uint32_t>(edges_.size()); }

    // Iterate edges: for (auto i = head_[u]; i < head_[u+1]; ++i) { edges_[i] }
    const Edge* edge_begin(uint32_t u) const { return edges_.data() + head_[u]; }
    const Edge* edge_end  (uint32_t u) const { return edges_.data() + head_[u+1]; }

    // Node coordinates (for Haversine weight computation during parse)
    struct NodeCoord { double lat, lon; };
    std::vector<NodeCoord> coords;   // indexed by dense node ID

private:
    std::vector<uint32_t> head_;    // size: N+1
    std::vector<Edge>     edges_;   // size: E

    // Temporary storage during build phase
    struct RawEdge { uint32_t from, to, weight; };
    std::vector<RawEdge> raw_edges_;
    uint32_t num_nodes_ = 0;
};

// Sentinel for "no path" or "unvisited"
inline constexpr uint32_t INF_DIST = std::numeric_limits<uint32_t>::max();
inline constexpr uint32_t NO_NODE  = std::numeric_limits<uint32_t>::max();
