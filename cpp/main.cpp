// main.cpp - command loop for the C++ expense tracker.
//
// This file acts as the interactive command-line front end for the expense
// tracker. It is responsible for:
//   * parsing command-line options,
//   * loading the expense data from storage,
//   * reading one command at a time from standard input,
//   * dispatching each command to a handler,
//   * printing results or error messages to stdout/stderr,
//   * and optionally emitting diagnostic counters for performance analysis.
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
// The static counters used to track copy/move behavior are defined here so they
// can be referenced by the main program and by the diagnostic report.
Expense::Counters Expense::counters;
}

namespace {

// Help text printed by the "help" command. It documents the supported commands
// and their flags so users can interact with the program without needing to read
// the source code.
const char* const kHelp =
    "Commands:\n"
    "  add DATE AMOUNT CATEGORY DESCRIPTION   append one expense and persist it\n"
    "  list                                   print all expenses in file order\n"
    "  filter [--from D] [--to D] [--category C]  print matching expenses\n"
    "  search TEXT                            print expenses whose description matches\n"
    "  summary [--parallel]                   print per-category totals, then the total\n"
    "  help                                   print this list\n"
    "  quit                                   exit";

// Prints a user-facing error message to stderr without terminating the program.
// This keeps the command loop alive after validation failures such as malformed
// input or missing arguments.
void fail(const std::string& message) {
    std::cerr << "Error: " << message << '\n';
}

// Formats and prints a sequence of expenses in the same way the rest of the
// application expects them to be rendered.
void printRows(const std::vector<et::Expense>& rows) {
    for (const et::Expense& e : rows) std::cout << et::formatRow(e) << '\n';
}

// Renders the category totals and the grand total in the summary format defined
// by the formatting layer.
void printTotals(const et::Totals& t) {
    for (const std::pair<const std::string, long long>& kv : t.byCategory) {
        std::cout << et::formatSummaryLine(kv.first, static_cast<double>(kv.second) / 100.0)
                  << '\n';
    }
    std::cout << et::summarySeparator() << '\n';
    std::cout << et::formatSummaryLine("TOTAL", static_cast<double>(t.overall) / 100.0)
              << '\n';
}

// Handles the "add" command.
//
// The command format is:
//   add DATE AMOUNT CATEGORY DESCRIPTION...
//
// The first three tokens are mandatory, while the remainder of the line is treated
// as the description. The expense is then parsed, appended to the in-memory
// collection, and saved to disk so it persists beyond the life of the process.
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

// Handles the "filter" command.
//
// The expression parser accepts repeated flags in any order. Each flag may be
// accompanied by a value, and the filter criteria are validated before running
// the query against the expense list.
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

// Emits the optional diagnostic report when the user invokes the program with
// --diag. This is primarily intended for performance tuning and correctness checks
// around how the Expense objects are copied and moved during loading and storage.
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

// Entry point for the expense tracker. The program accepts a small number of
// startup options and then reads commands from stdin until the user types quit.
// This design makes the program easy to test via scripts because the same input
// stream can be replayed and compared against a known-good output.
int main(int argc, char** argv) {
    std::string path = "spec/expenses.csv";
    bool diag = false;

    // Parse command-line options before reading any interactive commands.
    // --diag turns on extra stderr diagnostics, and --file overrides the default
    // CSV path used to load and persist expenses.
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

    // Load the persisted expense data first so the in-memory collection reflects
    // the current contents of the CSV file before any commands are processed.
    et::LoadResult loaded;
    try {
        loaded = et::load(path);
    } catch (const et::StorageError& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    std::vector<et::Expense> expenses = std::move(loaded.expenses);

    // The command loop is intentionally simple: read one line, trim it, parse the
    // first token as the command, and then dispatch to the handler for that
    // command. Any input line that is empty or contains only whitespace is ignored.
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
                // Search matches on the description, capturing the remainder of the
                // line after the command token so users can enter free-form text.
                std::string text;
                std::getline(in, text);
                text = et::trim(text);
                if (text.empty()) {
                    fail("search requires text");
                } else {
                    printRows(et::search(expenses, text));
                }
            } else if (cmd == "summary") {
                // Summary supports an optional --parallel flag. Without it the
                // single-threaded summary logic executes; with it, the faster
                // multi-threaded path is used when available.
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

    // Emit the final diagnostic summary after the command stream ends, but only if
    // the user explicitly enabled diagnostics at startup.
    if (diag) reportDiagnostics(loaded, expenses.size());
    return 0;
}
