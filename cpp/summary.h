// summary.h - aggregation, sequential and concurrent.
//
// Totals accumulate in integer cents rather than in double. That is not
// incidental tidiness. Floating-point addition is not associative, so the
// chunked order used by the concurrent path can differ from the sequential
// order in the last bit, which would break the byte-identical output contract
// on a large enough fixture. Integer addition is associative, so the two paths
// agree by construction. The Python implementation has to make the same choice
// for the same reason, which makes this a shared constraint rather than a
// language difference, and the report should say so.
//
// std::map is deliberate as well. It keeps categories in sorted order, which
// the output contract requires, where Python's dict preserves insertion order
// and has to be sorted explicitly at print time.
#ifndef SUMMARY_H
#define SUMMARY_H

#include <cmath>
#include <map>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "expense.h"

namespace et {

struct Totals {
    std::map<std::string, long long> byCategory;  // cents, sorted by category
    long long overall = 0;                        // cents
};

inline long long toCents(double amount) {
    return static_cast<long long>(std::llround(amount * 100.0));
}

inline Totals summarize(const std::vector<Expense>& all) {
    Totals t;
    for (const Expense& e : all) t.byCategory[e.category] += toCents(e.amount);
    t.overall = std::accumulate(
        t.byCategory.begin(), t.byCategory.end(), 0LL,
        [](long long acc, const std::pair<const std::string, long long>& kv) {
            return acc + kv.second;
        });
    return t;
}

// Partitions the expense list into one chunk per worker, accumulates each
// chunk into a thread-local map, then merges the partials under a mutex. The
// per-thread maps are the allocation cost of this design and are worth
// measuring: Python's equivalent needs no partials at all, because the global
// interpreter lock already serializes the updates, which is also why it will
// show no speedup.
inline Totals summarizeParallel(const std::vector<Expense>& all,
                                unsigned requestedThreads = 0) {
    unsigned workers = requestedThreads;
    if (workers == 0) {
        workers = std::thread::hardware_concurrency();
        if (workers == 0) workers = 2;
    }
    if (all.size() < workers) workers = static_cast<unsigned>(all.size());
    if (workers <= 1) return summarize(all);

    std::map<std::string, long long> shared;
    std::mutex mu;
    std::vector<std::thread> pool;
    pool.reserve(workers);

    const std::size_t chunk = (all.size() + workers - 1) / workers;

    for (unsigned w = 0; w < workers; ++w) {
        const std::size_t begin = w * chunk;
        if (begin >= all.size()) break;
        const std::size_t end = std::min(begin + chunk, all.size());

        pool.emplace_back([&all, &shared, &mu, begin, end]() {
            std::map<std::string, long long> local;
            for (std::size_t i = begin; i < end; ++i) {
                local[all[i].category] += toCents(all[i].amount);
            }
            std::lock_guard<std::mutex> guard(mu);
            for (const std::pair<const std::string, long long>& kv : local) {
                shared[kv.first] += kv.second;
            }
        });
    }
    for (std::thread& t : pool) t.join();

    Totals t;
    t.byCategory = std::move(shared);
    t.overall = std::accumulate(
        t.byCategory.begin(), t.byCategory.end(), 0LL,
        [](long long acc, const std::pair<const std::string, long long>& kv) {
            return acc + kv.second;
        });
    return t;
}

}  // namespace et

#endif  // SUMMARY_H
