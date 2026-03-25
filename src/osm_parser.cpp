#include "osm_parser.hpp"
#include <pugixml.hpp>
#include <cmath>
#include <stdexcept>
#include <iostream>

// ---------------------------------------------------------------------------
// Haversine formula. Returns distance in millimetres (uint32_t).
// Max OSM edge ~200 km → 200,000,000 mm — well within uint32_t (4,294,967,295).
// ---------------------------------------------------------------------------
uint32_t OsmParser::haversine_mm(double lat1, double lon1,
                                  double lat2, double lon2) {
    constexpr double R  = 6371000.0; // Earth radius in metres
    constexpr double PI = 3.14159265358979323846;
    auto to_rad = [PI](double d) { return d * PI / 180.0; };

    double dlat = to_rad(lat2 - lat1);
    double dlon = to_rad(lon2 - lon1);
    double a = std::sin(dlat/2) * std::sin(dlat/2)
             + std::cos(to_rad(lat1)) * std::cos(to_rad(lat2))
             * std::sin(dlon/2) * std::sin(dlon/2);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return static_cast<uint32_t>(R * c * 1000.0); // metres → mm
}

uint32_t highway_speed(const std::string& tag) {
    for (auto& rt : ROAD_TYPES)
        if (tag == rt.tag) return rt.speed_kmh;
    return 0; // not a routable road
}

void OsmParser::parse(const std::string& filepath, Graph& graph,
                      const std::string& weight_mode) {
    pugi::xml_document doc;
    auto result = doc.load_file(filepath.c_str());
    if (!result)
        throw std::runtime_error("Failed to parse OSM file: " +
                                 std::string(result.description()));

    auto osm = doc.child("osm");

    // -----------------------------------------------------------------------
    // Pass 1: Collect all node IDs referenced by routable ways.
    // We only keep nodes that appear in at least one way — avoids allocating
    // dense IDs for POI nodes, bus stops, etc. that bloat the graph.
    // -----------------------------------------------------------------------
    struct WayInfo {
        std::vector<int64_t> node_ids;
        bool one_way = false;
        uint32_t speed_kmh = 0;
    };
    std::vector<WayInfo> ways;

    for (auto way : osm.children("way")) {
        std::string highway_tag;
        bool one_way = false;
        bool access_denied = false;

        for (auto tag : way.children("tag")) {
            std::string k = tag.attribute("k").value();
            std::string v = tag.attribute("v").value();
            if (k == "highway")  highway_tag = v;
            if (k == "oneway" && (v == "yes" || v == "-1")) one_way = true;
            if (k == "access"  && (v == "no" || v == "private")) access_denied = true;
        }

        uint32_t spd = highway_speed(highway_tag);
        if (spd == 0 || access_denied) continue;

        WayInfo wi;
        wi.speed_kmh = spd;
        wi.one_way   = one_way;
        for (auto nd : way.children("nd"))
            wi.node_ids.push_back(nd.attribute("ref").as_llong());
        ways.push_back(std::move(wi));
    }

    // Collect referenced OSM node IDs
    std::unordered_map<int64_t, Graph::NodeCoord> osm_nodes_raw;
    for (auto& w : ways)
        for (int64_t id : w.node_ids)
            osm_nodes_raw[id] = {}; // placeholder

    // -----------------------------------------------------------------------
    // Pass 2: Fill in lat/lon for referenced nodes only.
    // -----------------------------------------------------------------------
    for (auto node : osm.children("node")) {
        int64_t id = node.attribute("id").as_llong();
        auto it = osm_nodes_raw.find(id);
        if (it == osm_nodes_raw.end()) continue;
        it->second.lat = node.attribute("lat").as_double();
        it->second.lon = node.attribute("lon").as_double();
    }

    // -----------------------------------------------------------------------
    // Build dense ID mapping
    // -----------------------------------------------------------------------
    uint32_t dense_id = 0;
    for (auto& [osm_id, coord] : osm_nodes_raw)
        node_map_[osm_id] = dense_id++;

    uint32_t N = dense_id;
    uint32_t E_estimate = 0;
    for (auto& w : ways) E_estimate += (w.node_ids.size() - 1) * (w.one_way ? 1 : 2);

    graph.reserve(N, E_estimate);
    graph.coords.resize(N);
    for (auto& [osm_id, coord] : osm_nodes_raw)
        graph.coords[node_map_[osm_id]] = coord;

    // -----------------------------------------------------------------------
    // Pass 3: Insert edges
    // -----------------------------------------------------------------------
    for (auto& w : ways) {
        for (size_t i = 0; i + 1 < w.node_ids.size(); ++i) {
            auto it_u = node_map_.find(w.node_ids[i]);
            auto it_v = node_map_.find(w.node_ids[i + 1]);
            if (it_u == node_map_.end() || it_v == node_map_.end()) continue;

            uint32_t u = it_u->second, v = it_v->second;
            auto& cu = graph.coords[u];
            auto& cv = graph.coords[v];
            uint32_t dist_mm = haversine_mm(cu.lat, cu.lon, cv.lat, cv.lon);

            uint32_t weight;
            if (weight_mode == "time") {
                // Travel time in milliseconds: dist_mm / (speed_kmh * 1000/3600)
                // = dist_mm * 3600 / (speed_kmh * 1000)  [ms]
                weight = static_cast<uint32_t>(
                    static_cast<uint64_t>(dist_mm) * 3600 / (w.speed_kmh * 1000));
            } else {
                weight = dist_mm;
            }

            graph.add_edge(u, v, weight);
            if (!w.one_way)
                graph.add_edge(v, u, weight);
        }
    }

    graph.finalize();
    std::cout << "[Parser] Nodes: " << N << "  Edges: " << graph.num_edges() << "\n";
}