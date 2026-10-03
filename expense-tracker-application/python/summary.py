"""Aggregation, sequential and concurrent.

Totals accumulate in integer cents for the same reason the C++ side does.
Floating-point addition is not associative, so the chunked order used by the
concurrent path can differ from the sequential order in the last bit, which
would break the byte-identical output contract on a large enough fixture. This
is a shared constraint rather than a language difference, and the Day 3 report
should present it as one.

The dict is the other half of the story. std::map keeps its keys sorted, which
is what the output contract wants, so the C++ side gets ordering for free and
pays for it on every insert. A Python dict preserves insertion order and has to
be sorted explicitly when it is printed.
"""

from __future__ import annotations

import os
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from typing import Dict, List

from expense import Expense


@dataclass
class Totals:
    by_category: Dict[str, int] = field(default_factory=dict)  # cents
    overall: int = 0                                           # cents


def to_cents(amount: float) -> int:
    return round(amount * 100)


def summarize(all_expenses: List[Expense]) -> Totals:
    by_category: Dict[str, int] = {}
    for expense in all_expenses:
        key = expense["category"]
        by_category[key] = by_category.get(key, 0) + to_cents(expense["amount"])
    # Sorted here rather than at the point of use, so that both Totals objects
    # compare equal and the printing code stays identical to the C++ version.
    ordered = {k: by_category[k] for k in sorted(by_category)}
    return Totals(by_category=ordered, overall=sum(ordered.values()))


def _chunk_totals(chunk: List[Expense]) -> Dict[str, int]:
    local: Dict[str, int] = {}
    for expense in chunk:
        key = expense["category"]
        local[key] = local.get(key, 0) + to_cents(expense["amount"])
    return local


def summarize_parallel(all_expenses: List[Expense], workers: int = 0) -> Totals:
    """Same result as summarize, computed on a thread pool.

    The merge needs no lock. Each worker returns its own dict and the parent
    combines them, so nothing is shared while the workers run. The C++ version
    needs a std::mutex around the shared map because its threads really do run
    at the same time. Here the global interpreter lock already serializes the
    bytecode, which is why this path is expected to show no speedup on
    CPU-bound work and may be slower once thread overhead is counted. That
    negative result is the finding, not a defect to hide.
    """
    if workers <= 0:
        workers = os.cpu_count() or 2
    if len(all_expenses) < workers:
        workers = len(all_expenses)
    if workers <= 1:
        return summarize(all_expenses)

    chunk = (len(all_expenses) + workers - 1) // workers
    slices = [all_expenses[i:i + chunk] for i in range(0, len(all_expenses), chunk)]

    merged: Dict[str, int] = {}
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for partial in pool.map(_chunk_totals, slices):
            for key, value in partial.items():
                merged[key] = merged.get(key, 0) + value

    ordered = {k: merged[k] for k in sorted(merged)}
    return Totals(by_category=ordered, overall=sum(ordered.values()))
