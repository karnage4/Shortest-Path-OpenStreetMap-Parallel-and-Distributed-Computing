cmake_minimum_required(VERSION 3.18)
project(osm_routing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

find_package(pugixml REQUIRED)
find_package(OpenMP REQUIRED)

# Milestone 1
add_executable(osm_routing
    src/main.cpp src/osm_parser.cpp src/dijkstra.cpp src/graph.cpp)
target_include_directories(osm_routing PRIVATE src)
target_link_libraries(osm_routing PRIVATE pugixml::pugixml)

add_executable(osm_bench
    src/main.cpp src/osm_parser.cpp src/dijkstra.cpp src/graph.cpp)
target_include_directories(osm_bench PRIVATE src)
target_link_libraries(osm_bench PRIVATE pugixml::pugixml)
target_compile_definitions(osm_bench PRIVATE BENCHMARK_MODE)

# Milestone 2
add_executable(osm_parallel
    src/parallel_main.cpp
    src/osm_parser.cpp
    src/dijkstra.cpp
    src/graph.cpp
    src/delta_stepping.cpp
    src/bidirectional_dijkstra.cpp
    src/parallel_dijkstra.cpp)
target_include_directories(osm_parallel PRIVATE src)
target_link_libraries(osm_parallel PRIVATE pugixml::pugixml OpenMP::OpenMP_CXX)
