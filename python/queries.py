"""Filter and search.

Each is one comprehension against the C++ std::copy_if plus lambda plus
back_inserter. The syntax gap is the obvious observation and the less
interesting one. The semantic gap is that copy_if duplicates every matching
Expense into the new vector, while the comprehension puts another reference to
the same dict in the new list. expense.shared_object_count measures that, and
the C++ copy counters measure the other side of it.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date
from typing import List, Optional

from expense import Expense


@dataclass
class FilterCriteria:
    date_from: Optional[date] = None  # inclusive
    date_to: Optional[date] = None    # inclusive
    category: Optional[str] = None

    def matches(self, expense: Expense) -> bool:
        if self.date_from is not None and expense["date"] < self.date_from:
            return False
        if self.date_to is not None and expense["date"] > self.date_to:
            return False
        if self.category is not None and expense["category"] != self.category:
            return False
        return True


def filter_expenses(all_expenses: List[Expense], criteria: FilterCriteria) -> List[Expense]:
    return [e for e in all_expenses if criteria.matches(e)]


def search(all_expenses: List[Expense], text: str) -> List[Expense]:
    needle = text.lower()
    return [e for e in all_expenses if needle in e["description"].lower()]
