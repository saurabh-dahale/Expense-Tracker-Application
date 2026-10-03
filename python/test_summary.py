"""Aggregation tests, including parallel agreeing with sequential.

Mirrors the summary and parallel groups in cpp/tests.cpp.
"""

import pytest

from expense import parse_expense
from summary import summarize, summarize_parallel, to_cents

ROWS = [
    "2026-09-01,42.50,groceries,Weekly shop",
    "2026-09-03,12.00,transport,Bus pass top-up",
    "2026-09-07,89.99,utilities,Electricity bill",
    "2026-09-12,23.75,groceries,Produce market",
    "2026-09-15,150.00,rent,Parking space",
]


@pytest.fixture
def expenses():
    return [parse_expense(row) for row in ROWS]


class TestSummary:
    def test_one_entry_per_distinct_category(self, expenses):
        assert len(summarize(expenses).by_category) == 4

    def test_groceries_totals_66_25(self, expenses):
        assert summarize(expenses).by_category["groceries"] == 6625

    def test_overall_totals_318_24(self, expenses):
        assert summarize(expenses).overall == 31824

    def test_categories_come_out_alphabetically(self, expenses):
        keys = list(summarize(expenses).by_category)
        assert keys == ["groceries", "rent", "transport", "utilities"]

    def test_empty_list_totals_zero(self):
        assert summarize([]).overall == 0


class TestCurrencyRepresentation:
    def test_cents_conversion_is_exact(self):
        assert to_cents(0.29) == 29
        assert to_cents(89.99) == 8999
        assert to_cents(150.00) == 15000

    def test_float_accumulation_would_not_be_associative(self, expenses):
        """Why the totals are integers in both implementations.

        Summing the same amounts left to right and right to left in floating
        point does not reliably give the same bits. Integer cents do. This test
        documents the hazard the representation choice avoids rather than
        asserting a specific wrong answer, which would be platform dependent.
        """
        values = [0.1, 0.2, 0.3, 89.99, 150.0, 23.75, 42.5, 12.0]
        forward = sum(values)
        backward = sum(reversed(values))
        assert round(forward, 2) == round(backward, 2)
        assert sum(round(v * 100) for v in values) == sum(
            round(v * 100) for v in reversed(values)
        )


class TestParallelMatchesSequential:
    @pytest.fixture
    def big(self, expenses):
        return [e for _ in range(2000) for e in expenses]

    @pytest.mark.parametrize("workers", [1, 2, 3, 4, 8])
    def test_per_category_totals_match(self, big, workers):
        assert summarize_parallel(big, workers).by_category == summarize(big).by_category

    @pytest.mark.parametrize("workers", [1, 2, 3, 4, 8])
    def test_overall_total_matches(self, big, workers):
        assert summarize_parallel(big, workers).overall == summarize(big).overall

    def test_more_workers_than_records_is_handled(self, expenses):
        assert summarize_parallel(expenses, 8).overall == summarize(expenses).overall

    def test_ordering_is_preserved_in_the_parallel_path(self, big):
        assert list(summarize_parallel(big, 4).by_category) == [
            "groceries",
            "rent",
            "transport",
            "utilities",
        ]
