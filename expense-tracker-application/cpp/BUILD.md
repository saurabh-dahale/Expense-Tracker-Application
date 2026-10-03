# C++ implementation: build, compile, and run

Owner: Saurabh Dahale. Requires a C++17 compiler and pthreads. Verified on
g++ 13.3 and clang 15.

## Build

```
make -C cpp
```

Produces two binaries in `cpp/`:

- `expense_tracker` the application
- `run_tests` the unit test suite

Flags are `-std=c++17 -Wall -Wextra -O2 -pthread`. Warnings are enabled because
the type-system section of the Day 3 report cites compiler diagnostics as
evidence, which only works if they are turned on.

## Test

```
make -C cpp test
```

Exits 0 when every assertion passes, 1 otherwise.

## Run

```
cpp/expense_tracker [--file PATH] [--diag]
```

Commands are read from stdin, one per line. `--file` defaults to
`spec/expenses.csv`. `--diag` adds memory diagnostics on stderr at exit.

Interactive:

```
cpp/expense_tracker --file spec/expenses.csv
```

Scripted, which is how the acceptance test drives it:

```
cpp/expense_tracker --file work.csv < spec/commands.txt
```

## Acceptance test

```
spec/run_diff.sh
```

Runs the scripted sequence against both implementations on separate copies of
the fixture and diffs stdout, the persisted CSV, and the exit status. Until
`python/main.py` exists it prints the C++ output instead of comparing.

## Output contract

Defined in one place, `cpp/format.h`, and frozen in `spec/expected_output.txt`.
Nothing outside `format.h` writes to stdout.

```
row        date(10) SP amount(right, 10, 2dp) SP SP category(left, 12) SP description
summary    category(left, 12) amount(right, 12, 2dp)
separator  24 hyphens
total      "TOTAL"(left, 12) amount(right, 12, 2dp)
```

Trailing whitespace is stripped from every line. Categories print
alphabetically.

## Error model

The choice recorded in Section 5 of the planning document, made deliberately
because the report analyzes it.

- A row that does not parse returns `std::nullopt`. It is skipped and counted,
  and the rest of the file still loads.
- A file that cannot be opened or written throws `StorageError`. There is no
  useful recovery, so the type does not ask the caller to handle it.
- A bad command prints `Error: ...` on stderr and the loop continues.
- A fatal I/O error prints `Error: ...` on stderr and exits 1. `quit` exits 0.

## Notes for the Python side

Three decisions in the C++ code are shared constraints, not C++ details. The
Python implementation has to match them or the diff will fail.

1. **Totals accumulate in integer cents, not floats.** Floating-point addition
   is not associative, so the chunked order used by `summary --parallel` can
   differ from the sequential order in the last bit. Integers make the two
   agree by construction. Python needs the same treatment, or `Decimal`.
2. **Amounts must have exactly two decimal places.** `parseAmount` rejects
   `42.5`, `.50`, and `1e3`. Python's `float()` accepts all three, so the
   Python parser needs an explicit check rather than a bare `float()` call.
3. **Categories print sorted.** `std::map` gives this for free. A Python dict
   preserves insertion order, so it has to sort at print time.

## Diagnostics

`--diag` writes to stderr only, never stdout, which is why `run_diff.sh`
compares stdout alone. On the five-row fixture:

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

These are the raw numbers for the memory-management section. The copies come
from `std::copy_if` in `filter` and `search`, which duplicates every matching
record into a fresh vector. The moves come from `std::vector` reallocating as
it grows through capacities 1, 2, 4, and 8, relocating every element it already
holds each time. The equivalent Python run performs zero of either, because a
list comprehension stores another reference to the same object and a list
growth never touches the objects it points at.
