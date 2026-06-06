#!/usr/bin/env python3
import argparse
import base64
import csv
import json
import os
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


PLOT_TITLES = {
    "strong_speedup.svg": "Strong scaling: speedup",
    "strong_efficiency.svg": "Strong scaling: eficienta",
    "strong_time.svg": "Strong scaling: timp total",
    "strong_compute_vs_comm.svg": "Strong scaling: calcul vs comunicatie",
    "weak_time.svg": "Weak scaling: timp total",
    "weak_compute_vs_comm.svg": "Weak scaling: calcul vs comunicatie",
}


def executable_name(name):
    return f"{name}.exe" if os.name == "nt" else name


def default_executable(build_dir, name):
    build_dir = Path(build_dir)
    candidates = [
        build_dir / executable_name(name),
        build_dir / "Release" / executable_name(name),
        build_dir / "Debug" / executable_name(name),
        build_dir / "RelWithDebInfo" / executable_name(name),
        build_dir / "MinSizeRel" / executable_name(name),
        build_dir / name,
        build_dir / "Release" / name,
        build_dir / "Debug" / name,
    ]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    return str(candidates[0])


def resolve_executable(path):
    candidate = Path(path)
    if candidate.exists():
        return str(candidate)
    resolved = shutil.which(path)
    return resolved if resolved else path


def command_text(command):
    return " ".join(str(part) for part in command)


def run_command(command, capture=False):
    command = [str(part) for part in command]
    print(command_text(command), flush=True)
    completed = subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
    )
    if completed.returncode != 0 and not capture:
        raise RuntimeError(f"command failed with exit code {completed.returncode}")
    return completed


def read_pgm(path):
    data = path.read_bytes()
    position = 0

    def read_token():
        nonlocal position
        while position < len(data):
            byte = data[position]
            if byte in b" \t\r\n":
                position += 1
                continue
            if byte == ord("#"):
                while position < len(data) and data[position] not in b"\r\n":
                    position += 1
                continue
            break

        start = position
        while position < len(data) and data[position] not in b" \t\r\n":
            position += 1
        return data[start:position].decode("ascii")

    magic = read_token()
    if magic != "P5":
        raise RuntimeError(f"{path} is not a binary PGM P5 file")

    width = int(read_token())
    height = int(read_token())
    max_value = int(read_token())
    if max_value != 255:
        raise RuntimeError(f"{path} uses unsupported max value {max_value}")

    if position < len(data) and data[position] in b" \t\r\n":
        position += 1

    pixels = data[position:]
    expected = width * height
    if len(pixels) != expected:
        raise RuntimeError(f"{path} has {len(pixels)} pixels, expected {expected}")

    return width, height, pixels


def frame_step(path):
    return int(path.stem.split("_")[-1])


def pack_frame(path):
    width, height, pixels = read_pgm(path)
    packed = bytearray((len(pixels) + 7) // 8)
    alive = 0
    for index, pixel in enumerate(pixels):
        if pixel < 128:
            packed[index // 8] |= 1 << (index % 8)
            alive += 1

    return {
        "step": frame_step(path),
        "alive": alive,
        "packed": base64.b64encode(packed).decode("ascii"),
    }, width, height


def clear_frames(frames_dir):
    frames_dir.mkdir(parents=True, exist_ok=True)
    for frame in frames_dir.glob("frame_*.pgm"):
        frame.unlink()


def generate_frames(args, serial_exe, frames_dir):
    clear_frames(frames_dir)
    command = [
        resolve_executable(serial_exe),
        "--width",
        args.demo_width,
        "--height",
        args.demo_height,
        "--steps",
        args.demo_steps,
        "--pattern",
        args.pattern,
        "--seed",
        args.seed,
        "--density",
        args.density,
        "--snapshot-interval",
        args.snapshot_interval,
        "--output",
        frames_dir,
    ]
    run_command(command)

    frame_paths = sorted(frames_dir.glob("frame_*.pgm"), key=frame_step)
    if not frame_paths:
        raise RuntimeError(f"no demo frames were written to {frames_dir}")
    return frame_paths


def encode_frames(frame_paths):
    frames = []
    width = None
    height = None
    for path in frame_paths:
        frame, frame_width, frame_height = pack_frame(path)
        if width is None:
            width = frame_width
            height = frame_height
        elif width != frame_width or height != frame_height:
            raise RuntimeError("all demo frames must have the same dimensions")
        frames.append(frame)

    return {
        "width": width or 0,
        "height": height or 0,
        "frames": frames,
    }


def read_csv_rows(path):
    if not path.exists():
        return []
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def copy_if_exists(source, destination):
    if not source.exists():
        return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return True


def refresh_plots(project_dir, results_dir, plots_dir):
    strong = results_dir / "summary_strong.csv"
    weak = results_dir / "summary_weak.csv"
    if not strong.exists() or not weak.exists():
        return

    run_command(
        [
            sys.executable,
            project_dir / "scripts" / "plot_results.py",
            "--results-dir",
            results_dir,
            "--output-dir",
            plots_dir,
        ]
    )


def copy_benchmark_artifacts(results_dir, plots_dir, demo_data_dir, demo_plots_dir):
    summaries = {}
    for name in ("summary_strong.csv", "summary_weak.csv"):
        source = results_dir / name
        copied = copy_if_exists(source, demo_data_dir / name)
        key = "strong" if "strong" in name else "weak"
        summaries[key] = read_csv_rows(source) if copied else []

    plots = []
    for source in sorted(plots_dir.glob("*.svg")):
        destination = demo_plots_dir / source.name
        shutil.copy2(source, destination)
        plots.append(
            {
                "title": PLOT_TITLES.get(source.name, source.stem.replace("_", " ")),
                "path": f"plots/{source.name}",
            }
        )

    return summaries, plots


def read_cmake_cache_value(build_dir, key):
    cache_path = Path(build_dir) / "CMakeCache.txt"
    if not cache_path.exists():
        return ""

    prefix = f"{key}:"
    for line in cache_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line.startswith(prefix):
            return line.split("=", 1)[1] if "=" in line else ""
    return ""


def detect_ctest_config(build_dir):
    configurations = read_cmake_cache_value(build_dir, "CMAKE_CONFIGURATION_TYPES")
    if not configurations:
        return ""

    available = [item.strip() for item in configurations.split(";") if item.strip()]
    for preferred in ("Release", "RelWithDebInfo", "Debug", "MinSizeRel"):
        if preferred in available:
            return preferred
    return available[0] if available else ""


def run_validation(args):
    command = ["ctest", "--test-dir", args.build_dir, "--output-on-failure"]
    config = args.config or detect_ctest_config(args.build_dir)
    if config:
        command.insert(3, "-C")
        command.insert(4, config)

    completed = run_command(command, capture=True)
    output = completed.stdout or ""
    lines = [line.rstrip() for line in output.splitlines() if line.strip()]
    summary = "\n".join(lines[-12:])
    if completed.returncode != 0 and "Unable to find executable" in output:
        summary += (
            "\nSugestie: compileaza configuratia CTest, de exemplu "
            "`cmake --build build --config Release`, sau refa folderul build "
            "cu acelasi generator pe care il folosesti la rulare."
        )
    return {
        "ran": True,
        "passed": completed.returncode == 0,
        "command": command_text(command),
        "returnCode": completed.returncode,
        "summary": summary,
    }


def run_benchmarks(args, project_dir, serial_exe, mpi_exe, results_dir):
    command = [
        sys.executable,
        project_dir / "scripts" / "run_benchmarks.py",
        "--serial-exe",
        serial_exe,
        "--mpi-exe",
        mpi_exe,
        "--mpiexec",
        args.mpiexec,
        "--processes",
        args.processes,
        "--steps",
        args.benchmark_steps,
        "--output-dir",
        results_dir,
    ]
    run_command(command)


def write_demo_data(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    text = "window.demoData = "
    text += json.dumps(data, indent=2, sort_keys=True)
    text += ";\n"
    path.write_text(text, encoding="utf-8")


def main(argv):
    parser = argparse.ArgumentParser(description="Prepare the static HTML demo.")
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--config", default="")
    parser.add_argument("--serial-exe", default="")
    parser.add_argument("--mpi-exe", default="")
    parser.add_argument("--mpiexec", default="mpiexec")
    parser.add_argument("--processes", default="1,4,8,16,32")
    parser.add_argument("--benchmark-steps", type=int, default=100)
    parser.add_argument("--results-dir", default="results")
    parser.add_argument("--plots-dir", default="plots")
    parser.add_argument("--output-dir", default="demo")
    parser.add_argument("--demo-width", type=int, default=96)
    parser.add_argument("--demo-height", type=int, default=96)
    parser.add_argument("--demo-steps", type=int, default=100)
    parser.add_argument("--snapshot-interval", type=int, default=1)
    parser.add_argument("--pattern", default="random")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--density", type=float, default=0.25)
    parser.add_argument("--skip-frames", action="store_true")
    parser.add_argument("--run-validation", action="store_true")
    parser.add_argument("--run-benchmarks", action="store_true")
    parser.add_argument("--no-refresh-plots", action="store_true")
    args = parser.parse_args(argv)

    project_dir = Path(__file__).resolve().parents[1]
    build_dir = Path(args.build_dir)
    results_dir = Path(args.results_dir)
    plots_dir = Path(args.plots_dir)
    output_dir = Path(args.output_dir)
    demo_data_dir = output_dir / "data"
    demo_frames_dir = output_dir / "frames"
    demo_plots_dir = output_dir / "plots"

    demo_data_dir.mkdir(parents=True, exist_ok=True)
    demo_plots_dir.mkdir(parents=True, exist_ok=True)

    serial_exe = args.serial_exe or default_executable(build_dir, "life_serial")
    mpi_exe = args.mpi_exe or default_executable(build_dir, "life_mpi")

    if args.run_benchmarks:
        run_benchmarks(args, project_dir, serial_exe, mpi_exe, results_dir)
        refresh_plots(project_dir, results_dir, plots_dir)
    elif not args.no_refresh_plots:
        refresh_plots(project_dir, results_dir, plots_dir)

    if args.skip_frames:
        frame_paths = sorted(demo_frames_dir.glob("frame_*.pgm"), key=frame_step)
    else:
        frame_paths = generate_frames(args, serial_exe, demo_frames_dir)

    encoded = encode_frames(frame_paths) if frame_paths else {"width": 0, "height": 0, "frames": []}
    summaries, plots = copy_benchmark_artifacts(results_dir, plots_dir, demo_data_dir, demo_plots_dir)

    validation = (
        run_validation(args)
        if args.run_validation
        else {
            "ran": False,
            "passed": None,
            "command": "",
            "returnCode": None,
            "summary": "Validarea nu a fost rulata in demo. Ruleaza cu --run-validation.",
        }
    )

    data = {
        "generatedAt": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "visual": {
            "width": encoded["width"],
            "height": encoded["height"],
            "steps": args.demo_steps,
            "pattern": args.pattern,
            "seed": args.seed,
            "density": args.density,
            "snapshotInterval": args.snapshot_interval,
        },
        "frames": encoded["frames"],
        "summaries": summaries,
        "plots": plots,
        "validation": validation,
    }

    write_demo_data(demo_data_dir / "demo-data.js", data)
    print(f"Demo written to {output_dir / 'index.html'}", flush=True)
    print(f"Demo data written to {demo_data_dir / 'demo-data.js'}", flush=True)


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
