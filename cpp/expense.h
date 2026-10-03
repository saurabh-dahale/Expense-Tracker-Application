// expense.h - domain model for the cross-language expense tracker.
//
// Two things in this file exist for the Day 3 analysis rather than for the
// application:
//
//   1. Date is a hand-rolled comparable value type. Python gets the same
//      capability from one datetime.strptime call. The gap between the two is
//      the standard-library-breadth evidence for the writability section.
//
//   2. Expense declares all five special member functions so that each copy
//      and each move can be counted. Python never copies a record at all,
//      because its containers hold references. Those counters are the
//      concrete evidence for the memory-management section.
#ifndef EXPENSE_H
#define EXPENSE_H

#include <array>
#include <cctype>
#include <cstdio>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace et {

// ---------------------------------------------------------------- Date

struct Date {
    int year = 0;
    int month = 0;
    int day = 0;

    static bool isLeap(int y) {
        return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    }

    static int daysInMonth(int y, int m) {
        static const std::array<int, 12> kDays = {31, 28, 31, 30, 31, 30,
                                                  31, 31, 30, 31, 30, 31};
        if (m < 1 || m > 12) return 0;
        if (m == 2 && isLeap(y)) return 29;
        return kDays[static_cast<std::size_t>(m - 1)];
    }

    // Accepts exactly YYYY-MM-DD and validates the calendar, so 2026-02-30 is
    // rejected rather than silently accepted the way a plain string compare
    // would accept it.
    static std::optional<Date> parse(const std::string& s) {
        if (s.size() != 10 || s[4] != '-' || s[7] != '-') return std::nullopt;
        for (std::size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u}) {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
        }
        Date d;
        d.year = std::stoi(s.substr(0, 4));
        d.month = std::stoi(s.substr(5, 2));
        d.day = std::stoi(s.substr(8, 2));
        if (d.month < 1 || d.month > 12) return std::nullopt;
        if (d.day < 1 || d.day > daysInMonth(d.year, d.month)) return std::nullopt;
        return d;
    }

    std::string toString() const {
        char buf[11];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
        return std::string(buf);
    }

    bool operator<(const Date& o) const {
        return std::tie(year, month, day) < std::tie(o.year, o.month, o.day);
    }
    bool operator>(const Date& o) const { return o < *this; }
    bool operator<=(const Date& o) const { return !(o < *this); }
    bool operator>=(const Date& o) const { return !(*this < o); }
    bool operator==(const Date& o) const {
        return std::tie(year, month, day) == std::tie(o.year, o.month, o.day);
    }
    bool operator!=(const Date& o) const { return !(*this == o); }
};

// ---------------------------------------------------------------- Expense

struct Expense {
    Date date;
    double amount = 0.0;
    std::string category;
    std::string description;

    // Instrumentation for the memory-management analysis. These are process
    // wide and are reported on stderr under --diag, never on stdout, so the
    // byte-identical output contract in Section 2.3 is unaffected.
    struct Counters {
        long copyCtor = 0;
        long moveCtor = 0;
        long copyAssign = 0;
        long moveAssign = 0;
    };
    static Counters counters;

    Expense() = default;

    Expense(Date d, double a, std::string c, std::string desc)
        : date(d), amount(a), category(std::move(c)), description(std::move(desc)) {}

    // Rule of five, written out rather than defaulted so each path can be
    // counted. Declaring the copy operations is also what makes C++ force the
    // move operations to be declared explicitly, which is itself worth a
    // sentence in the report.
    Expense(const Expense& o)
        : date(o.date), amount(o.amount), category(o.category),
          description(o.description) {
        ++counters.copyCtor;
    }

    Expense(Expense&& o) noexcept
        : date(o.date), amount(o.amount), category(std::move(o.category)),
          description(std::move(o.description)) {
        ++counters.moveCtor;
    }

    Expense& operator=(const Expense& o) {
        if (this != &o) {
            date = o.date;
            amount = o.amount;
            category = o.category;
            description = o.description;
            ++counters.copyAssign;
        }
        return *this;
    }

    Expense& operator=(Expense&& o) noexcept {
        if (this != &o) {
            date = o.date;
            amount = o.amount;
            category = std::move(o.category);
            description = std::move(o.description);
            ++counters.moveAssign;
        }
        return *this;
    }

    ~Expense() = default;
};

// ---------------------------------------------------------------- helpers

inline std::string toLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

inline std::string trim(const std::string& s) {
    std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Amount must be a non-negative decimal with exactly two places, per the
// shared specification. Python's float() would accept "1e3" and ".5"; this
// rejects both, which keeps the two implementations in agreement.
inline std::optional<double> parseAmount(const std::string& s) {
    if (s.empty()) return std::nullopt;
    std::size_t dot = s.find('.');
    if (dot == std::string::npos || s.size() - dot != 3) return std::nullopt;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i == dot) continue;
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return std::nullopt;
    }
    if (dot == 0) return std::nullopt;
    return std::stod(s);
}

// Splits on commas with no quoting support, which the specification allows
// because descriptions may contain spaces but never commas.
inline std::vector<std::string> splitCsv(const std::string& line, std::size_t expected) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (out.size() + 1 < expected) {
        std::size_t pos = line.find(',', start);
        if (pos == std::string::npos) break;
        out.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    out.push_back(line.substr(start));
    return out;
}

// Returns nullopt for a malformed row rather than throwing. A missing or
// unreadable file throws instead. That split is the deliberate error-handling
// choice recorded in the Day 1 plan: recoverable per-row faults are visible in
// the type, unrecoverable I/O faults are not.
inline std::optional<Expense> parseExpense(const std::string& rawLine) {
    std::string line = trim(rawLine);
    if (line.empty()) return std::nullopt;

    std::vector<std::string> f = splitCsv(line, 4);
    if (f.size() != 4) return std::nullopt;

    std::optional<Date> d = Date::parse(trim(f[0]));
    if (!d) return std::nullopt;

    std::optional<double> a = parseAmount(trim(f[1]));
    if (!a) return std::nullopt;

    std::string cat = trim(f[2]);
    if (cat.empty()) return std::nullopt;
    for (char c : cat) {
        if (!std::islower(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
            return std::nullopt;
        }
    }

    std::string desc = trim(f[3]);
    if (desc.empty()) return std::nullopt;

    return Expense(*d, *a, std::move(cat), std::move(desc));
}

inline std::string toCsv(const Expense& e) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", e.amount);
    return e.date.toString() + "," + buf + "," + e.category + "," + e.description;
}

}  // namespace et

#endif  // EXPENSE_H
