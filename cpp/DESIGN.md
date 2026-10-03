# Cross-Language Expense Tracker: Initial Planning and Design

**Deliverable 1, Day 1**

Saurabh Dahale and Lily Oforiwa Sam
Department of Information Technology, University of the Cumberlands
Friday, October 3, 2026

> Markdown mirror of the submitted planning document. The submitted copy is the
> record of record. This file exists so the design is readable in the
> repository next to the code it governs.

---

## 1. Project Scope and Guiding Principle

The team will build a stand-alone, text-based expense tracker twice, once in
Python and once in C++. Both programs will record expenses, filter them by date
range and category, and report totals by category and overall.

The design is governed by one principle that shapes every decision below.
Because the grade rests primarily on the quality of the language comparison
rather than on the application itself, the two implementations must be
behaviorally identical so that every observed difference is attributable to the
language rather than to a design choice one member made and the other did not.
We therefore fix three things before any code is written: the on-disk data
format, the command set, and the exact output text. Both programs must produce
byte-identical output for the same input file and the same commands. Anything
that differs after that is a genuine language difference and is material for the
analysis.

---

## 2. Shared Specification

### 2.1 Data Format

Expenses are stored in a single CSV file named `expenses.csv` with a header row.
CSV was chosen over JSON because it keeps parsing effort comparable in both
languages. JSON would have given Python a large unearned advantage through its
standard library and forced C++ toward a third-party dependency, which would
distort the comparison.

```
date,amount,category,description
2026-09-01,42.50,groceries,Weekly shop
2026-09-03,12.00,transport,Bus pass top-up
2026-09-07,89.99,utilities,Electricity bill
2026-09-12,23.75,groceries,Produce market
2026-09-15,150.00,rent,Parking space
```

Field rules are identical in both implementations. The date is ISO 8601
(YYYY-MM-DD). The amount is a non-negative decimal with exactly two places. The
category is a lowercase single token. The description is free text that may
contain spaces but no commas, which keeps CSV parsing simple and equivalent in
both languages.

### 2.2 Command Set

**Table 1.** *Shared Command Interface*

| Command | Arguments | Behavior |
|---|---|---|
| `add` | `date amount category description` | Append one expense and persist it |
| `list` | none | Print all expenses in file order |
| `filter` | `--from DATE --to DATE --category NAME` | Print expenses matching all supplied criteria |
| `search` | `TEXT` | Print expenses whose description contains TEXT, case insensitive |
| `summary` | none | Print total per category, then the overall total |
| `summary` | `--parallel` | Identical output, computed concurrently. See Section 4 |
| `help` | none | Print the command list |
| `quit` | none | Exit with status 0 |

### 2.3 Output Contract

Both programs print an expense row as a single line of fixed-width columns, with
the amount right-aligned to two decimal places. The summary prints one line per
category sorted alphabetically, then a separator, then the overall total. Error
messages are written to standard error with a leading `Error:` prefix and exit
status 1. The acceptance test is a shell diff between the two programs' output
on the shared fixture file, and the Day 3 milestone is not met until that diff
is empty.

---

## 3. Component Breakdown

The application decomposes into seven components. The decomposition is
deliberately the same in both languages so that each component becomes a direct
comparison point.

**Table 2.** *Component Decomposition and Expected Language Divergence*

| Component | Python approach | C++ approach | Expected divergence |
|---|---|---|---|
| Domain model | Dictionary per expense, keys as field names | `struct Expense` with typed members | Heterogeneous untyped record versus fixed typed layout |
| Persistence | `csv` module, context manager for file handles | `ifstream` and `ofstream` with RAII, manual tokenizing | Library coverage and resource cleanup style |
| Collection | list of dictionaries | `std::vector<Expense>` | Reallocation and copy semantics become visible in C++ |
| Query | List comprehensions and `filter` | `std::copy_if` with lambdas into a new vector | Expression-level conciseness versus explicit iterators |
| Aggregation | `collections.defaultdict(float)` | `std::map<std::string,double>` and `std::accumulate` | Implicit default values versus explicit insertion semantics |
| Concurrency | `concurrent.futures` thread pool | `std::thread` with `std::mutex` | The GIL prevents real parallelism in Python. See Section 4 |
| CLI layer | input loop, `str.split`, try and except | `getline`, `istringstream`, `optional` or exceptions | Error signaling strategy differs most here |

---

## 4. Concurrency Element and Its Justification

An expense tracker does not need concurrency, and adding it without reason would
be artificial. We are adding one narrow concurrent feature anyway, for a
specific analytical reason. Concurrency is among the language-specific features
the project asks teams to demonstrate, and it is the single axis on which Python
and C++ differ most sharply at the level of programming paradigm rather than
syntax. No other feature in this application will expose that difference.

The feature is `summary --parallel`. It partitions the expense list into chunks,
computes per-category subtotals for each chunk on a separate worker, and merges
the partial results. Its output must be byte-identical to sequential `summary`,
which means the output contract doubles as a correctness test for the concurrent
implementation. The scope is deliberately small, roughly thirty lines per
language, so the risk to the Saturday schedule stays low.

The expected finding is the point of the exercise. Python threads are serialized
by the global interpreter lock for CPU-bound work, so the parallel version
should show no speedup and may be slower once thread overhead is counted. C++
threads run on separate cores, so the same algorithm should scale until memory
bandwidth limits it. Both teams will time the two paths on an enlarged fixture
and report the measured difference rather than asserting the textbook claim. If
Python shows no improvement, that negative result is the finding and will be
reported as such.

---

## 5. Anticipated Language-Specific Challenges

Naming these in advance matters, because the analysis is graded on specific
examples drawn from our own code rather than on general claims. Each item below
is a prediction we intend to confirm or refute with a concrete code reference.

**Table 3.** *Predicted Difficulties and the Analysis Point Each Supports*

| Area | Anticipated challenge | Analysis point it supports |
|---|---|---|
| Dates | Python `datetime` parses and compares in one call. C++ needs `std::chrono` with manual parsing or a custom comparable date struct | Standard library breadth as a writability factor |
| Type system | Python dictionaries accept a misspelled key silently. C++ rejects a misspelled member at compile time | When errors surface, and the cost of each position |
| Memory | Python reference counting is invisible. C++ vector growth, copies on return, and reference versus value parameters are explicit choices | Deterministic destruction versus automatic management |
| Concurrency | Python threads serialize under the GIL for CPU-bound work. C++ threads run in parallel but require explicit synchronization of the shared result map | Paradigm-level difference in the concurrency model |
| Numeric types | Python floats and the `decimal` module behave differently for currency. C++ `double` has the same rounding hazard | Shared weakness rather than a difference, worth reporting honestly |
| Error handling | Python raises freely. C++ forces a choice among exceptions, error codes, and `std::optional` | Explicit versus implicit fallibility in interfaces |
| Modularity | Python modules import directly. C++ separates declaration and definition across headers | Compilation model as a design constraint |

Two items deserve emphasis. First, the currency rounding hazard is identical in
both languages, and reporting a non-difference honestly is more convincing than
manufacturing a contrast. Second, the C++ error handling decision should be made
deliberately and documented, because an unexamined choice there would weaken the
strongest section of the analysis.

---

## 6. Testing Strategy

Testing operates at two levels, because each catches something the other cannot.
The acceptance test is the diff harness described in Section 2.3, which proves
the two implementations agree with each other. On its own it is insufficient,
since both could be wrong in the same way and the diff would still pass.

Each implementation therefore also carries a small set of unit tests covering
the cases most likely to break quietly. Those are date boundary handling on an
inclusive filter range, a category with no matching expenses, an empty input
file, a malformed CSV row, case-insensitive search, and agreement between
sequential and parallel summary. Python uses `pytest` and C++ uses plain
assertions in a separate test target, since adopting a C++ test framework would
add build complexity disproportionate to a three-day project. Unit tests are
written alongside each component rather than at the end.

---

## 7. Role Assignment

The team has two members, so each owns exactly one implementation. This
preserves the most important property of the arrangement, which is that no
single person writes both codebases and unconsciously imposes the same habits on
each. Assignment follows experience. Saurabh has prior production C++ and
systems work, and Lily takes Python, which also means neither person is
reviewing their own design decisions when the analysis is written.

**Table 4.** *Role Assignment and Deliverables*

| Role | Owner | Responsibilities and deliverables |
|---|---|---|
| C++ implementation lead | Saurabh Dahale | All C++ source, unit tests, and build instructions. Also owns the shared fixture, the expected output file, and the diff harness in `spec/` |
| Python implementation lead | Lily Oforiwa Sam | All Python source, unit tests, and run instructions. Also owns the README and the repository structure |
| Shared | Both members | The Day 2 progress report, the Day 3 comparison report, and the presentation. Each drafts the parts covering their own language, then reviews and challenges the other's |

A two-person team loses something a larger team would have, which is a neutral
specification owner able to settle a dispute about the output contract. We
substitute process for that missing person. The contract in Section 2 is frozen
once this document is approved, and it lives in version control at
`spec/expected_output.txt`. Changing it afterward requires a pull request that
both members approve, and the pull request must state why the change is a
genuine specification error rather than a convenience for one language. Without
that rule, the natural pressure when an implementation is difficult is to
quietly relax the contract, which would destroy the basis for the comparison.

Both members read both implementations before the analysis is written. With only
two people this is unavoidable rather than aspirational, and it is the main
compensation for the smaller team, since every observation in the analysis will
have been seen by someone who did not write the code in question. Both members
also present, as the project requires, and each presents the language they did
not write. That arrangement forces genuine cross-reading and makes the
comparison credible to the audience.

---

## 8. Three-Day Timeline

**Table 5.** *Milestones by Half Day*

| When | Milestone | Done means |
|---|---|---|
| Fri PM | Planning complete | This document approved, repository created, spec frozen |
| Fri PM | Fixture ready | `expenses.csv` fixture and expected output committed to `spec/` |
| Sat AM | Model and persistence | Both languages load the fixture and print it with `list` |
| Sat AM | Unit tests started | Tests written alongside each component, not deferred |
| Sat PM | Query and aggregation | `filter`, `search`, and `summary` work in both languages |
| Sat PM | First diff run | Outputs compared, contract violations logged as issues |
| Sat PM | Day 2 report | Screenshots captured during the build, brief report submitted |
| Sun AM | Concurrency and add | `summary --parallel` matches sequential, `add` persists, timings recorded |
| Sun AM | Code swap | Each member has read and annotated the other's implementation |
| Sun PM | Comparison report | 2 to 3 pages, APA 7, with code snippets and screenshots |
| Sun PM | Slides built | 15 to 20 minutes of material, both members assigned sections |
| Sun PM | Submission | README finished, repository tagged, report and slides submitted |

The schedule front-loads implementation into Saturday so that Sunday is
protected for the analysis and the presentation, which together carry most of
the grade. With two members rather than four, each person writes an entire
implementation alone, so Saturday is the real risk in this plan. If Saturday
slips, the agreed response is to cut the `search` command first and the `add`
command second, rather than compressing Sunday. Cutting scope is preferred to
cutting analysis or rehearsal time in every case. Screenshots are captured while
building rather than reconstructed afterward, because recreating terminal output
on Sunday night wastes time that belongs to the report.

---

## 9. Repository Structure

```
expense-tracker-crosslang/
  README.md               functionality, build, compile, run for both
  DESIGN.md               this document in markdown
  ANALYSIS.md             long-form notes feeding the Day 3 report
  docs/screenshots/       terminal captures for the Day 2 and Day 3 reports
  spec/
    expenses.csv          shared fixture
    expected_output.txt   expected output for the acceptance test
    commands.txt          scripted command sequence
    run_diff.sh           runs both programs and diffs the output
  python/
    expense.py  storage.py  queries.py  summary.py  main.py  test_*.py
  cpp/
    expense.h  storage.h  queries.h  summary.h  main.cpp  tests.cpp
    Makefile
```

Both implementations live in one repository so that reviewers can see the
parallel structure immediately, and the mirrored file names make the comparison
navigable. Each member commits to a feature branch and opens a pull request,
which gives the project a reviewable collaborative history rather than a single
upload. The README is a graded artifact in its own right on Day 2 and Day 3, so
it must cover what the application does and exactly how to build, compile, and
run each implementation.

---

## 10. Deliverable Map and Analysis Plan

**Table 6.** *What Is Submitted Each Day*

| Day | Submission | Owner |
|---|---|---|
| Friday | This planning and design report, repository created | Both |
| Saturday | Brief progress report with screenshots and repository link | Both |
| Sunday | Comparison report (2 to 3 pages, APA 7) and presentation slides | Both |

The Sunday comparison report is capped at 2 to 3 pages, which is the single most
important constraint on the analysis and the reason it must be planned now. Six
criteria will not fit. We will therefore give full sections to only three,
selected because they produce the sharpest contrast and because our own code
will supply concrete examples: the type system, memory management, and error
handling. Concurrency becomes a fourth section only if the measured timings
prove interesting, which we will know by Sunday morning. Readability,
writability, and overall language experience are addressed in the conclusion
rather than as separate sections, since they are judgments that follow from the
first three rather than independent findings.

The report is organized by criterion rather than by language, so each section
places the two code samples side by side. Every claim cites a file and line
number from our own repository, following the evaluation framework used
throughout this course (Sebesta, 2019). Any criterion where we cannot produce a
concrete example from our code will be dropped rather than padded with general
statements. Longer working notes accumulate in `ANALYSIS.md` during the build
and are condensed into the submitted report, which keeps the page limit from
suppressing observations we make along the way.

---

## Reference

Sebesta, R. W. (2019). *Concepts of programming languages* (12th ed.). Pearson.
