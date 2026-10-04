#!/usr/bin/env python3
"""gen_large_csv.py - synthetic fixture for timing summary --parallel.

The shared fixture has five rows, which is far too small for thread start-up
cost to be anything but the whole measurement. This writes a large file in the
exact shared CSV format, so both implementations load it without a single
skipped row and the timing comparison is between the summaries alone.

The output is deterministic for a given --seed, so a C++ run and a Python run
read byte-identical input, and a timing can be reproduced later.

Usage: spec/gen_large_csv.py [--rows N] [--seed S] [--out PATH]
"""

import argparse
import random
import sys

CATEGORIES = [
    "groceries", "transport", "utilities", "rent", "dining", "health",
    "entertainment", "education", "clothing", "travel", "insurance", "gifts",
]

WORDS = [
    "weekly", "shop", "bus", "pass", "top-up", "electricity", "bill", "produce",
    "market", "parking", "space", "coffee", "lunch", "dinner", "pharmacy",
    "textbook", "ticket", "fuel", "subscription", "repair", "online", "order",
]

DAYS_IN_MONTH = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rows", type=int, default=1_000_000)
    parser.add_argument("--seed", type=int, default=2026)
    parser.add_argument("--out", default="spec/expenses_large.csv")
    args = parser.parse_args(argv[1:])

    if args.rows < 0:
        print("Error: --rows must be non-negative", file=sys.stderr)
        return 1

    rng = random.Random(args.seed)
    with open(args.out, "w", newline="") as out:
        out.write("date,amount,category,description\n")
        batch = []
        for _ in range(args.rows):
            # 2026 is not a leap year, so the fixed table is always valid.
            month = rng.randint(1, 12)
            day = rng.randint(1, DAYS_IN_MONTH[month - 1])
            # Amounts are drawn in whole cents, so every value has an exact
            # two-place decimal form and nothing depends on float rounding.
            cents = rng.randint(1, 50_000)
            category = rng.choice(CATEGORIES)
            description = " ".join(rng.choices(WORDS, k=rng.randint(1, 4))).capitalize()
            batch.append(f"2026-{month:02d}-{day:02d},{cents // 100}.{cents % 100:02d},"
                         f"{category},{description}\n")
            if len(batch) >= 10_000:
                out.writelines(batch)
                batch.clear()
        out.writelines(batch)

    print(f"wrote {args.rows} rows to {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
