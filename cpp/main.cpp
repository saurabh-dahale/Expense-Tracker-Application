// main.cpp - command loop for the C++ expense tracker.
//
// Usage: expense_tracker [--file PATH] [--diag]
//
// Commands are read from stdin, one per line, so the same scripted sequence in
// spec/commands.txt drives both implementations and the acceptance test is a
// diff of their stdout. Diagnostics requested with --diag go to stderr only,
// which is why the diff harness compares stdout alone.
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "expense.h"
#include "format.h"
#include "queries.h"
#include "storage.h"
#include "summary.h"

namespace et {
Expense::Counters Expense::counters;
}

namespace {

const char* const kHelp =
    "Commands:\n"
    "  add DATE AMOUNT CATEGORY DESCRIPTION   append one expense and persist it\n"
    "  list                                   print all expenses in file order\n"
    "  filter [--from D] [--to D] [--category C]  print matching expenses\n"
    "  search TEXT                            print expenses whose description matches\n"
    "  summary [--parallel]                   print per-category totals, then the total\n"
    "  help                                   print this list\n"
    "  quit                                   exit";

void fail(const std::string& message) {
    std::cerr << "Error: " << message << '\n';
}

void printRows(const std::vector<et::Expense>& rows) {
    for (const et::Expense& e : rows) std::cout << et::formatRow(e) << '\n';
}

void printTotals(const et::Totals& t) {
    for (const std::pair<const std::string, long long>& kv : t.byCategory) {
        std::cout << et::formatSummaryLine(kv.first, static_cast<double>(kv.second) / 100.0)
                  << '\n';
    }
    std::cout << et::summarySeparator() << '\n';
    std::cout << et::formatSummaryLine("TOTAL", static_cast<double>(t.overall) / 100.0)
              << '\n';
}

// add DATE AMOUNT CATEGORY DESCRIPTION... (description is the rest of the line)
void handleAdd(std::istringstream& args, std::vector<et::Expense>& expenses,
               const std::string& path) {
    std::string dateTok, amountTok, categoryTok;
    if (!(args >> dateTok >> amountTok >> categoryTok)) {
        fail("add requires DATE AMOUNT CATEGORY DESCRIPTION");
        return;
    }
    std::string rest;
    std::getline(args, rest);
    rest = et::trim(rest);
    if (rest.empty()) {
        fail("add requires a description");
        return;
    }

    const std::string row = dateTok + "," + amountTok + "," + categoryTok + "," + rest;
    std::optional<et::Expense> e = et::parseExpense(row);
    if (!e) {
        fail("could not parse expense: " + row);
        return;
    }
    expenses.push_back(std::move(*e));
    et::save(path, expenses);
}

void handleFilter(std::istringstream& args, const std::vector<et::Expense>& expenses) {
    et::FilterCriteria c;
    std::string flag;
    while (args >> flag) {
        std::string value;
        if (!(args >> value)) {
            fail(flag + " requires a value");
            return;
        }
        if (flag == "--from" || flag == "--to") {
            std::optional<et::Date> d = et::Date::parse(value);
            if (!d) {
                fail("invalid date: " + value);
                return;
            }
            if (flag == "--from") c.from = d; else c.to = d;
        } else if (flag == "--category") {
            c.category = value;
        } else {
            fail("unknown filter flag: " + flag);
            return;
        }
    }
    if (c.from && c.to && *c.to < *c.from) {
        fail("--to is before --from");
        return;
    }
    printRows(et::filter(expenses, c));
}

void reportDiagnostics(const et::LoadResult& loaded, std::size_t finalSize) {
    const et::Expense::Counters& c = et::Expense::counters;
    std::cerr << "[diag] sizeof(Expense)      = " << sizeof(et::Expense) << " bytes\n";
    std::cerr << "[diag] records held         = " << finalSize << '\n';
    std::cerr << "[diag] rows skipped on load = " << loaded.skipped << '\n';
    std::cerr << "[diag] vector capacity path =";
    for (std::size_t cap : loaded.capacityTrace) std::cerr << ' ' << cap;
    std::cerr << '\n';
    std::cerr << "[diag] Expense copy ctor    = " << c.copyCtor << '\n';
    std::cerr << "[diag] Expense move ctor    = " << c.moveCtor << '\n';
    std::cerr << "[diag] Expense copy assign  = " << c.copyAssign << '\n';
    std::cerr << "[diag] Expense move assign  = " << c.moveAssign << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    std::string path = "spec/expenses.csv";
    bool diag = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--diag") {
            diag = true;
        } else if (a == "--file") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --file requires a path\n";
                return 1;
            }
            path = argv[++i];
        } else {
            std::cerr << "Error: unknown option: " << a << '\n';
            return 1;
        }
    }

    et::LoadResult loaded;
    try {
        loaded = et::load(path);
    } catch (const et::StorageError& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    std::vector<et::Expense> expenses = std::move(loaded.expenses);

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(et::trim(line));
        std::string cmd;
        if (!(in >> cmd)) continue;

        try {
            if (cmd == "quit") {
                break;
            } else if (cmd == "help") {
                std::cout << kHelp << '\n';
            } else if (cmd == "list") {
                printRows(expenses);
            } else if (cmd == "add") {
                handleAdd(in, expenses, path);
            } else if (cmd == "filter") {
                handleFilter(in, expenses);
            } else if (cmd == "search") {
                std::string text;
                std::getline(in, text);
                text = et::trim(text);
                if (text.empty()) {
                    fail("search requires text");
                } else {
                    printRows(et::search(expenses, text));
                }
            } else if (cmd == "summary") {
                std::string flag;
                if (in >> flag) {
                    if (flag == "--parallel") {
                        printTotals(et::summarizeParallel(expenses));
                    } else {
                        fail("unknown summary flag: " + flag);
                    }
                } else {
                    printTotals(et::summarize(expenses));
                }
            } else {
                fail("unknown command: " + cmd);
            }
        } catch (const et::StorageError& e) {
            std::cerr << "Error: " << e.what() << '\n';
            return 1;
        }
    }

    if (diag) reportDiagnostics(loaded, expenses.size());
    return 0;
}
