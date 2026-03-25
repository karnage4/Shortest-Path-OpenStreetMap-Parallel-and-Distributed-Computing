cmake_minimum_required(VERSION 3.18)

# Must be BEFORE project() — tells CMake to find vcpkg libraries
set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(osm_routing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -O3 and -march=native are GCC/Clang flags, they don't work on MSVC.
# This sets the equivalent optimization flags depending on the compiler.
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
endif()

find_package(pugixml CONFIG REQUIRED)

add_executable(osm_routing
    src/main.cpp
    src/osm_parser.cpp
    src/dijkstra.cpp
    src/graph.cpp          # <-- was missing from original
)
target_include_directories(osm_routing PRIVATE src)
target_link_libraries(osm_routing PRIVATE pugixml::pugixml)

add_executable(osm_bench
    src/main.cpp
    src/osm_parser.cpp
    src/dijkstra.cpp
    src/graph.cpp          # <-- was missing from original
)
target_include_directories(osm_bench PRIVATE src)
target_link_libraries(osm_bench PRIVATE pugixml::pugixml)
target_compile_definitions(osm_bench PRIVATE BENCHMARK_MODE)