"""Filter and search tests. Mirrors the filter and search groups in cpp/tests.cpp."""

import pytest

from expense import parse_date, parse_expense, shared_object_count
from queries import FilterCriteria, filter_expenses, search

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


class TestFilterBoundaries:
    def test_range_endpoints_are_inclusive(self, expenses):
        criteria = FilterCriteria(
            date_from=parse_date("2026-09-03"), date_to=parse_date("2026-09-12")
        )
        assert len(filter_expenses(expenses, criteria)) == 3

    def test_single_day_range_matches_that_day(self, expenses):
        criteria = FilterCriteria(
            date_from=parse_date("2026-09-07"), date_to=parse_date("2026-09-07")
        )
        assert len(filter_expenses(expenses, criteria)) == 1

    def test_unmatched_category_yields_no_rows(self, expenses):
        assert filter_expenses(expenses, FilterCriteria(category="nosuch")) == []

    def test_criteria_combine_with_and(self, expenses):
        criteria = FilterCriteria(
            date_from=parse_date("2026-09-01"),
            date_to=parse_date("2026-09-07"),
            category="groceries",
        )
        assert len(filter_expenses(expenses, criteria)) == 1

    def test_no_criteria_matches_everything(self, expenses):
        assert len(filter_expenses(expenses, FilterCriteria())) == len(expenses)


class TestSearch:
    def test_search_is_case_insensitive(self, expenses):
        assert len(search(expenses, "BUS")) == 1

    def test_search_matches_lowercase(self, expenses):
        assert len(search(expenses, "bus")) == 1

    def test_search_matches_a_substring_anywhere(self, expenses):
        assert len(search(expenses, "o")) == 3

    def test_unmatched_term_yields_no_rows(self, expenses):
        assert search(expenses, "xyzzy") == []

    def test_case_folding_is_ascii_only(self):
        # Matches the C++ std::tolower behaviour, so both programs print the
        # same rows. Unicode-aware str.lower() would match the second case.
        rows = [parse_expense("2026-09-01,1.00,misc,Caf\u00c9 ECLAIR")]
        assert len(search(rows, "ECLAIR")) == 1
        assert len(search(rows, "Caf\u00c9")) == 1
        assert search(rows, "caf\u00e9") == []


class TestReferenceSemantics:
    """The measurement behind the memory-management section.

    The C++ filter copies every match into a fresh vector, which its copy
    counters record. Here the result list holds the very same dicts, which is
    asserted by identity rather than by equality so the test cannot pass by
    accident on two equal-but-distinct records.
    """

    def test_filter_results_share_objects_with_the_source(self, expenses):
        result = filter_expenses(expenses, FilterCriteria(category="groceries"))
        assert shared_object_count(expenses, result) == len(result) == 2

    def test_mutating_a_filtered_row_changes_the_source_row(self, expenses):
        result = filter_expenses(expenses, FilterCriteria(category="rent"))
        result[0]["description"] = "Changed"
        assert expenses[4]["description"] == "Changed"

    def test_search_results_share_objects_with_the_source(self, expenses):
        result = search(expenses, "bill")
        assert shared_object_count(expenses, result) == len(result) == 1
