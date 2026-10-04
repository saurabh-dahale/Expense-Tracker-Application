"""Parsing and formatting tests. Mirrors the first two groups in cpp/tests.cpp."""

from datetime import date

from expense import format_date, parse_amount, parse_date, parse_expense, to_csv
from formatting import SEPARATOR, format_row, format_summary_line


class TestDateParsing:
    def test_accepts_a_valid_date(self):
        assert parse_date("2026-09-01") == date(2026, 9, 1)

    def test_rejects_february_30(self):
        assert parse_date("2026-02-30") is None

    def test_accepts_a_leap_day(self):
        assert parse_date("2024-02-29") == date(2024, 2, 29)

    def test_rejects_a_non_leap_february_29(self):
        assert parse_date("2026-02-29") is None

    def test_rejects_month_13(self):
        assert parse_date("2026-13-01") is None

    def test_rejects_an_unpadded_month(self):
        # strptime alone accepts this. The shape check is what rejects it, and
        # what keeps this implementation in agreement with the C++ one.
        assert parse_date("2026-9-01") is None

    def test_rejects_a_missing_separator(self):
        assert parse_date("20260901") is None

    def test_format_zero_pads_the_year(self):
        # strftime("%Y") gives "1" on glibc; the saved CSV would then fail to
        # reload. isoformat() pads on every platform.
        assert format_date(date(1, 1, 1)) == "0001-01-01"


class TestAmountParsing:
    def test_accepts_two_decimal_places(self):
        assert parse_amount("42.50") == 42.50

    def test_rejects_one_decimal_place(self):
        assert parse_amount("42.5") is None

    def test_rejects_scientific_notation(self):
        # float("1e3") would succeed. The specification does not allow it.
        assert parse_amount("1e3") is None

    def test_rejects_a_leading_dot(self):
        assert parse_amount(".50") is None

    def test_rejects_a_negative_amount(self):
        assert parse_amount("-5.00") is None


class TestRowParsing:
    def test_rejects_an_amount_with_one_decimal_place(self):
        assert parse_expense("2026-09-01,42.5,groceries,Weekly shop") is None

    def test_rejects_a_negative_amount(self):
        assert parse_expense("2026-09-01,-5.00,groceries,Refund") is None

    def test_rejects_a_non_lowercase_category(self):
        assert parse_expense("2026-09-01,42.50,Groceries,Weekly shop") is None

    def test_rejects_a_row_with_too_few_fields(self):
        assert parse_expense("2026-09-01,42.50,groceries") is None

    def test_rejects_an_empty_line(self):
        assert parse_expense("") is None

    def test_accepts_a_description_containing_spaces(self):
        parsed = parse_expense("2026-09-01,42.50,groceries,Weekly shop at the market")
        assert parsed is not None
        assert parsed["description"] == "Weekly shop at the market"

    def test_round_trips_through_csv(self):
        line = "2026-09-01,42.50,groceries,Weekly shop"
        assert to_csv(parse_expense(line)) == line


class TestFormatting:
    def test_row_matches_the_output_contract(self):
        expense = parse_expense("2026-09-01,42.50,groceries,Weekly shop")
        assert format_row(expense) == "2026-09-01      42.50  groceries    Weekly shop"

    def test_no_trailing_whitespace(self):
        expense = parse_expense("2026-09-01,42.50,groceries,Weekly shop")
        row = format_row(expense)
        assert row == row.rstrip()

    def test_summary_line_matches_the_output_contract(self):
        assert format_summary_line("TOTAL", 318.24) == "TOTAL             318.24"

    def test_separator_is_24_characters(self):
        assert len(SEPARATOR) == 24
