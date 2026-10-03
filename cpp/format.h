// format.h - the output contract from Section 2.3 of the planning document.
//
// Every byte printed on stdout by either implementation is produced here or by
// the Python mirror of this file. Nothing else in the program writes to
// stdout. Keeping the contract in one place is what makes the acceptance diff
// meaningful, because a drift in formatting is then a single-file change
// rather than a hunt through the command handlers.
//
// Row:       date(10) SP amount(>10.2f) SP SP category(<12) SP description
// Summary:   category(<12) amount(>12.2f)
// Separator: 24 hyphens
// Total:     "TOTAL"(<12) amount(>12.2f)
//
// Trailing whitespace is stripped from every line.
#ifndef FORMAT_H
#define FORMAT_H

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>

#include "expense.h"

namespace et {

inline std::string rstrip(const std::string& s) {
    std::size_t e = s.find_last_not_of(" \t");
    if (e == std::string::npos) return "";
    return s.substr(0, e + 1);
}

inline std::string formatRow(const Expense& e) {
    std::ostringstream os;
    os << std::left << std::setw(10) << e.date.toString() << ' '
       << std::right << std::setw(10) << std::fixed << std::setprecision(2) << e.amount
       << "  " << std::left << std::setw(12) << e.category << ' ' << e.description;
    return rstrip(os.str());
}

inline std::string formatSummaryLine(const std::string& label, double amount) {
    std::ostringstream os;
    os << std::left << std::setw(12) << label << std::right << std::setw(12)
       << std::fixed << std::setprecision(2) << amount;
    return rstrip(os.str());
}

inline std::string summarySeparator() { return std::string(24, '-'); }

}  // namespace et

#endif  // FORMAT_H
