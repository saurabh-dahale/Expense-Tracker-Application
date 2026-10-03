# Analysis Working Notes

Long-form notes accumulated during the build, feeding the Day 3 comparison
report. The report is capped at 2 to 3 pages, so most of what is here will not
survive into it. That is the point of keeping this file: the page limit should
not silently suppress observations made along the way.

**Status tags**

| Tag | Meaning |
|---|---|
| CONFIRMED | The Day 1 prediction held, with a code reference |
| QUALIFIED | Broadly held, but needs a condition attached before it is stated |
| REFUTED | Did not hold as predicted |
| NEW | Not anticipated on Day 1 |
| OPEN | Measurement or check still to do |

Citations give file and symbol. Line numbers are filled in when the report is
drafted, since the files are still moving.

---

## 1. Type system

### 1.1 The one-liner is more permissive than it looks

**Status: QUALIFIED.** Table 3 predicted that Python `datetime` parses and
compares in one call where C++ needs manual parsing or a custom struct. That
held for calendar validation and did not hold for format validation.

`datetime.strptime("2026-9-01", "%Y-%m-%d")` succeeds. The month is unpadded,
which the specification does not allow and which the C++ parser rejects. A
regular expression had to go in front of the call to keep the implementations in
agreement.

- Python: `expense.py`, `parse_date`, the `_DATE_SHAPE` guard before `strptime`
- C++: `expense.h`, `Date::parse`, the length and digit checks before `std::stoi`

What survives into the report: the standard library advantage is real, because
`strptime` rejects `2026-02-30` and handles leap years with no code, where the
C++ side needs `daysInMonth` and `isLeap` written out by hand. But the advantage
is narrower than "one call versus thirty lines" suggests, and the honest framing
is that Python got the calendar logic for free and still had to write the format
check.

Test evidence: `test_expense.py::TestDateParsing::test_rejects_an_unpadded_month`
has an explicit comment saying strptime alone accepts it.

### 1.2 Numeric conversion is permissive in both languages

**Status: CONFIRMED as a non-difference.** `float("1e3")` succeeds. So does
`std::stod("1e3")`. Both implementations had to validate the shape before
converting, and neither language helped.

- Python: `expense.py`, `parse_amount`, `_AMOUNT_SHAPE`
- C++: `expense.h`, `parseAmount`, the dot-position and digit loop

This is the second non-difference in the project, alongside currency rounding.
Worth stating plainly in the report, because a comparison that finds a
difference everywhere it looks is not credible.

### 1.3 The untyped record accepts a misspelled key

**Status: OPEN.** The Python side keeps a dict per expense specifically so this
can be demonstrated, per Table 2. The demonstration itself is not written yet.

Needed: a short snippet showing `expense["catgory"]` raising `KeyError` at
runtime on a code path that may not execute, against a C++ `e.catgory` that
fails at compile time with the member name in the diagnostic. Capture the g++
error text for the figure.

### 1.4 Optional is in both type systems, enforced in one

**Status: NEW, and probably the strongest single point available.**

Both implementations put the recoverable failure in the return type. The
signatures look almost identical:

```cpp
std::optional<Expense> parseExpense(const std::string& rawLine);
```

```python
def parse_expense(raw_line: str) -> Optional[Expense]:
```

C++ will not let a caller reach the value without going through `operator*` or
`value()`, and dereferencing an empty optional is a diagnosable bug. The Python
annotation is never checked at runtime, and nothing stops a caller writing
`parse_expense(line)["amount"]` and getting a `TypeError` only on the rows that
happen to be malformed.

This belongs in the type-system section rather than the error-handling section,
because the mechanism is the same on both sides and only the enforcement
differs. It connects directly to the PEP 484 material from the earlier
assignment.

---

## 2. Memory management

### 2.1 Copy counts on the five-row fixture

**Status: CONFIRMED.** Raw `--diag` output, both implementations, same input.

```
C++                                  Python
sizeof(Expense)      = 88 bytes      record footprint     = 535 bytes (deep)
records held         = 6             records held         = 6
vector capacity path = 1 2 4 8       list bytes path      = 88 120
Expense copy ctor    = 8             record copies        = 0
Expense move ctor    = 23            filtered rows shared = 6 of 6 by identity
Expense copy assign  = 0
Expense move assign  = 0
```

Where the C++ numbers come from:

- The 8 copies are `std::copy_if` in `queries.h` duplicating every matching
  record into a fresh vector, once per `filter` and `search` in the scripted
  sequence.
- The 23 moves are `std::vector` reallocating through capacities 1, 2, 4 and 8
  in `storage.h::load`, relocating every element it already holds each time,
  plus the `push_back(std::move(*e))` per parsed row.

Where the Python numbers come from:

- There is no copy path to count. The comprehension in `queries.py` stores
  another reference to the same dict, which the identity line measures directly
  rather than asserting.
- The list grew once in bytes terms, from 88 to 120, because CPython
  over-allocates. The growth moved pointers. The dicts never moved.

The footprint figures are not directly comparable and the report must say so.
`sizeof` is a compile-time constant that excludes heap-allocated string
contents. `sys.getsizeof` is shallow, so `record_footprint` walks the dict and
everything it points at. Two different measurements of two different things,
presented side by side only to show the order of magnitude.

### 2.2 Mutation through a filtered result

**Status: CONFIRMED, with a test.** `test_queries.py::TestReferenceSemantics::
test_mutating_a_filtered_row_changes_the_source_row` mutates a record reached
through `filter_expenses` and asserts the source list sees the change. The
equivalent C++ test would fail, because the filtered vector holds copies.

This is the cleanest one-paragraph demonstration of value versus reference
semantics in the whole project, and it costs three lines.

### 2.3 Rule of five is not optional once you declare one

**Status: NEW.** Declaring a copy constructor on `Expense` suppressed the
implicitly declared move operations, so all five special members had to be
written out. Without that, every `push_back` would have fallen back to copying
and the move counter would have read zero for the wrong reason.

The instrumentation changed what was being measured. Worth one sentence in the
report as a methodological note, and worth remembering as a general C++ hazard:
adding one special member silently changes the others.

- C++: `expense.h`, the five declarations under the `Counters` struct

### 2.4 Scale measurement

**Status: CONFIRMED.** 200,000 generated rows, two cores, best of three.

| | Peak RSS |
|---|---|
| C++ | 25.8 MB |
| Python | 88.2 MB |

Roughly 3.4x for the same records. Consistent with the per-record footprint
figures above.

### 2.5 Still to do

**Status: OPEN.**

- Valgrind memcheck on `cpp/expense_tracker` to show a clean run under RAII. No
  Python equivalent exists, and the absence is itself the finding.
- `tracemalloc` snapshot on the Python side to attribute allocations, since it
  is the nearest available instrument and it answers a different question than
  Valgrind does. The tooling asymmetry is a reportable observation in its own
  right.
- Re-run `--diag` on the enlarged fixture. The five-row numbers are legible but
  the growth pattern is more convincing at scale.

---

## 3. Error handling

### 3.1 The chosen model

**Status: CONFIRMED, decision documented as Day 1 required.**

| Situation | C++ | Python |
|---|---|---|
| Row does not parse | `std::optional` empty | `None` |
| File cannot be opened or written | throws `StorageError` | raises `StorageError` |
| Command malformed or unknown | stderr, loop continues | stderr, loop continues |
| Fatal I/O | stderr, exit 1 | stderr, exit 1 |

The split is by recoverability, not by language convenience. A bad row is
recoverable because the rest of the file is usable, so the failure goes in the
return type where the caller must handle it. A bad file is not recoverable, so
it propagates.

See 1.4 above for why the interesting part of this is a type-system point rather
than an error-handling one.

### 3.2 Error messages match across implementations

**Status: NEW, and not required by the contract.**

Ran an adversarial script with malformed CSV rows, an invalid date, a reversed
date range, a dangling flag, an unknown flag, a bad amount, a non-lowercase
category, a missing description, a missing search term, and a bad summary flag.
Both implementations produced the same ten stderr lines in the same order, and
agreed on records loaded, rows skipped, and exit status.

```
Error: invalid date: 2026-02-30
Error: --to is before --from
Error: --category requires a value
Error: unknown filter flag: --bogus
Error: could not parse expense: 2026-09-21,5.5,food,Bad amount
Error: could not parse expense: 2026-09-21,5.50,Food,Bad category
Error: add requires a description
Error: add requires DATE AMOUNT CATEGORY DESCRIPTION
Error: search requires text
Error: unknown summary flag: --nope
```

The acceptance test excludes stderr by design, so this agreement was not
enforced by anything. It means the error-handling section can compare observed
behavior rather than only comparing mechanisms, which is a stronger position
than Day 1 anticipated.

### 3.3 The idiomatic tool had to be set aside

**Status: NEW.** The Python CLI does not use `argparse`, which would be the
obvious choice. `argparse` brings its own usage text, its own error wording and
its own exit codes, none of which the hand-written C++ parser reproduces.
Holding the cross-language contract cost Python its idiomatic option.

- Python: `main.py`, the hand-written option loop in `main`, with the reason in
  the module docstring

This is a writability observation with a concrete cause, which is more useful
than a general claim that Python has better libraries. It also cuts against the
expected direction, which makes it worth keeping.

---

## 4. Concurrency

### 4.1 Measured timings

**Status: QUALIFIED. The Python prediction held. The C++ prediction did not.**

200,000 rows, two cores, best of three.

| Run | Wall time | Peak RSS |
|---|---|---|
| C++ `summary` | 0.145 s | 25.8 MB |
| C++ `summary --parallel` | 0.164 s | 25.8 MB |
| Python `summary` | 1.354 s | 88.2 MB |
| Python `--parallel` | 1.344 s | 89.9 MB |

Python showed no speedup, exactly as Day 1 predicted. The global interpreter
lock serializes the CPU-bound accumulation and the thread pool buys nothing.

C++ was **slower** in parallel, which Day 1 did not predict. Two cores, plus
thread creation and the mutex merge, cost more than the work saved at this input
size. The claim that C++ scales therefore needs qualifying by core count and
input size rather than being stated flat.

### 4.2 Two methodological problems with the numbers above

**Status: OPEN, and this must be fixed before the report.**

1. The timings cover the whole process, which is dominated by file reading and
   parsing rather than by the aggregation. The concurrent path is a small
   fraction of what was measured. Timing the summary call alone is required
   before any conclusion is drawn.
2. Two cores is not enough to test a scaling claim. Re-run on a machine with
   four or more, or state the core count as a limitation in the report.

Until those are addressed, the only defensible claim is the Python one.

### 4.3 The synchronization asymmetry

**Status: CONFIRMED.** The C++ merge needs `std::mutex` around the shared
`std::map`, because the threads genuinely run at the same time. The Python merge
needs no lock at all, because each worker returns its own dict and the parent
combines them after the fact.

- C++: `summary.h`, `summarizeParallel`, the `std::lock_guard` in the worker
- Python: `summary.py`, `summarize_parallel`, `pool.map` returning partials

The same algorithm needs explicit synchronization in one language and none in
the other. That is a paradigm-level difference, and it is visible in about six
lines on each side, which makes it a good side-by-side figure.

### 4.4 Concurrency forced a representation change

**Status: NEW, and it connects to the Table 3 currency row.**

Floating-point addition is not associative. The chunked order used by the
parallel path can differ from the sequential order in the last bit, which would
break the byte-identical contract on a large enough fixture. Both
implementations therefore accumulate in integer cents.

- C++: `summary.h`, `toCents`, `long long` accumulators
- Python: `summary.py`, `to_cents`, integer dict values
- Test: `test_summary.py::TestCurrencyRepresentation::
  test_float_accumulation_would_not_be_associative`

Day 1 listed currency rounding as a shared weakness rather than a difference.
That turned out to be right, and the concurrency feature is what forced the team
to confront it. Good material for the conclusion: the comparison exercise
surfaced a correctness issue that neither single-language implementation would
have run into on its own.

---

## 5. Observations with no home yet

- `std::map` gives sorted keys for free and pays on every insert. A Python dict
  preserves insertion order and sorts at print time. Both reach the contract,
  with the cost in different places. Probably a sentence in the conclusion
  rather than a section.
- C++ `iomanip` manipulators persist on the stream, so `std::left` has to be
  re-stated after each `std::right` in `format.h`. The Python f-string carries
  alignment per field. Small, real, citable.
- Header-based C++ meant no separate declaration and definition for this project,
  which sidesteps the Table 3 modularity prediction entirely. Either find a real
  example of the compilation-model constraint or drop that row from the report
  rather than inventing one.
- Both test suites cover the same cases, but the Python suite has a group with no
  C++ counterpart, asserting object identity. The test suites diverged because
  the languages do, which is a small finding about testability.

---

## 6. Report plan

Three full sections, per Day 1 Section 10. Concurrency is promoted to a fourth
only if 4.2 is resolved in time.

| Section | Lead evidence | Status |
|---|---|---|
| Type system | 1.4 optional enforced versus annotated, 1.1 the strptime qualification | Strong, needs 1.3 written |
| Memory management | 2.1 copy counters, 2.2 mutation test, 2.4 peak RSS | Strong, 2.5 would strengthen it |
| Error handling | 3.2 matched stderr behavior, 3.3 argparse set aside | Strong, mostly written up here |
| Concurrency (conditional) | 4.3 synchronization asymmetry, 4.4 representation change | Blocked on 4.2 |

Drop rather than pad: the modularity row in Table 3 has no concrete example yet,
and Day 1 committed to dropping any criterion that cannot produce one.
