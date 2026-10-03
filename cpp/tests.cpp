// tests.cpp - unit tests, plain assertions, no framework.
//
// A C++ test framework would add build complexity disproportionate to a
// three-day project, so this is a separate target that exits non-zero on the
// first failure. The cases are the ones named in Section 6 of the planning
// document: date boundaries on an inclusive range, a category with no matches,
// a malformed row, case-insensitive search, and parallel agreeing with
// sequential.
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "expense.h"
#include "format.h"
#include "queries.h"
#include "summary.h"

namespace et {
Expense::Counters Expense::counters;
}

namespace {

int failures = 0;

void check(bool condition, const std::string& label) {
    if (condition) {
        std::cout << "  pass  " << label << '\n';
    } else {
        std::cout << "  FAIL  " << label << '\n';
        ++failures;
    }
}

et::Expense make(const std::string& date, const std::string& amount,
                 const std::string& category, const std::string& description) {
    std::optional<et::Expense> e =
        et::parseExpense(date + "," + amount + "," + category + "," + description);
    assert(e.has_value());
    return *e;
}

std::vector<et::Expense> fixture() {
    return {
        make("2026-09-01", "42.50", "groceries", "Weekly shop"),
        make("2026-09-03", "12.00", "transport", "Bus pass top-up"),
        make("2026-09-07", "89.99", "utilities", "Electricity bill"),
        make("2026-09-12", "23.75", "groceries", "Produce market"),
        make("2026-09-15", "150.00", "rent", "Parking space"),
    };
}

void testDateParsing() {
    std::cout << "date parsing\n";
    check(et::Date::parse("2026-09-01").has_value(), "accepts a valid date");
    check(!et::Date::parse("2026-02-30").has_value(), "rejects February 30");
    check(et::Date::parse("2024-02-29").has_value(), "accepts a leap day");
    check(!et::Date::parse("2026-02-29").has_value(), "rejects a non-leap February 29");
    check(!et::Date::parse("2026-13-01").has_value(), "rejects month 13");
    check(!et::Date::parse("2026-9-01").has_value(), "rejects an unpadded month");
    check(!et::Date::parse("20260901").has_value(), "rejects a missing separator");
}

void testRowParsing() {
    std::cout << "row parsing\n";
    check(!et::parseExpense("2026-09-01,42.5,groceries,Weekly shop").has_value(),
          "rejects an amount with one decimal place");
    check(!et::parseExpense("2026-09-01,-5.00,groceries,Refund").has_value(),
          "rejects a negative amount");
    check(!et::parseExpense("2026-09-01,42.50,Groceries,Weekly shop").has_value(),
          "rejects a non-lowercase category");
    check(!et::parseExpense("2026-09-01,42.50,groceries").has_value(),
          "rejects a row with too few fields");
    check(!et::parseExpense("").has_value(), "rejects an empty line");

    std::optional<et::Expense> ok =
        et::parseExpense("2026-09-01,42.50,groceries,Weekly shop at the market");
    check(ok.has_value(), "accepts a description containing spaces");
    check(ok && ok->description == "Weekly shop at the market",
          "keeps the whole description");
}

void testFilterBoundaries() {
    std::cout << "filter boundaries\n";
    std::vector<et::Expense> all = fixture();

    et::FilterCriteria inclusive;
    inclusive.from = et::Date::parse("2026-09-03");
    inclusive.to = et::Date::parse("2026-09-12");
    check(et::filter(all, inclusive).size() == 3, "range endpoints are inclusive");

    et::FilterCriteria single;
    single.from = et::Date::parse("2026-09-07");
    single.to = et::Date::parse("2026-09-07");
    check(et::filter(all, single).size() == 1, "a single-day range matches that day");

    et::FilterCriteria none;
    none.category = std::string("nosuch");
    check(et::filter(all, none).empty(), "an unmatched category yields no rows");

    et::FilterCriteria both;
    both.from = et::Date::parse("2026-09-01");
    both.to = et::Date::parse("2026-09-07");
    both.category = std::string("groceries");
    check(et::filter(all, both).size() == 1, "criteria combine with AND");

    check(et::filter(all, et::FilterCriteria{}).size() == all.size(),
          "no criteria matches everything");
}

void testSearch() {
    std::cout << "search\n";
    std::vector<et::Expense> all = fixture();
    check(et::search(all, "BUS").size() == 1, "search is case insensitive");
    check(et::search(all, "bus").size() == 1, "search matches lowercase");
    check(et::search(all, "o").size() == 3, "search matches a substring anywhere");
    check(et::search(all, "xyzzy").empty(), "an unmatched term yields no rows");
}

void testSummary() {
    std::cout << "summary\n";
    std::vector<et::Expense> all = fixture();
    et::Totals t = et::summarize(all);

    check(t.byCategory.size() == 4, "one entry per distinct category");
    check(t.byCategory.at("groceries") == 6625, "groceries totals 66.25");
    check(t.overall == 31824, "overall totals 318.24");

    std::vector<std::string> keys;
    for (const std::pair<const std::string, long long>& kv : t.byCategory) {
        keys.push_back(kv.first);
    }
    check(keys == std::vector<std::string>({"groceries", "rent", "transport", "utilities"}),
          "categories come out alphabetically");

    check(et::summarize({}).overall == 0, "an empty list totals zero");
}

void testParallelMatchesSequential() {
    std::cout << "parallel summary\n";
    std::vector<et::Expense> all = fixture();

    // Enlarged so the work actually splits across workers.
    std::vector<et::Expense> big;
    for (int i = 0; i < 2000; ++i) {
        for (const et::Expense& e : all) big.push_back(e);
    }

    et::Totals sequential = et::summarize(big);
    for (unsigned workers : {1u, 2u, 3u, 4u, 8u}) {
        et::Totals parallel = et::summarizeParallel(big, workers);
        check(parallel.byCategory == sequential.byCategory,
              "per-category totals match with " + std::to_string(workers) + " workers");
        check(parallel.overall == sequential.overall,
              "overall total matches with " + std::to_string(workers) + " workers");
    }
    check(et::summarizeParallel(all, 8).overall == et::summarize(all).overall,
          "more workers than records is handled");
}

void testFormatting() {
    std::cout << "formatting\n";
    et::Expense e = make("2026-09-01", "42.50", "groceries", "Weekly shop");
    const std::string row = et::formatRow(e);
    check(row == "2026-09-01      42.50  groceries    Weekly shop",
          "row matches the output contract");
    check(row.find_last_not_of(' ') == row.size() - 1, "no trailing whitespace");
    check(et::formatSummaryLine("TOTAL", 318.24) == "TOTAL             318.24",
          "summary line matches the output contract");
    check(et::summarySeparator().size() == 24, "separator is 24 characters");
}

}  // namespace

int main() {
    testDateParsing();
    testRowParsing();
    testFilterBoundaries();
    testSearch();
    testSummary();
    testParallelMatchesSequential();
    testFormatting();

    std::cout << '\n';
    if (failures == 0) {
        std::cout << "all tests passed\n";
        return 0;
    }
    std::cout << failures << " test(s) failed\n";
    return 1;
}
