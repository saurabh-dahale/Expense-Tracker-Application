"""Persistence, and the list growth trace.

The error model matches the C++ side by intent, not by imitation. A file that
cannot be opened raises, because no caller can recover from it. A row that does
not parse comes back as None and is counted as skipped, because the rest of the
file is still usable.

The difference worth reporting is not the shape of the two models, which is the
same, but what the language does about it. C++ puts the recoverable case in the
return type, so a caller cannot reach the value without first acknowledging it
might be absent. Python puts it in the same place and the interpreter never
checks. Both files say Optional. Only one of them is enforced.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from typing import List

from expense import Expense, parse_expense, to_csv

HEADER = "date,amount,category,description"


class StorageError(Exception):
    """Unrecoverable I/O failure. Mirrors et::StorageError."""


@dataclass
class LoadResult:
    expenses: List[Expense] = field(default_factory=list)
    skipped: int = 0
    # sys.getsizeof of the backing list at each point it grew. CPython lists
    # over-allocate the same way std::vector does, but a growth here only moves
    # pointers: the dicts they point at never move, and nothing a caller is
    # holding is invalidated. The C++ trace in the same run shows elements
    # being relocated.
    size_trace: List[int] = field(default_factory=list)


def load(path: str) -> LoadResult:
    result = LoadResult()
    last_size = 0
    try:
        with open(path, "r", encoding="utf-8") as handle:
            for index, line in enumerate(handle):
                if index == 0 and line.strip().startswith("date,"):
                    continue
                if not line.strip():
                    continue
                expense = parse_expense(line)
                if expense is None:
                    result.skipped += 1
                    continue
                result.expenses.append(expense)
                current = sys.getsizeof(result.expenses)
                if current != last_size:
                    last_size = current
                    result.size_trace.append(current)
    except OSError as exc:
        raise StorageError("cannot open {}".format(path)) from exc
    return result


def save(path: str, expenses: List[Expense]) -> None:
    try:
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(HEADER + "\n")
            for expense in expenses:
                handle.write(to_csv(expense) + "\n")
    except OSError as exc:
        raise StorageError("cannot write {}".format(path)) from exc
