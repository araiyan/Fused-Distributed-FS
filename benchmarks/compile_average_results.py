#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path
from statistics import mean

OP_CONFIG = [
    ("mkdir", "MKDIR"),
    ("create", "CREATE"),
    ("write", "WRITE+SYNC"),
    ("read", "READ"),
    ("ls", "LS"),
    ("rm", "RM FILE"),
    ("rmdir", "RMDIR"),
]


def parse_ns_values(csv_path: Path):
    values = []
    with csv_path.open("r", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if not header:
            return values

        # Expect either: iteration,latency_ns or raw iteration,ns rows
        latency_idx = 1
        if len(header) > 1 and "latency" in header[1].lower():
            latency_idx = 1

        for row in reader:
            if len(row) <= latency_idx:
                continue
            try:
                value = float(row[latency_idx])
                if value < 0:
                    continue
                values.append(value)
            except ValueError:
                continue
    return values


def find_csv(op_name: str, search_dirs):
    filename = f"{op_name}.csv"
    for d in search_dirs:
        candidate = d / filename
        if candidate.exists():
            return candidate
    return None


def build_report(search_dirs):
    op_results = []
    run_counts = []

    for op_name, display in OP_CONFIG:
        csv_file = find_csv(op_name, search_dirs)
        if not csv_file:
            op_results.append((display, None, 0, None))
            continue

        ns_values = parse_ns_values(csv_file)
        if not ns_values:
            op_results.append((display, None, 0, csv_file))
            continue

        avg_ms = mean(ns_values) / 1_000_000.0
        op_results.append((display, avg_ms, len(ns_values), csv_file))
        run_counts.append(len(ns_values))

    runs_label = max(run_counts) if run_counts else 0

    lines = []
    lines.append(f"--- RESULTS (Average over {runs_label} runs) ---")

    for display, avg_ms, _, _ in op_results:
        if avg_ms is None:
            lines.append(f"Average {display + ':':<16} N/A")
        else:
            lines.append(f"Average {display + ':':<16} {avg_ms:8.2f} ms")

    lines.append("")
    lines.append("--- DATA SOURCES ---")
    for display, avg_ms, count, source in op_results:
        if source is None:
            lines.append(f"{display:<12} missing")
        else:
            status = "ok" if avg_ms is not None else "empty"
            lines.append(f"{display:<12} {status:<5} {count:>4} runs  {source}")

    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description="Compile operation CSVs into average results page")
    parser.add_argument(
        "--input-dir",
        action="append",
        default=[],
        help="Directory containing operation CSV files (can be repeated)",
    )
    parser.add_argument(
        "--output",
        default="benchmarks/average_results.txt",
        help="Output file for formatted average results",
    )
    args = parser.parse_args()

    if args.input_dir:
        search_dirs = [Path(p) for p in args.input_dir]
    else:
        search_dirs = [
            Path("benchmarks/fused_ops_results_local"),
            Path("scripts/microbench_results"),
        ]

    report = build_report(search_dirs)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(report, encoding="utf-8")

    print(report, end="")
    print(f"Saved report to: {output_path}")


if __name__ == "__main__":
    main()
