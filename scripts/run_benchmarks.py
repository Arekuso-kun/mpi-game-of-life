#!/usr/bin/env python3
import argparse
import csv
import os
import shutil
import subprocess
import sys
from pathlib import Path


def parse_processes(value):
    processes = []
    for item in value.split(","):
        item = item.strip()
        if not item:
            continue
        process_count = int(item)
        if process_count <= 0:
            raise argparse.ArgumentTypeError("process counts must be positive")
        processes.append(process_count)
    if not processes:
        raise argparse.ArgumentTypeError("at least one process count is required")
    return processes


def resolve_executable(path, env=None):
    candidate = Path(path)
    if candidate.exists():
        return str(candidate)
    search_path = None if env is None else env.get("PATH")
    resolved = shutil.which(path, path=search_path)
    return resolved if resolved else path


def remove_if_exists(path):
    if path.exists():
        path.unlink()


def run_command(command, env):
    print(" ".join(command), flush=True)
    completed = subprocess.run(command, env=env)
    if completed.returncode != 0:
        raise RuntimeError(f"command failed with exit code {completed.returncode}")


def run_serial(args, env, output_csv):
    command = [
        resolve_executable(args.serial_exe, env),
        "--width",
        str(args.strong_width),
        "--height",
        str(args.strong_height),
        "--steps",
        str(args.steps),
        "--pattern",
        args.pattern,
        "--seed",
        str(args.seed),
        "--density",
        str(args.density),
        "--snapshot-interval",
        "0",
        "--csv",
        str(output_csv),
    ]
    run_command(command, env)


def run_mpi(args, env, output_csv, processes, width, height):
    command = [
        resolve_executable(args.mpiexec, env),
        args.process_flag,
        str(processes),
        resolve_executable(args.mpi_exe, env),
        "--width",
        str(width),
        "--height",
        str(height),
        "--steps",
        str(args.steps),
        "--pattern",
        args.pattern,
        "--seed",
        str(args.seed),
        "--density",
        str(args.density),
        "--snapshot-interval",
        "0",
        "--csv",
        str(output_csv),
    ]
    run_command(command, env)


def read_rows(path):
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def write_summary(input_csv, output_csv, scaling):
    rows = read_rows(input_csv)
    if not rows:
        return

    if scaling == "strong":
        mpi_rows = [row for row in rows if row["implementation"].startswith("mpi")]
        baseline = next((row for row in mpi_rows if int(row["processes"]) == 1), None)
        if baseline is None:
            raise RuntimeError("strong scaling summary requires an MPI run with processes=1")
        baseline_time = float(baseline["total_seconds"])
    else:
        baseline_time = float(rows[0]["total_seconds"])

    fieldnames = list(rows[0].keys()) + ["scaling", "speedup", "efficiency"]
    with output_csv.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            processes = int(row["processes"])
            total_seconds = float(row["total_seconds"])
            if scaling == "strong" and not row["implementation"].startswith("mpi"):
                speedup = 0.0
                efficiency = 0.0
            else:
                speedup = baseline_time / total_seconds if total_seconds > 0.0 else 0.0
                efficiency = speedup / processes if processes > 0 else 0.0

            output = dict(row)
            output["scaling"] = scaling
            output["speedup"] = f"{speedup:.10g}"
            output["efficiency"] = f"{efficiency:.10g}"
            writer.writerow(output)


def main(argv):
    parser = argparse.ArgumentParser(
        description="Run strong and weak scaling benchmarks for MPI Game of Life."
    )
    parser.add_argument("--mpi-exe", default="build/life_mpi.exe")
    parser.add_argument("--serial-exe", default="build/life_serial.exe")
    parser.add_argument("--mpiexec", default="mpiexec")
    parser.add_argument("--process-flag", default="-n")
    parser.add_argument("--processes", type=parse_processes, default=parse_processes("1,4,8,16,32"))
    parser.add_argument("--steps", type=int, default=100)
    parser.add_argument("--strong-width", type=int, default=1000)
    parser.add_argument("--strong-height", type=int, default=1000)
    parser.add_argument("--weak-width", type=int, default=1000)
    parser.add_argument("--weak-height-per-process", type=int, default=1000)
    parser.add_argument("--pattern", default="random")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--density", type=float, default=0.25)
    parser.add_argument("--output-dir", default="results")
    parser.add_argument("--skip-serial", action="store_true")
    parser.add_argument("--skip-strong", action="store_true")
    parser.add_argument("--skip-weak", action="store_true")
    args = parser.parse_args(argv)

    if args.steps <= 0:
        raise ValueError("--steps must be positive")
    if args.strong_width <= 0 or args.strong_height <= 0:
        raise ValueError("strong scaling dimensions must be positive")
    if args.weak_width <= 0 or args.weak_height_per_process <= 0:
        raise ValueError("weak scaling dimensions must be positive")

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()

    strong_csv = output_dir / "results_strong.csv"
    weak_csv = output_dir / "results_weak.csv"
    strong_summary_csv = output_dir / "summary_strong.csv"
    weak_summary_csv = output_dir / "summary_weak.csv"

    if not args.skip_strong:
        remove_if_exists(strong_csv)
        remove_if_exists(strong_summary_csv)
        if not args.skip_serial:
            run_serial(args, env, strong_csv)
        for processes in args.processes:
            run_mpi(args, env, strong_csv, processes, args.strong_width, args.strong_height)
        write_summary(strong_csv, strong_summary_csv, "strong")

    if not args.skip_weak:
        remove_if_exists(weak_csv)
        remove_if_exists(weak_summary_csv)
        for processes in args.processes:
            run_mpi(
                args,
                env,
                weak_csv,
                processes,
                args.weak_width,
                args.weak_height_per_process * processes,
            )
        write_summary(weak_csv, weak_summary_csv, "weak")

    print(f"Results written to {output_dir}", flush=True)


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
