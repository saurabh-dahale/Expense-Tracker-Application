// storage.h - persistence, and the vector growth trace.
//
// The error model here is the one chosen on Day 1. A file that cannot be
// opened throws StorageError, because no caller can recover from it. A row
// that does not parse returns nullopt from parseExpense and is counted as
// skipped, because the rest of the file is still usable. Python expresses both
// of those with the same mechanism, which is the comparison the error-handling
// section is built on.
#ifndef STORAGE_H
#define STORAGE_H

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "expense.h"

namespace et {

class StorageError : public std::runtime_error {
public:
    explicit StorageError(const std::string& what) : std::runtime_error(what) {}
};

struct LoadResult {
    std::vector<Expense> expenses;
    int skipped = 0;
    // Each entry is a capacity the vector grew to while loading. Reported on
    // stderr under --diag. Python's list also over-allocates, but because a
    // Python list holds references, a growth there never touches the elements
    // and never invalidates anything the caller is holding.
    std::vector<std::size_t> capacityTrace;
};

inline LoadResult load(const std::string& path) {
    std::ifstream in(path);  // closed by ~ifstream on every exit path
    if (!in) throw StorageError("cannot open " + path);

    LoadResult result;
    std::string line;
    bool first = true;
    std::size_t lastCapacity = 0;

    while (std::getline(in, line)) {
        if (first) {
            first = false;
            if (trim(line).rfind("date,", 0) == 0) continue;  // header row
        }
        if (trim(line).empty()) continue;

        std::optional<Expense> e = parseExpense(line);
        if (!e) {
            ++result.skipped;
            continue;
        }
        result.expenses.push_back(std::move(*e));
        if (result.expenses.capacity() != lastCapacity) {
            lastCapacity = result.expenses.capacity();
            result.capacityTrace.push_back(lastCapacity);
        }
    }
    return result;
}

inline void save(const std::string& path, const std::vector<Expense>& expenses) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) throw StorageError("cannot write " + path);
    out << "date,amount,category,description\n";
    for (const Expense& e : expenses) out << toCsv(e) << '\n';
    if (!out) throw StorageError("write failed for " + path);
}

}  // namespace et

#endif  // STORAGE_H
