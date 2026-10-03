"""Command loop for the Python expense tracker.

Usage: main.py [--file PATH] [--diag]

Commands are read from stdin, one per line, so the same scripted sequence in
spec/commands.txt drives both implementations and the acceptance test is a diff
of their stdout. Diagnostics requested with --diag go to stderr only, which is
why the diff harness compares stdout alone.

The option parsing below is hand-written rather than argparse. That is a
concession to the output contract, not a preference: argparse brings its own
usage text, its own error wording and its own exit codes, none of which the C++
side would reproduce. Noting that the idiomatic tool had to be set aside to
keep two implementations in agreement is itself material for the report.
"""

from __future__ import annotations

import sys
from typing import List, Optional

from expense import Expense, parse_date, parse_expense, record_footprint, shared_object_count
from formatting import SEPARATOR, format_row, format_summary_line
from queries import FilterCriteria, filter_expenses, search
from storage import LoadResult, StorageError, load, save
from summary import Totals, summarize, summarize_parallel

HELP = """Commands:
  add DATE AMOUNT CATEGORY DESCRIPTION   append one expense and persist it
  list                                   print all expenses in file order
  filter [--from D] [--to D] [--category C]  print matching expenses
  search TEXT                            print expenses whose description matches
  summary [--parallel]                   print per-category totals, then the total
  help                                   print this list
  quit                                   exit"""


def fail(message: str) -> None:
    print("Error: " + message, file=sys.stderr)


def print_rows(rows: List[Expense]) -> None:
    for expense in rows:
        print(format_row(expense))


def print_totals(totals: Totals) -> None:
    for category, cents in totals.by_category.items():
        print(format_summary_line(category, cents / 100.0))
    print(SEPARATOR)
    print(format_summary_line("TOTAL", totals.overall / 100.0))


def handle_add(args: str, expenses: List[Expense], path: str) -> None:
    parts = args.split(None, 3)
    if len(parts) < 3:
        fail("add requires DATE AMOUNT CATEGORY DESCRIPTION")
        return
    if len(parts) < 4 or not parts[3].strip():
        fail("add requires a description")
        return

    row = "{},{},{},{}".format(parts[0], parts[1], parts[2], parts[3].strip())
    expense = parse_expense(row)
    if expense is None:
        fail("could not parse expense: " + row)
        return
    expenses.append(expense)
    save(path, expenses)


def handle_filter(args: str, expenses: List[Expense]) -> None:
    criteria = FilterCriteria()
    tokens = args.split()
    index = 0
    while index < len(tokens):
        flag = tokens[index]
        if index + 1 >= len(tokens):
            fail(flag + " requires a value")
            return
        value = tokens[index + 1]
        index += 2

        if flag in ("--from", "--to"):
            parsed = parse_date(value)
            if parsed is None:
                fail("invalid date: " + value)
                return
            if flag == "--from":
                criteria.date_from = parsed
            else:
                criteria.date_to = parsed
        elif flag == "--category":
            criteria.category = value
        else:
            fail("unknown filter flag: " + flag)
            return

    if (criteria.date_from is not None and criteria.date_to is not None
            and criteria.date_to < criteria.date_from):
        fail("--to is before --from")
        return
    print_rows(filter_expenses(expenses, criteria))


def report_diagnostics(loaded: LoadResult, expenses: List[Expense]) -> None:
    footprint = record_footprint(expenses[0]) if expenses else 0
    # Identity check rather than a copy counter: there is no copy to count,
    # so the measurement has to show that the filtered list holds the very
    # same objects as the source list.
    sample = filter_expenses(expenses, FilterCriteria())
    shared = shared_object_count(expenses, sample)

    out = sys.stderr
    print("[diag] record footprint     = {} bytes (deep, one record)".format(footprint), file=out)
    print("[diag] records held         = {}".format(len(expenses)), file=out)
    print("[diag] rows skipped on load = {}".format(loaded.skipped), file=out)
    print("[diag] list bytes path      ={}".format(
        "".join(" " + str(n) for n in loaded.size_trace)), file=out)
    print("[diag] record copies        = 0 (no copy path exists)", file=out)
    print("[diag] filtered rows shared = {} of {} by identity".format(shared, len(sample)), file=out)


def main(argv: List[str]) -> int:
    path = "spec/expenses.csv"
    diag = False

    index = 1
    while index < len(argv):
        arg = argv[index]
        if arg == "--diag":
            diag = True
        elif arg == "--file":
            if index + 1 >= len(argv):
                print("Error: --file requires a path", file=sys.stderr)
                return 1
            index += 1
            path = argv[index]
        else:
            print("Error: unknown option: " + arg, file=sys.stderr)
            return 1
        index += 1

    try:
        loaded = load(path)
    except StorageError as exc:
        print("Error: " + str(exc), file=sys.stderr)
        return 1
    expenses = loaded.expenses

    for line in sys.stdin:
        stripped = line.strip()
        if not stripped:
            continue
        split = stripped.split(None, 1)
        command = split[0]
        args = split[1] if len(split) > 1 else ""

        try:
            if command == "quit":
                break
            elif command == "help":
                print(HELP)
            elif command == "list":
                print_rows(expenses)
            elif command == "add":
                handle_add(args, expenses, path)
            elif command == "filter":
                handle_filter(args, expenses)
            elif command == "search":
                text = args.strip()
                if not text:
                    fail("search requires text")
                else:
                    print_rows(search(expenses, text))
            elif command == "summary":
                flag = args.strip()
                if not flag:
                    print_totals(summarize(expenses))
                elif flag == "--parallel":
                    print_totals(summarize_parallel(expenses))
                else:
                    fail("unknown summary flag: " + flag)
            else:
                fail("unknown command: " + command)
        except StorageError as exc:
            print("Error: " + str(exc), file=sys.stderr)
            return 1

    if diag:
        report_diagnostics(loaded, expenses)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
