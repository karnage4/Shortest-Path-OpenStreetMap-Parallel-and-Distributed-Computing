#pragma once
#include "graph.hpp"
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// OSM road categories we care about, with typical speed limits (km/h).
// Used to compute travel-time weights as an alternative to distance.
// ---------------------------------------------------------------------------
struct RoadType {
    const char* tag;
    uint32_t    speed_kmh;
};

inline constexpr RoadType ROAD_TYPES[] = {
    {"motorway",       130},
    {"motorway_link",  80},
    {"trunk",          100},
    {"trunk_link",     60},
    {"primary",        80},
    {"primary_link",   50},
    {"secondary",      60},
    {"secondary_link", 40},
    {"tertiary",       40},
    {"tertiary_link",  30},
    {"residential",    30},
    {"living_street",  10},
    {"unclassified",   30},
    {"road",           30},
};

// Returns speed in km/h for a highway tag value, or 0 if not a road we route on.
uint32_t highway_speed(const std::string& highway_tag);

// ---------------------------------------------------------------------------
// Parse an OSM XML file and populate graph.
// Node ID remapping: OSM node IDs are 64-bit sparse integers. We remap them
// to dense [0, N) uint32_t IDs here so the graph uses plain array indexing.
// ---------------------------------------------------------------------------
class OsmParser {
public:
    // weight_mode: "distance" (mm) or "time" (ms ≈ mm / speed)
    void parse(const std::string& filepath, Graph& graph,
               const std::string& weight_mode = "distance");

    // After parse(), maps OSM node ID → dense graph ID
    const std::unordered_map<int64_t, uint32_t>& node_map() const {
        return node_map_;
    }

private:
    std::unordered_map<int64_t, uint32_t> node_map_;

    // Haversine distance between two lat/lon points, returns millimetres
    static uint32_t haversine_mm(double lat1, double lon1,
                                 double lat2, double lon2);
};