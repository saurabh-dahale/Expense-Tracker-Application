"""Timing for summary versus summary --parallel.

The command-line program cannot show the concurrency result, because on a
large file nearly all of its run time is spent loading the CSV. This loads the
file once, untimed, and then times only summarize and summarize_parallel with
time.perf_counter. cpp/bench.cpp is the mirror of this file and prints the same
table, so the two can be read side by side.

Every parallel result is checked against the sequential one, and a mismatch
exits with status 1, so a timing is never reported for a wrong answer.

The GIL line matters for the report. Python 3.13 and later can be built
free-threaded, and on such a build the threads really do run in parallel, so
the "no speedup" prediction only holds where the GIL is enabled.

Usage: python3 bench.py [--file PATH] [--runs N] [--threads 1,2,4,8]
"""

from __future__ import annotations

import argparse
import os
import platform
import sys
import time
from typing import Callable, List, Tuple

from storage import StorageError, load
from summary import summarize, summarize_parallel


def time_it(runs: int, f: Callable[[], object]) -> Tuple[float, float]:
    """One untimed warm-up run, then `runs` timed ones, as (best, median) ms."""
    f()
    samples: List[float] = []
    for _ in range(runs):
        start = time.perf_counter()
        f()
        samples.append((time.perf_counter() - start) * 1000.0)
    samples.sort()
    return samples[0], samples[len(samples) // 2]


def print_row(label: str, stats: Tuple[float, float], baseline: float) -> None:
    best, median = stats
    print(f"{label:<14} {best:10.2f} {median:10.2f} {baseline / best:8.2f}x")


def gil_status() -> str:
    check = getattr(sys, "_is_gil_enabled", None)
    if check is None:
        return "enabled (no free-threaded support in this build)"
    return "enabled" if check() else "DISABLED (free-threaded build)"


def main(argv: List[str]) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description="Time summary versus summary --parallel.")
    parser.add_argument("--file", default=os.path.join(here, "..", "spec", "expenses_large.csv"))
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--threads", default="1,2,4,8")
    args = parser.parse_args(argv[1:])

    if args.runs < 1:
        print("Error: --runs must be at least 1", file=sys.stderr)
        return 1
    try:
        threads = [int(t) for t in args.threads.split(",")]
    except ValueError:
        print("Error: --threads must be a comma-separated list of integers", file=sys.stderr)
        return 1
    if any(t < 1 for t in threads):
        print("Error: thread counts must be at least 1", file=sys.stderr)
        return 1

    load_start = time.perf_counter()
    try:
        loaded = load(args.file)
    except StorageError as exc:
        print("Error: " + str(exc), file=sys.stderr)
        return 1
    load_ms = (time.perf_counter() - load_start) * 1000.0
    rows = loaded.expenses

    print(f"language       Python {platform.python_version()} (ThreadPoolExecutor)")
    print(f"file           {args.file}")
    print(f"rows           {len(rows)} (skipped {loaded.skipped})")
    print(f"hardware       {os.cpu_count()} logical cores")
    print(f"GIL            {gil_status()}")
    print(f"load           {load_ms:.2f} ms (not part of the timings below)")
    print(f"runs           {args.runs} timed, after 1 warm-up")
    print()
    print(f"{'mode':<14} {'best ms':>10} {'median ms':>10} {'speedup':>9}")

    expected = summarize(rows)
    seq = time_it(args.runs, lambda: summarize(rows))
    print_row("sequential", seq, seq[0])

    for n in threads:
        if summarize_parallel(rows, n) != expected:
            print(f"Error: parallel result with {n} threads differs from sequential",
                  file=sys.stderr)
            return 1
        par = time_it(args.runs, lambda: summarize_parallel(rows, n))
        print_row(f"parallel x{n}", par, seq[0])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
