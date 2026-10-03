"""Domain model for the cross-language expense tracker.

The record type here is a plain dict, which is the divergence recorded in
Table 2 of the planning document: a heterogeneous untyped record against the
fixed typed layout of the C++ struct. Keeping the dict rather than reaching for
a dataclass is deliberate, because the type-system section of the Day 3 report
needs a Python side that really does accept a misspelled key without complaint.

Two things here look like they should be shorter than their C++ counterparts
and, on inspection, are not:

  1. datetime.strptime does parse and validate a calendar date in one call,
     which is the standard-library-breadth point. But it is also more
     permissive than it looks: it happily accepts "2026-9-01" with an
     unpadded month, where the C++ parser rejects it. The explicit shape
     check below is what keeps the two implementations in agreement, and the
     need for it is a finding in its own right.

  2. float() accepts "42.5", ".50" and "1e3", none of which the shared
     specification allows. The regular expression is not ceremony, it is the
     validation the C++ character loop does by hand.
"""

from __future__ import annotations

import re
from datetime import date, datetime
from typing import Any, Dict, List, Optional

Expense = Dict[str, Any]  # keys: date, amount, category, description

_DATE_SHAPE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
_AMOUNT_SHAPE = re.compile(r"^\d+\.\d{2}$")
_CATEGORY_SHAPE = re.compile(r"^[a-z_-]+$")


def parse_date(text: str) -> Optional[date]:
    """Parse YYYY-MM-DD, rejecting what the C++ Date::parse rejects."""
    if not _DATE_SHAPE.match(text):
        return None
    try:
        return datetime.strptime(text, "%Y-%m-%d").date()
    except ValueError:
        # Covers 2026-02-30 and 2026-13-01. The C++ side needs an explicit
        # day-length table and a leap-year rule to reach the same answer.
        return None


def format_date(value: date) -> str:
    return value.strftime("%Y-%m-%d")


def parse_amount(text: str) -> Optional[float]:
    """Non-negative decimal with exactly two places, per the specification."""
    if not _AMOUNT_SHAPE.match(text):
        return None
    return float(text)


def parse_expense(raw_line: str) -> Optional[Expense]:
    """Return a record, or None for a malformed row.

    Returning None rather than raising mirrors the C++ std::optional and
    matches the error model agreed on Day 1: a bad row is recoverable because
    the rest of the file is still usable, so the caller is handed the failure
    instead of being interrupted by it.

    The annotation says Optional[Expense]. Nothing at runtime enforces that,
    which is exactly the asymmetry the type-system section examines.
    """
    line = raw_line.strip()
    if not line:
        return None

    fields = line.split(",", 3)
    if len(fields) != 4:
        return None

    parsed_date = parse_date(fields[0].strip())
    if parsed_date is None:
        return None

    amount = parse_amount(fields[1].strip())
    if amount is None:
        return None

    category = fields[2].strip()
    if not _CATEGORY_SHAPE.match(category):
        return None

    description = fields[3].strip()
    if not description:
        return None

    return {
        "date": parsed_date,
        "amount": amount,
        "category": category,
        "description": description,
    }


def to_csv(expense: Expense) -> str:
    return "{},{:.2f},{},{}".format(
        format_date(expense["date"]),
        expense["amount"],
        expense["category"],
        expense["description"],
    )


def record_footprint(expense: Expense) -> int:
    """Bytes held by one record, counting the dict and everything it points at.

    sys.getsizeof is shallow, so the C++ sizeof(Expense) has no direct
    counterpart and this walk is the closest honest equivalent. Reported under
    --diag for the memory-management section.
    """
    import sys

    total = sys.getsizeof(expense)
    for key, value in expense.items():
        total += sys.getsizeof(key) + sys.getsizeof(value)
    return total


def shared_object_count(left: List[Expense], right: List[Expense]) -> int:
    """How many records in `right` are the very same objects as in `left`.

    The C++ filter copies every match into a new vector. The Python
    comprehension stores another reference to the same dict. This counts the
    overlap by identity so the report can state that as a measurement rather
    than as a claim about semantics.
    """
    originals = {id(item) for item in left}
    return sum(1 for item in right if id(item) in originals)
