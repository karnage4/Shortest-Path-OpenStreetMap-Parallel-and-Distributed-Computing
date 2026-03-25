#pragma once
#include <chrono>
#include <string>
#include <iostream>

// ---------------------------------------------------------------------------
// RAII-style wall-clock timer. Prints elapsed time on destruction.
// Usage:
//   { Timer t("Dijkstra");  dijkstra(g, src); }  // prints on scope exit
// ---------------------------------------------------------------------------
struct Timer {
    std::string label;
    std::chrono::steady_clock::time_point start;

    explicit Timer(std::string lbl)
        : label(std::move(lbl))
        , start(std::chrono::steady_clock::now()) {}

    ~Timer() {
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "[" << label << "] " << ms << " ms\n";
    }

    double elapsed_ms() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(now - start).count();
    }
};