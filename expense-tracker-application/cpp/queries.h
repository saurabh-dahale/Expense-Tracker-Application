// queries.h - filter and search.
//
// Both return a new vector built with std::copy_if and a lambda, which is the
// direct counterpart of a Python list comprehension. The difference the report
// cares about is not the syntax but what lands in the result: copy_if copies
// each matching Expense into the new vector, while the comprehension stores
// another reference to the same object. The copy counters make that visible.
#ifndef QUERIES_H
#define QUERIES_H

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "expense.h"

namespace et {

struct FilterCriteria {
    std::optional<Date> from;      // inclusive
    std::optional<Date> to;        // inclusive
    std::optional<std::string> category;

    bool matches(const Expense& e) const {
        if (from && e.date < *from) return false;
        if (to && e.date > *to) return false;
        if (category && e.category != *category) return false;
        return true;
    }
};

inline std::vector<Expense> filter(const std::vector<Expense>& all,
                                   const FilterCriteria& c) {
    std::vector<Expense> out;
    std::copy_if(all.begin(), all.end(), std::back_inserter(out),
                 [&c](const Expense& e) { return c.matches(e); });
    return out;
}

inline std::vector<Expense> search(const std::vector<Expense>& all,
                                   const std::string& text) {
    const std::string needle = toLower(text);
    std::vector<Expense> out;
    std::copy_if(all.begin(), all.end(), std::back_inserter(out),
                 [&needle](const Expense& e) {
                     return toLower(e.description).find(needle) != std::string::npos;
                 });
    return out;
}

}  // namespace et

#endif  // QUERIES_H
