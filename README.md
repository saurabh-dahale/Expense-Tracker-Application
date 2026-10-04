# Cross-Language Expense Tracker

A command-line expense tracker implemented twice, once in Python and once in
C++, for a side-by-side comparison of the two languages.

The application is the instrument, not the point. Both programs are held to a
single shared specification so that every observable difference between them is
attributable to the language rather than to a design choice one author made and
the other did not. The two implementations must produce **byte-identical output**
for the same input file and the same commands. Anything that differs after that
is a genuine language difference and is material for the analysis.

**Authors:** Saurabh Dahale and Lily Oforiwa Sam
**Course:** Department of Information Technology, University of the Cumberlands

---

## Table of contents

- [Functionality](#functionality)
- [Requirements](#requirements)
- [Build and compile](#build-and-compile)
- [Run](#run)
- [Command reference](#command-reference)
- [Data format](#data-format)
- [Output contract](#output-contract)
- [Error handling](#error-handling)
- [Testing](#testing)
- [The acceptance test](#the-acceptance-test)
- [Concurrency](#concurrency)
- [Memory diagnostics](#memory-diagnostics)
- [Repository layout](#repository-layout)
- [Design decisions that bind both implementations](#design-decisions-that-bind-both-implementations)
- [Measured results](#measured-results)
- [Troubleshooting](#troubleshooting)

---

## Functionality

The tracker reads a CSV file of expenses, then accepts commands on standard
input until it is told to quit. It can:

- **Record** a new expense and persist it back to the CSV file
- **List** every expense in file order
- **Filter** expenses by an inclusive date range, by category, or by both
- **Search** expense descriptions for a substring, case insensitively
- **Summarize** spending per category and overall, sequentially or concurrently

Expenses live in a single CSV file. There is no database, no network access and
no third-party dependency in either implementation beyond the Python test
runner. That is deliberate. A dependency available in one language and not the
other would contaminate the comparison.

---

## Requirements

| | Needs |
|---|---|
| C++ | A C++17 compiler and pthreads. Verified on g++ 13.3 and clang 15. |
| Python | Python 3.8 or newer. No runtime dependencies. |
| Tests | `pytest` for the Python suite. The C++ suite needs nothing extra. |
| Acceptance test | `bash` and `diff`, both standard on Linux and macOS. |

Install the one test dependency:

```bash
pip install pytest
```

---

## Build and compile

Python is interpreted and needs no build step.

C++ builds with the provided Makefile:

```bash
make -C cpp
```

That produces two binaries in `cpp/`:

| Binary | Purpose |
|---|---|
| `cpp/expense_tracker` | the application |
| `cpp/run_tests` | the unit test suite |

Compiler flags are `-std=c++17 -Wall -Wextra -O2 -pthread`. Warnings are on
deliberately, because the analysis cites compiler diagnostics as evidence and
that only works if they are enabled.

To remove build output:

```bash
make -C cpp clean
```

---

## Run

Both programs take the same options and read commands from standard input.

```
expense_tracker [--file PATH] [--diag]
main.py         [--file PATH] [--diag]
```

| Option | Meaning |
|---|---|
| `--file PATH` | CSV file to load and persist to. Defaults to `spec/expenses.csv`. |
| `--diag` | Print memory diagnostics to stderr at exit. Never affects stdout. |

### Interactive

Run from the repository root so the default file path resolves.

```bash
# C++
cpp/expense_tracker

# Python
python3 python/main.py
```

Then type commands, one per line:

```
list
filter --category groceries
summary
quit
```

### Scripted

Piping a command file is how the acceptance test drives both programs. Work on
a copy, because `add` writes back to the file it loaded.

```bash
cp spec/expenses.csv work.csv

cpp/expense_tracker --file work.csv < spec/commands.txt
python3 python/main.py --file work.csv < spec/commands.txt
```

---

## Command reference

| Command | Arguments | Behavior |
|---|---|---|
| `add` | `DATE AMOUNT CATEGORY DESCRIPTION` | Append one expense and persist the file |
| `list` | none | Print all expenses in file order |
| `filter` | `[--from DATE] [--to DATE] [--category NAME]` | Print expenses matching every supplied criterion |
| `search` | `TEXT` | Print expenses whose description contains TEXT, case insensitive |
| `summary` | none | Print the total per category, then the overall total |
| `summary` | `--parallel` | Identical output, computed concurrently |
| `help` | none | Print the command list |
| `quit` | none | Exit with status 0 |

Notes:

- `--from` and `--to` are **inclusive** on both ends.
- `search` folds case for ASCII letters only (A-Z), in both implementations. Accented letters match exactly.
- Multiple `filter` criteria combine with AND. `filter` with no criteria
  matches everything.
- `DESCRIPTION` is the rest of the line and may contain spaces.
- An unrecognized command prints an error on stderr and the loop continues.

### Example session

```
$ cpp/expense_tracker --file work.csv
list
2026-09-01      42.50  groceries    Weekly shop
2026-09-03      12.00  transport    Bus pass top-up
2026-09-07      89.99  utilities    Electricity bill
2026-09-12      23.75  groceries    Produce market
2026-09-15     150.00  rent         Parking space
filter --from 2026-09-03 --to 2026-09-12
2026-09-03      12.00  transport    Bus pass top-up
2026-09-07      89.99  utilities    Electricity bill
2026-09-12      23.75  groceries    Produce market
search BUS
2026-09-03      12.00  transport    Bus pass top-up
summary
groceries          66.25
rent              150.00
transport          12.00
utilities          89.99
------------------------
TOTAL             318.24
add 2026-09-20 61.40 groceries Late night snacks
quit
```

---

## Data format

A single CSV file with a header row.

```csv
date,amount,category,description
2026-09-01,42.50,groceries,Weekly shop
2026-09-03,12.00,transport,Bus pass top-up
2026-09-07,89.99,utilities,Electricity bill
2026-09-12,23.75,groceries,Produce market
2026-09-15,150.00,rent,Parking space
```

CSV was chosen over JSON because it keeps parsing effort comparable in both
languages. JSON would have handed Python a large unearned advantage through its
standard library and pushed C++ toward a third-party dependency.

Field rules, enforced identically in both implementations:

| Field | Rule |
|---|---|
| `date` | ISO 8601 `YYYY-MM-DD`, zero padded, a real calendar date. `2026-02-30` and `2026-9-01` are both rejected. |
| `amount` | Non-negative decimal with **exactly** two places. `42.5`, `.50` and `1e3` are all rejected. |
| `category` | A single lowercase token. Hyphen and underscore are allowed. |
| `description` | Free text, may contain spaces, must not contain commas, must not be empty. |

A row that violates any of these is skipped, counted, and reported under
`--diag`. The rest of the file still loads.

---

## Output contract

Section 2.3 of the planning document. Every byte either program writes to
stdout is produced by exactly one file on each side, `cpp/format.h` and
`python/formatting.py`. Nothing else writes to stdout.

```
row        date(10) SP amount(right, width 10, 2dp) SP SP category(left, width 12) SP description
summary    category(left, width 12) amount(right, width 12, 2dp)
separator  24 hyphens
total      "TOTAL"(left, width 12) amount(right, width 12, 2dp)
```

Trailing whitespace is stripped from every line. Summary categories print in
alphabetical order.

The frozen reference output lives at `spec/expected_output.txt`. It is the
result of running `spec/commands.txt` against `spec/expenses.csv`, and it is
what both implementations are measured against.

---

## Error handling

The split below is a deliberate design decision, not an accident of
implementation, because the comparison report analyzes it.

| Situation | C++ | Python | Visible effect |
|---|---|---|---|
| A row does not parse | returns `std::optional` empty | returns `None` | Row skipped and counted, loading continues |
| A file cannot be opened or written | throws `StorageError` | raises `StorageError` | `Error: ...` on stderr, exit status 1 |
| A command is malformed or unknown | prints to stderr | prints to stderr | `Error: ...` on stderr, loop continues |
| `quit` | returns 0 | returns 0 | Exit status 0 |

Every error message is written to **stderr** with a leading `Error: ` prefix.
Standard output carries application output only, which is what makes the
acceptance diff meaningful.

The recoverable case is in the return type on both sides. The difference the
report examines is that C++ will not let a caller reach the value without
acknowledging it might be absent, while Python's identical-looking
`Optional[Expense]` annotation is never checked at runtime.

---

## Testing

Two layers, because each catches what the other cannot.

### Unit tests

```bash
make -C cpp test          # C++, 44 assertions
python3 -m pytest python  # Python, 55 cases
```

Both suites cover the same cases, named in Section 6 of the planning document:
date boundary handling on an inclusive filter range, a category with no
matching expenses, an empty input file, a malformed CSV row, case-insensitive
search, and agreement between the sequential and parallel summary.

The Python suite adds a group that has no C++ counterpart, asserting by object
identity that a filtered list holds the very same records as its source. That
is the measurement behind the memory-management section of the report.

C++ uses plain assertions in a separate target rather than a test framework,
since adding one would bring build complexity disproportionate to a three-day
project.

### Expected output

```bash
cp spec/expenses.csv work.csv
cpp/expense_tracker --file work.csv < spec/commands.txt | diff - spec/expected_output.txt
```

An empty diff means the implementation satisfies the contract.

---

## The acceptance test

This is the project's real correctness criterion. Unit tests alone are
insufficient, since both implementations could be wrong in the same way and
still agree.

```bash
spec/run_diff.sh
```

The harness runs the scripted command sequence against both programs, each on
its own private copy of the fixture so that the persisting `add` command cannot
let one run influence the other, then compares three things:

1. stdout, byte for byte
2. the persisted CSV file after `add` has written to it
3. the process exit status

stderr is captured but deliberately excluded, because `--diag` diagnostics and
error messages go there.

Current status:

```
PASS: implementations agree on stdout, persisted file, and exit status
```

Add `--diag` to see both diagnostic blocks side by side:

```bash
spec/run_diff.sh --diag
```

---

## Concurrency

`summary --parallel` partitions the expense list into one chunk per worker,
computes per-category subtotals for each chunk independently, and merges the
partial results. Its output must be byte identical to sequential `summary`,
which makes the existing output contract double as a correctness test for the
concurrent path.

| | C++ | Python |
|---|---|---|
| Mechanism | `std::thread` | `concurrent.futures.ThreadPoolExecutor` |
| Shared state | one `std::map` guarded by `std::mutex` | none, each worker returns its own dict |
| Why | threads run on separate cores and really do race | the global interpreter lock already serializes them |

The expected finding is that Python shows no speedup on this CPU-bound work
because of the interpreter lock, while C++ scales until memory bandwidth
limits it. That negative result for Python is the finding and is reported as
such rather than hidden. See [Measured results](#measured-results) for what
actually happened.

---

## Memory diagnostics

`--diag` writes a block to stderr at exit. It never touches stdout, which is
why the acceptance test compares stdout alone.

C++ on the five-row fixture:

```
[diag] sizeof(Expense)      = 88 bytes
[diag] records held         = 6
[diag] rows skipped on load = 0
[diag] vector capacity path = 1 2 4 8
[diag] Expense copy ctor    = 8
[diag] Expense move ctor    = 23
[diag] Expense copy assign  = 0
[diag] Expense move assign  = 0
```

Python on the same fixture:

```
[diag] record footprint     = 535 bytes (deep, one record)
[diag] records held         = 6
[diag] rows skipped on load = 0
[diag] list bytes path      = 88 120
[diag] record copies        = 0 (no copy path exists)
[diag] filtered rows shared = 6 of 6 by identity
```

How to read this:

- **The copies** come from `std::copy_if` in `filter` and `search`, which
  duplicates every matching record into a fresh vector. The Python
  comprehension stores another reference to the same dict, so there is no copy
  to count and the identity line is the measurement instead.
- **The moves** come from `std::vector` reallocating as it grows through
  capacities 1, 2, 4 and 8, relocating every element it already holds each
  time. A CPython list over-allocates the same way, but a growth there moves
  only pointers. The dicts never move and nothing a caller holds is
  invalidated.
- **The footprints** are not directly comparable and are not presented as if
  they were. `sizeof` is a compile-time constant that excludes heap-allocated
  string contents. `sys.getsizeof` is shallow, so the Python figure is a
  deliberate walk over the dict and everything it points at.

`Expense` declares all five special member functions explicitly rather than
defaulting them, so that each copy and move can be counted. That is
instrumentation for the analysis and would not be in a production version.

---

## Repository layout

```
expense-tracker-crosslang/
  README.md                functionality, build, compile, run for both
  DESIGN.md                the Day 1 planning and design document
  ANALYSIS.md              working notes feeding the comparison report
  docs/screenshots/        terminal captures for the progress and final reports
  spec/
    expenses.csv           shared fixture, the input both programs load
    expected_output.txt    frozen reference output, the contract
    commands.txt           scripted command sequence
    run_diff.sh            the acceptance test
  python/
    expense.py             record type, parsing, validation
    formatting.py          the entire output contract
    storage.py             load and save, list growth trace
    queries.py             filter and search
    summary.py             aggregation, sequential and parallel
    main.py                command loop
    test_expense.py        parsing and formatting tests
    test_queries.py        filter, search, reference-semantics tests
    test_summary.py        aggregation and parallel-agreement tests
  cpp/
    expense.h              Date, Expense, parsing, copy and move counters
    format.h               the entire output contract
    storage.h              load and save, vector capacity trace
    queries.h              filter and search
    summary.h              aggregation, sequential and parallel
    main.cpp               command loop
    tests.cpp              unit tests
    Makefile               build, test, clean
    BUILD.md               C++ specific build and run notes
```

Both implementations live in one repository so that the parallel structure is
visible at a glance, and the mirrored file names make the comparison
navigable. `queries.h` and `queries.py` solve the same problem, so they can be
read side by side.

---

## Design decisions that bind both implementations

Three choices are shared constraints rather than language details. Either
implementation departing from them breaks the acceptance test.

**1. Totals accumulate in integer cents, never in floating point.**
Floating-point addition is not associative, so the chunked order used by
`summary --parallel` can differ from the sequential order in the last bit. On a
large enough fixture that alone would break the byte-identical contract.
Integer addition is associative, so the two paths agree by construction. This
is a shared weakness in both languages rather than a difference between them,
and the report presents it that way.

**2. Amount validation is explicit on both sides.**
Python's `float()` accepts `42.5`, `.50` and `1e3`. C++ `std::stod` is
similarly permissive. Neither is allowed by the specification, so both
implementations validate the shape before converting.

**3. Date validation needs more than the obvious one-liner.**
`datetime.strptime` does parse and validate a calendar date in a single call,
which is a real standard-library-breadth advantage over the hand-rolled C++
`Date`. It is also more permissive than it looks: it accepts `2026-9-01` with
an unpadded month, which C++ rejects. Python therefore needs an explicit format
check in front of it. The advantage is real but smaller than it first appears,
and the report says so.

A fourth decision is worth recording because it cuts the other way. The Python
implementation does not use `argparse`, which would be the idiomatic choice.
`argparse` brings its own usage text, error wording and exit codes, none of
which the C++ side reproduces. Having to set aside the idiomatic tool to hold a
cross-language contract is itself an observation about writability.

---

## Measured results

Measured on a 200,000-row generated fixture, two cores, best of three runs.

| Run | Wall time | Peak RSS |
|---|---|---|
| C++ `summary` | 0.145 s | 25.8 MB |
| C++ `summary --parallel` | 0.164 s | 25.8 MB |
| Python `summary` | 1.354 s | 88.2 MB |
| Python `summary --parallel` | 1.344 s | 89.9 MB |

Two caveats that matter more than the numbers:

1. These timings cover the whole process, which is dominated by loading and
   parsing rather than by the summary itself. Timing the aggregation alone is
   required before drawing conclusions about the concurrent path.
2. Python showed no speedup, as predicted. C++ was also slightly **slower** in
   parallel, because this machine has two cores and thread setup plus the mutex
   merge cost more than the work saved at this input size. The prediction that
   C++ scales therefore needs qualifying by core count rather than being stated
   flat.

---

## Troubleshooting

**`cpp/expense_tracker: No such file or directory`**
The binary is not built. Run `make -C cpp`.

**`Error: cannot open spec/expenses.csv`**
Run from the repository root, or pass an explicit `--file PATH`.

**The file changed after I ran something**
`add` persists to whatever `--file` points at. Work on a copy.

**`run_diff.sh` says the Python implementation is not present**
It expects `python/main.py` relative to the repository root. Run the script by
its path from anywhere, as it resolves the root itself.

**pytest reports `ModuleNotFoundError`**
Run it as `python3 -m pytest python` from the repository root, so the modules
resolve.

**Compiler errors mentioning `std::optional`**
The compiler is defaulting to a pre-C++17 standard. The Makefile sets
`-std=c++17`. If you are compiling by hand, pass it.
