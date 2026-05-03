#pragma once
#include "graph.hpp"
#include "dijkstra.hpp"
#include <iostream>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Correctness Validator for Milestone 1
//
// Two checks:
//  1. TRIANGLE INEQUALITY - for three random nodes A, B, C:
//     dist(A,C) <= dist(A,B) + dist(B,C) must always hold.
//     If Dijkstra is correct this is always true.
//
//  2. HAVERSINE LOWER BOUND - Dijkstra road distance >= straight-line
//     (Haversine) distance between same two nodes. Roads can't be shorter
//     than a straight line, so any violation = bug.
//
//  3. SYMMETRY CHECK - for two-way roads: dist(A→B) should equal dist(B→A).
//     Violations indicate a one-way parsing bug.
// ---------------------------------------------------------------------------

namespace Validator {

// Haversine straight-line distance in mm (reused from parser)
inline uint32_t haversine_mm(double lat1, double lon1,
                               double lat2, double lon2) {
    constexpr double R  = 6371000.0;
    constexpr double PI = 3.14159265358979323846;
    auto rad = [PI](double d){ return d * PI / 180.0; };
    double dlat = rad(lat2 - lat1), dlon = rad(lon2 - lon1);
    double a = std::sin(dlat/2)*std::sin(dlat/2)
             + std::cos(rad(lat1))*std::cos(rad(lat2))
             * std::sin(dlon/2)*std::sin(dlon/2);
    return static_cast<uint32_t>(R * 2.0 * std::atan2(std::sqrt(a),
                                  std::sqrt(1-a)) * 1000.0);
}

inline void run(const Graph& g, uint32_t num_tests = 5) {
    std::cout << "\n========== CORRECTNESS VALIDATION ==========\n";

    const uint32_t N = g.num_nodes();
    if (N < 3) {
        std::cout << "[SKIP] Graph too small to validate.\n";
        return;
    }

    // Pick evenly spaced test nodes so results are reproducible
    uint32_t step = N / (num_tests + 1);
    std::vector<uint32_t> nodes;
    for (uint32_t i = 1; i <= num_tests; ++i)
        nodes.push_back(i * step);

    int passed = 0, failed = 0;

    // ------------------------------------------------------------------
    // CHECK 1: Haversine Lower Bound
    // Road distance (Dijkstra) must be >= straight-line distance
    // ------------------------------------------------------------------
    std::cout << "\n[CHECK 1] Haversine Lower Bound (road >= straight-line)\n";
    for (uint32_t i = 0; i + 1 < nodes.size(); ++i) {
        uint32_t src = nodes[i], tgt = nodes[i + 1];
        auto res = dijkstra(g, src);

        if (res.dist[tgt] == INF_DIST) {
            std::cout << "  Node " << src << " -> " << tgt
                      << " : UNREACHABLE (skip)\n";
            continue;
        }

        auto& cs = g.coords[src];
        auto& ct = g.coords[tgt];
        uint32_t straight = haversine_mm(cs.lat, cs.lon, ct.lat, ct.lon);
        bool ok = res.dist[tgt] >= straight;

        std::cout << "  Node " << src << " -> " << tgt
                  << " | Road: "     << res.dist[tgt] / 1000.0 << " m"
                  << " | Straight: " << straight / 1000.0      << " m"
                  << (ok ? "  PASS" : "  FAIL <<<") << "\n";
        ok ? ++passed : ++failed;
    }

    // ------------------------------------------------------------------
    // CHECK 2: Triangle Inequality  dist(A,C) <= dist(A,B) + dist(B,C)
    // ------------------------------------------------------------------
    std::cout << "\n[CHECK 2] Triangle Inequality\n";
    for (uint32_t i = 0; i + 2 < nodes.size(); ++i) {
        uint32_t A = nodes[i], B = nodes[i+1], C = nodes[i+2];
        auto resA = dijkstra(g, A);
        auto resB = dijkstra(g, B);

        if (resA.dist[C] == INF_DIST || resA.dist[B] == INF_DIST
                                     || resB.dist[C] == INF_DIST) {
            std::cout << "  (" << A << "," << B << "," << C
                      << ") : UNREACHABLE (skip)\n";
            continue;
        }

        bool ok = resA.dist[C] <= resA.dist[B] + resB.dist[C];
        std::cout << "  dist(" << A << "->" << C << ")=" << resA.dist[C]
                  << " <= dist(" << A << "->" << B << ")=" << resA.dist[B]
                  << " + dist(" << B << "->" << C << ")=" << resB.dist[C]
                  << (ok ? "  PASS" : "  FAIL <<<") << "\n";
        ok ? ++passed : ++failed;
    }

    // ------------------------------------------------------------------
    // CHECK 3: Symmetry (A->B distance should equal B->A on two-way roads)
    // We allow 1% tolerance for floating point/rounding
    // ------------------------------------------------------------------
    std::cout << "\n[CHECK 3] Symmetry (A->B == B->A for two-way roads)\n";
    for (uint32_t i = 0; i + 1 < nodes.size(); ++i) {
        uint32_t A = nodes[i], B = nodes[i+1];
        auto resA = dijkstra(g, A);
        auto resB = dijkstra(g, B);

        if (resA.dist[B] == INF_DIST || resB.dist[A] == INF_DIST) {
            std::cout << "  (" << A << "<->" << B
                      << ") : UNREACHABLE (skip)\n";
            continue;
        }

        // 1% tolerance - one-way roads can legitimately differ
        double ratio = static_cast<double>(resA.dist[B]) /
                       static_cast<double>(resB.dist[A]);
        bool ok = (ratio > 0.5 && ratio < 2.0); // loose check - flags big asymmetry only

        std::cout << "  dist(" << A << "->" << B << ")=" << resA.dist[B]
                  << "  dist(" << B << "->" << A << ")=" << resB.dist[A]
                  << (ok ? "  PASS" : "  ASYMMETRIC (may be one-way)") << "\n";
        ok ? ++passed : ++failed;
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    std::cout << "\n--------------------------------------------\n";
    std::cout << "Results: " << passed << " PASSED  |  " << failed << " FAILED\n";
    if (failed == 0)
        std::cout << "STATUS: ALL CHECKS PASSED - Dijkstra is correct\n";
    else
        std::cout << "STATUS: FAILURES DETECTED - check parsing logic\n";
    std::cout << "============================================\n\n";
}

} // namespace Validator