"""The output contract from Section 2.3, mirroring cpp/format.h.

Every byte this program writes to stdout is produced here. Nothing else writes
to stdout. That is what makes the acceptance diff a single-file concern on each
side rather than a hunt through the command handlers.

Row:       date(10) SP amount(>10.2f) SP SP category(<12) SP description
Summary:   category(<12) amount(>12.2f)
Separator: 24 hyphens
Total:     "TOTAL"(<12) amount(>12.2f)

Trailing whitespace is stripped from every line.

The C++ version of this file needs iomanip manipulators that persist on the
stream, so std::left has to be re-stated after each std::right. The f-string
carries its alignment per field. That difference is small but it is a real
writability observation with a code citation behind it.
"""

from __future__ import annotations

from typing import Any, Dict

from expense import format_date

SEPARATOR = "-" * 24


def format_row(expense: Dict[str, Any]) -> str:
    line = "{:<10} {:>10.2f}  {:<12} {}".format(
        format_date(expense["date"]),
        expense["amount"],
        expense["category"],
        expense["description"],
    )
    return line.rstrip()


def format_summary_line(label: str, amount: float) -> str:
    return "{:<12}{:>12.2f}".format(label, amount).rstrip()
