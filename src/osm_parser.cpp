#include "osm_parser.hpp"
#include <pugixml.hpp>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <cstring>  // strcmp

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

// ---------------------------------------------------------------------------
// Streaming two-pass parser using pugixml's SAX-style xml_reader.
//
// The OSM format always has <node> elements BEFORE <way> elements in the file,
// but our graph-build logic needs to know which nodes are referenced by ways
// before we can assign dense IDs. We therefore do two passes over the file:
//
//   Pass 1 — scan only <way> elements:
//             • determine routable ways (highway tag with nonzero speed)
//             • collect the set of OSM node IDs referenced by those ways
//             • store WayInfo (node id list, oneway flag, speed) for pass 3
//
//   Pass 2 — scan only <node> elements:
//             • look up each node ID in the referenced-set from pass 1
//             • store lat/lon for matched nodes, assign dense [0,N) IDs
//
//   Build — iterate stored ways, compute edge weights, populate CSR graph.
//
// Memory usage: O(N_road_nodes + E_road_edges) — never the full XML DOM.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Helper: a tiny SAX handler wrapping pugi::xml_reader.
// pugixml's xml_reader fires callbacks for each element start/end/attribute.
// We subclass xml_reader_writer (the interface is xml_reader) and override
// for_each_node().  The cleaner public interface is pugi::xml_reader — we use
// the callback-style traversal via the pugi SAX event reader.
// ---------------------------------------------------------------------------

// pugixml exposes a streaming reader through pugi::xml_reader.
// The canonical pattern is:
//
//   struct MyHandler {
//       bool for_each_node(const pugi::xml_node&, pugi::xml_parse_status) { ... }
//   };
//   pugi::xml_document doc;
//   doc.load_buffer_inplace_own(...)   // NOT what we want for large files
//
// However, pugixml *also* provides a low-level streaming reader:
//   pugi::xml_parse_result  (returned by load_file / load_buffer)
//   struct xml_reader { virtual bool handle(...) = 0; };   // SAX interface
//
// The correct large-file API is pugi::xml_reader combined with
// pugi::xml_document::traverse(). But that still requires the DOM to be built.
//
// THE REAL solution for huge files: pugi::xml_reader_input + custom reader.
// pugixml 1.11+ exposes this through the sax-like interface via:
//   pugi::xml_document::load(pugi::xml_reader&)    where the reader is fed
//   data in chunks.  BUT this is undocumented and platform-specific.
//
// PRACTICAL ALTERNATIVE (used here):
// Open the file as a binary stream and use pugixml to parse one top-level
// element at a time by feeding it chunks. Since this is complex to implement
// correctly, we instead use the simpler approach of loading the document in
// "streaming" partial mode — pugixml does NOT natively support true streaming
// for arbitrary XML, but for OSM files we can exploit the flat structure:
// all <node> and <way> elements are direct children of <osm>.
//
// For files >2 GB on 64-bit builds, pugixml's load_file works fine because
// it uses 64-bit file offsets. The only limit is AVAILABLE VIRTUAL MEMORY.
// Belgium at 9.1 GB uncompressed XML will require ~9–18 GB RAM for pugixml DOM.
//
// REAL FIX: Use expat or a hand-rolled line reader, OR use osmium/libosmium.
// For this project we implement a lightweight hand-rolled streaming XML parser
// that handles the flat OSM structure (no arbitrary nesting needed):
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Minimal streaming OSM reader.
// OSM XML structure is extremely regular:
//   <osm ...>
//     <node id="..." lat="..." lon="..."> [tags] </node>
//     ...
//     <way id="..."> <nd ref="..."/> ... [tags] </way>
//     ...
//   </osm>
//
// We exploit this by reading line-by-line (OSM exporters always put one
// element per line, or at most a few lines per element). We scan for the
// relevant XML start-tags with simple string operations — no full XML parser
// needed for this well-known schema.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>

// Fast attribute extraction from a raw XML start-tag string.
// Returns true and writes value into `out` if attribute `name` is found.
static bool get_attr(const char* line, const char* name, char* out, size_t out_sz) {
    const char* p = std::strstr(line, name);
    if (!p) return false;
    p += std::strlen(name);
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '=') return false;
    ++p;
    if (*p != '"' && *p != '\'') return false;
    char delim = *p++;
    size_t i = 0;
    while (*p && *p != delim && i + 1 < out_sz)
        out[i++] = *p++;
    out[i] = '\0';
    return i > 0 || *p == delim;
}

void OsmParser::parse(const std::string& filepath, Graph& graph,
                      const std::string& weight_mode) {

    // ------------------------------------------------------------------
    // Pass 1: Collect routable ways + referenced node IDs
    // ------------------------------------------------------------------
    struct WayInfo {
        std::vector<int64_t> node_ids;
        bool     one_way   = false;
        uint32_t speed_kmh = 0;
    };
    std::vector<WayInfo> ways;

    {
        FILE* f = std::fopen(filepath.c_str(), "rb");
        if (!f)
            throw std::runtime_error("Cannot open file: " + filepath);

        static const size_t BUF = 4 * 1024 * 1024; // 4 MB read buffer
        std::vector<char> buf(BUF);

        // We read the file in large chunks and split on '\n'.
        // leftover holds any partial line carried over between chunks.
        std::string leftover;
        leftover.reserve(1024);

        WayInfo cur_way;
        bool in_way      = false;
        bool way_routable= false;
        bool seen_way    = false; // once we see the first <way>, nodes are done

        auto process_line = [&](const char* line) {
            // Skip whitespace
            while (*line == ' ' || *line == '\t') ++line;

            if (!seen_way) {
                // We only care about <way> elements (nodes come before ways in OSM)
                if (line[0] != '<') return;
                if (std::strncmp(line, "<way", 4) == 0) {
                    seen_way = true;
                    in_way = true;
                    way_routable = false;
                    cur_way = WayInfo{};
                } else {
                    return; // still in node section
                }
            }

            if (!in_way) {
                if (std::strncmp(line, "<way", 4) == 0) {
                    in_way = true;
                    way_routable = false;
                    cur_way = WayInfo{};
                }
                return;
            }

            // Inside a <way> element
            char val[256];

            if (std::strncmp(line, "<nd", 3) == 0) {
                if (get_attr(line, "ref", val, sizeof(val)))
                    cur_way.node_ids.push_back(std::atoll(val));

            } else if (std::strncmp(line, "<tag", 4) == 0) {
                char k[128] = {}, v[128] = {};
                get_attr(line, " k", k, sizeof(k));
                get_attr(line, " v", v, sizeof(v));

                if (std::strcmp(k, "highway") == 0) {
                    uint32_t spd = highway_speed(std::string(v));
                    if (spd > 0) { cur_way.speed_kmh = spd; way_routable = true; }
                } else if (std::strcmp(k, "oneway") == 0) {
                    if (std::strcmp(v, "yes") == 0 || std::strcmp(v, "-1") == 0)
                        cur_way.one_way = true;
                } else if (std::strcmp(k, "access") == 0) {
                    if (std::strcmp(v, "no") == 0 || std::strcmp(v, "private") == 0)
                        way_routable = false;
                }

            } else if (std::strncmp(line, "</way>", 6) == 0 ||
                       // self-closing <way .../> with no children (rare)
                       (line[0] == '<' && line[1] == 'w')) {
                // end of way
                in_way = false;
                if (way_routable && cur_way.node_ids.size() >= 2)
                    ways.push_back(std::move(cur_way));
                cur_way = WayInfo{};
            }
        };

        size_t n;
        while ((n = std::fread(buf.data(), 1, BUF, f)) > 0) {
            size_t start = 0;
            for (size_t i = 0; i < n; ++i) {
                if (buf[i] == '\n') {
                    if (!leftover.empty()) {
                        leftover.append(buf.data() + start, i - start);
                        process_line(leftover.c_str());
                        leftover.clear();
                    } else {
                        // Temporarily null-terminate in the buffer
                        char save = buf[i];
                        buf[i] = '\0';
                        process_line(buf.data() + start);
                        buf[i] = save;
                    }
                    start = i + 1;
                }
            }
            // Carry over the partial line
            if (start < n)
                leftover.append(buf.data() + start, n - start);
        }
        if (!leftover.empty())
            process_line(leftover.c_str());

        std::fclose(f);
    }

    // Collect the set of OSM node IDs we actually need
    std::unordered_map<int64_t, Graph::NodeCoord> osm_nodes_raw;
    osm_nodes_raw.reserve(ways.size() * 4); // rough estimate
    for (auto& w : ways)
        for (int64_t id : w.node_ids)
            osm_nodes_raw.emplace(id, Graph::NodeCoord{});

    // ------------------------------------------------------------------
    // Pass 2: Fill lat/lon for referenced nodes only
    // ------------------------------------------------------------------
    {
        FILE* f = std::fopen(filepath.c_str(), "rb");
        if (!f)
            throw std::runtime_error("Cannot open file: " + filepath);

        static const size_t BUF = 4 * 1024 * 1024;
        std::vector<char> buf(BUF);
        std::string leftover;
        leftover.reserve(1024);

        auto process_line = [&](const char* line) {
            while (*line == ' ' || *line == '\t') ++line;
            if (std::strncmp(line, "<node", 5) != 0) return;

            char id_s[32], lat_s[32], lon_s[32];
            if (!get_attr(line, " id", id_s, sizeof(id_s))) return;
            int64_t id = std::atoll(id_s);

            auto it = osm_nodes_raw.find(id);
            if (it == osm_nodes_raw.end()) return;

            if (!get_attr(line, " lat", lat_s, sizeof(lat_s))) return;
            if (!get_attr(line, " lon", lon_s, sizeof(lon_s))) return;
            it->second.lat = std::atof(lat_s);
            it->second.lon = std::atof(lon_s);
        };

        size_t n;
        while ((n = std::fread(buf.data(), 1, BUF, f)) > 0) {
            size_t start = 0;
            for (size_t i = 0; i < n; ++i) {
                if (buf[i] == '\n') {
                    if (!leftover.empty()) {
                        leftover.append(buf.data() + start, i - start);
                        process_line(leftover.c_str());
                        leftover.clear();
                    } else {
                        char save = buf[i];
                        buf[i] = '\0';
                        process_line(buf.data() + start);
                        buf[i] = save;
                    }
                    start = i + 1;
                }
            }
            if (start < n)
                leftover.append(buf.data() + start, n - start);
        }
        if (!leftover.empty())
            process_line(leftover.c_str());

        std::fclose(f);
    }

    // ------------------------------------------------------------------
    // Build dense ID mapping
    // ------------------------------------------------------------------
    uint32_t dense_id = 0;
    for (auto& [osm_id, coord] : osm_nodes_raw)
        node_map_[osm_id] = dense_id++;

    uint32_t N = dense_id;
    uint32_t E_estimate = 0;
    for (auto& w : ways) E_estimate += static_cast<uint32_t>(w.node_ids.size() - 1) * (w.one_way ? 1 : 2);

    graph.reserve(N, E_estimate);
    graph.coords.resize(N);
    for (auto& [osm_id, coord] : osm_nodes_raw)
        graph.coords[node_map_[osm_id]] = coord;

    // ------------------------------------------------------------------
    // Pass 3: Insert edges
    // ------------------------------------------------------------------
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
