// bench.cpp - timing for summary versus summary --parallel.
//
// The command-line program cannot show the concurrency result, because on a
// large file nearly all of its run time is spent loading the CSV. This loads
// the file once, untimed, and then times only summarize and summarizeParallel
// with std::chrono::steady_clock. python/bench.py is the mirror of this file
// and prints the same table, so the two can be read side by side.
//
// Every parallel result is checked against the sequential one, and a mismatch
// exits with status 1, so a timing is never reported for a wrong answer.
//
// Usage: run_bench [--file PATH] [--runs N] [--threads 1,2,4,8]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "expense.h"
#include "storage.h"
#include "summary.h"

namespace et {
Expense::Counters Expense::counters;
}

namespace {

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

struct Stats {
    double best = 0.0;
    double median = 0.0;
};

// One untimed warm-up run, then `runs` timed ones. The best run is the least
// disturbed by the scheduler and the median shows how typical it is.
template <typename F>
Stats timeIt(int runs, F&& f) {
    f();
    std::vector<double> samples;
    for (int i = 0; i < runs; ++i) {
        Clock::time_point start = Clock::now();
        f();
        samples.push_back(msSince(start));
    }
    std::sort(samples.begin(), samples.end());
    return {samples.front(), samples[samples.size() / 2]};
}

void printRow(const std::string& label, const Stats& s, double baseline) {
    std::printf("%-14s %10.2f %10.2f %8.2fx\n", label.c_str(), s.best, s.median,
                baseline / s.best);
}

}  // namespace

int main(int argc, char** argv) {
    std::string path = "../spec/expenses_large.csv";
    int runs = 5;
    std::vector<unsigned> threads = {1, 2, 4, 8};

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--file" || a == "--runs" || a == "--threads") && i + 1 >= argc) {
            std::cerr << "Error: " << a << " requires a value\n";
            return 1;
        }
        if (a == "--file") {
            path = argv[++i];
        } else if (a == "--runs") {
            runs = std::stoi(argv[++i]);
            if (runs < 1) {
                std::cerr << "Error: --runs must be at least 1\n";
                return 1;
            }
        } else if (a == "--threads") {
            threads.clear();
            std::istringstream list(argv[++i]);
            std::string item;
            while (std::getline(list, item, ',')) {
                int n = std::stoi(item);
                if (n < 1) {
                    std::cerr << "Error: thread counts must be at least 1\n";
                    return 1;
                }
                threads.push_back(static_cast<unsigned>(n));
            }
        } else {
            std::cerr << "Error: unknown option: " << a << '\n';
            return 1;
        }
    }

    Clock::time_point loadStart = Clock::now();
    et::LoadResult loaded;
    try {
        loaded = et::load(path);
    } catch (const et::StorageError& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    const double loadMs = msSince(loadStart);
    const std::vector<et::Expense>& all = loaded.expenses;

    std::printf("language       C++ (std::thread)\n");
    std::printf("file           %s\n", path.c_str());
    std::printf("rows           %zu (skipped %d)\n", all.size(), loaded.skipped);
    std::printf("hardware       %u logical cores\n", std::thread::hardware_concurrency());
    std::printf("load           %.2f ms (not part of the timings below)\n", loadMs);
    std::printf("runs           %d timed, after 1 warm-up\n\n", runs);
    std::printf("%-14s %10s %10s %9s\n", "mode", "best ms", "median ms", "speedup");

    const et::Totals expected = et::summarize(all);
    const Stats seq = timeIt(runs, [&] { et::summarize(all); });
    printRow("sequential", seq, seq.best);

    for (unsigned n : threads) {
        const et::Totals got = et::summarizeParallel(all, n);
        if (got.byCategory != expected.byCategory || got.overall != expected.overall) {
            std::cerr << "Error: parallel result with " << n
                      << " threads differs from sequential\n";
            return 1;
        }
        const Stats par = timeIt(runs, [&] { et::summarizeParallel(all, n); });
        printRow("parallel x" + std::to_string(n), par, seq.best);
    }
    return 0;
}
