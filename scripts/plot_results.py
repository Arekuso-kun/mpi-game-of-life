#!/usr/bin/env python3
import argparse
import csv
import html
import math
import sys
from pathlib import Path


WIDTH = 920
HEIGHT = 600
LEFT = 86
RIGHT = 36
TOP = 84
BOTTOM = 82
PLOT_WIDTH = WIDTH - LEFT - RIGHT
PLOT_HEIGHT = HEIGHT - TOP - BOTTOM

BLUE = "#2563eb"
GREEN = "#16a34a"
ORANGE = "#f97316"
GRAY = "#64748b"
LIGHT_GRAY = "#e2e8f0"
TEXT = "#0f172a"


def read_rows(path):
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def as_float(row, key):
    return float(row[key])


def as_int(row, key):
    return int(row[key])


def escape(value):
    return html.escape(str(value), quote=True)


def nice_max(value):
    if value <= 0:
        return 1.0
    exponent = math.floor(math.log10(value))
    fraction = value / (10 ** exponent)
    if fraction <= 1:
        nice_fraction = 1
    elif fraction <= 2:
        nice_fraction = 2
    elif fraction <= 5:
        nice_fraction = 5
    else:
        nice_fraction = 10
    return nice_fraction * (10 ** exponent)


def y_ticks(max_value, count=5):
    top = nice_max(max_value * 1.12)
    step = top / count
    return [step * index for index in range(count + 1)]


def format_tick(value):
    if abs(value) >= 10:
        return f"{value:.0f}"
    if abs(value) >= 1:
        return f"{value:.1f}".rstrip("0").rstrip(".")
    return f"{value:.2f}".rstrip("0").rstrip(".")


def svg_document(title, subtitle, body):
    return "\n".join(
        [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
            '<rect width="100%" height="100%" fill="#ffffff"/>',
            f'<text x="{WIDTH / 2}" y="28" text-anchor="middle" font-family="Arial, sans-serif" font-size="22" font-weight="700" fill="{TEXT}">{escape(title)}</text>',
            f'<text x="{WIDTH / 2}" y="54" text-anchor="middle" font-family="Arial, sans-serif" font-size="14" fill="{GRAY}">{escape(subtitle)}</text>',
            body,
            "</svg>",
            "",
        ]
    )


def x_position(index, count, centered=False):
    if centered:
        slot_width = PLOT_WIDTH / max(count, 1)
        return LEFT + slot_width * index + slot_width / 2
    return LEFT + (PLOT_WIDTH * index / max(count - 1, 1) if count > 1 else PLOT_WIDTH / 2)


def draw_axes(x_labels, max_value, y_label, centered_x=False):
    parts = []
    ticks = y_ticks(max_value)
    top_tick = ticks[-1]

    parts.append(f'<line x1="{LEFT}" y1="{TOP}" x2="{LEFT}" y2="{TOP + PLOT_HEIGHT}" stroke="{TEXT}" stroke-width="1.5"/>')
    parts.append(f'<line x1="{LEFT}" y1="{TOP + PLOT_HEIGHT}" x2="{LEFT + PLOT_WIDTH}" y2="{TOP + PLOT_HEIGHT}" stroke="{TEXT}" stroke-width="1.5"/>')

    for tick in ticks:
        y = TOP + PLOT_HEIGHT - (tick / top_tick) * PLOT_HEIGHT
        parts.append(f'<line x1="{LEFT}" y1="{y:.2f}" x2="{LEFT + PLOT_WIDTH}" y2="{y:.2f}" stroke="{LIGHT_GRAY}" stroke-width="1"/>')
        parts.append(f'<text x="{LEFT - 12}" y="{y + 4:.2f}" text-anchor="end" font-family="Arial, sans-serif" font-size="12" fill="{GRAY}">{format_tick(tick)}</text>')

    count = max(len(x_labels), 1)
    for index, label in enumerate(x_labels):
        x = x_position(index, count, centered_x)
        parts.append(f'<line x1="{x:.2f}" y1="{TOP + PLOT_HEIGHT}" x2="{x:.2f}" y2="{TOP + PLOT_HEIGHT + 6}" stroke="{TEXT}" stroke-width="1"/>')
        parts.append(f'<text x="{x:.2f}" y="{TOP + PLOT_HEIGHT + 28}" text-anchor="middle" font-family="Arial, sans-serif" font-size="13" fill="{TEXT}">{escape(label)}</text>')

    parts.append(f'<text x="{LEFT + PLOT_WIDTH / 2}" y="{HEIGHT - 24}" text-anchor="middle" font-family="Arial, sans-serif" font-size="14" fill="{TEXT}">Procese MPI</text>')
    parts.append(f'<text transform="translate(24 {TOP + PLOT_HEIGHT / 2}) rotate(-90)" text-anchor="middle" font-family="Arial, sans-serif" font-size="14" fill="{TEXT}">{escape(y_label)}</text>')

    return parts, top_tick


def line_chart(title, subtitle, rows, value_key, y_label, output_path, ideal=None):
    rows = sorted(rows, key=lambda row: as_int(row, "processes"))
    labels = [row["processes"] for row in rows]
    values = [as_float(row, value_key) for row in rows]
    ideal_values = [ideal(as_int(row, "processes")) for row in rows] if ideal else []
    max_value = max(values + ideal_values + [1.0])

    parts, top_tick = draw_axes(labels, max_value, y_label)

    def point(index, value):
        count = max(len(rows), 1)
        x = x_position(index, count)
        y = TOP + PLOT_HEIGHT - (value / top_tick) * PLOT_HEIGHT
        return x, y

    points = [point(index, value) for index, value in enumerate(values)]
    path = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
    parts.append(f'<polyline points="{path}" fill="none" stroke="{BLUE}" stroke-width="3"/>')

    for index, value in enumerate(values):
        x, y = point(index, value)
        parts.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="5" fill="{BLUE}"/>')
        parts.append(f'<text x="{x:.2f}" y="{y - 10:.2f}" text-anchor="middle" font-family="Arial, sans-serif" font-size="12" fill="{TEXT}">{format_tick(value)}</text>')

    if ideal:
        ideal_points = [point(index, value) for index, value in enumerate(ideal_values)]
        ideal_path = " ".join(f"{x:.2f},{y:.2f}" for x, y in ideal_points)
        parts.append(f'<polyline points="{ideal_path}" fill="none" stroke="{GRAY}" stroke-width="2.5" stroke-dasharray="7 6"/>')
        parts.append(f'<text x="{LEFT + PLOT_WIDTH - 10}" y="{TOP + 22}" text-anchor="end" font-family="Arial, sans-serif" font-size="13" fill="{GRAY}">ideal</text>')

    output_path.write_text(svg_document(title, subtitle, "\n".join(parts)), encoding="utf-8")


def stacked_bar_chart(title, subtitle, rows, output_path):
    rows = sorted(rows, key=lambda row: as_int(row, "processes"))
    labels = [row["processes"] for row in rows]
    totals = [
        as_float(row, "computation_seconds") + as_float(row, "communication_seconds")
        for row in rows
    ]
    max_value = max(totals + [1.0])
    parts, top_tick = draw_axes(labels, max_value, "Timp (secunde)", centered_x=True)

    group_count = max(len(rows), 1)
    slot_width = PLOT_WIDTH / group_count
    bar_width = min(70, slot_width * 0.48)

    for index, row in enumerate(rows):
        center = x_position(index, group_count, centered=True)
        comp = as_float(row, "computation_seconds")
        comm = as_float(row, "communication_seconds")
        comp_height = (comp / top_tick) * PLOT_HEIGHT
        comm_height = (comm / top_tick) * PLOT_HEIGHT
        base_y = TOP + PLOT_HEIGHT
        comp_y = base_y - comp_height
        comm_y = comp_y - comm_height

        parts.append(f'<rect x="{center - bar_width / 2:.2f}" y="{comp_y:.2f}" width="{bar_width:.2f}" height="{comp_height:.2f}" fill="{GREEN}"/>')
        parts.append(f'<rect x="{center - bar_width / 2:.2f}" y="{comm_y:.2f}" width="{bar_width:.2f}" height="{comm_height:.2f}" fill="{ORANGE}"/>')
        parts.append(f'<text x="{center:.2f}" y="{comm_y - 8:.2f}" text-anchor="middle" font-family="Arial, sans-serif" font-size="12" fill="{TEXT}">{format_tick(comp + comm)}</text>')

    legend_x = LEFT + PLOT_WIDTH - 220
    legend_y = TOP + 8
    parts.append(f'<rect x="{legend_x}" y="{legend_y}" width="14" height="14" fill="{GREEN}"/>')
    parts.append(f'<text x="{legend_x + 22}" y="{legend_y + 12}" font-family="Arial, sans-serif" font-size="13" fill="{TEXT}">calcul</text>')
    parts.append(f'<rect x="{legend_x + 96}" y="{legend_y}" width="14" height="14" fill="{ORANGE}"/>')
    parts.append(f'<text x="{legend_x + 118}" y="{legend_y + 12}" font-family="Arial, sans-serif" font-size="13" fill="{TEXT}">comunicatie</text>')

    output_path.write_text(svg_document(title, subtitle, "\n".join(parts)), encoding="utf-8")


def mpi_rows(rows):
    return [row for row in rows if row["implementation"].startswith("mpi")]


def main(argv):
    parser = argparse.ArgumentParser(description="Generate SVG plots from benchmark CSV files.")
    parser.add_argument("--results-dir", default="results")
    parser.add_argument("--output-dir", default="plots")
    parser.add_argument("--strong-summary", default="")
    parser.add_argument("--weak-summary", default="")
    args = parser.parse_args(argv)

    results_dir = Path(args.results_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    strong_path = Path(args.strong_summary) if args.strong_summary else results_dir / "summary_strong.csv"
    weak_path = Path(args.weak_summary) if args.weak_summary else results_dir / "summary_weak.csv"

    strong = mpi_rows(read_rows(strong_path))
    weak = mpi_rows(read_rows(weak_path))

    if not strong:
        raise RuntimeError(f"no MPI rows found in {strong_path}")
    if not weak:
        raise RuntimeError(f"no MPI rows found in {weak_path}")

    line_chart(
        "Strong Scaling: Speedup",
        "Accelerarea obtinuta prin cresterea numarului de procese pentru aceeasi dimensiune a grilei.",
        strong,
        "speedup",
        "Speedup",
        output_dir / "strong_speedup.svg",
        ideal=lambda processes: processes,
    )
    line_chart(
        "Strong Scaling: Eficienta",
        "Raportul dintre speedup si numarul de procese, indicand utilizarea resurselor paralele.",
        strong,
        "efficiency",
        "Eficienta",
        output_dir / "strong_efficiency.svg",
        ideal=lambda _processes: 1.0,
    )
    line_chart(
        "Strong Scaling: Timp Total",
        "Evolutia timpului de executie pentru o problema fixa rulata pe mai multe procese.",
        strong,
        "total_seconds",
        "Timp (secunde)",
        output_dir / "strong_time.svg",
    )
    stacked_bar_chart(
        "Strong Scaling: Calcul vs Comunicatie",
        "Comparatie intre timpul de calcul local si timpul de comunicare intre procese.",
        strong,
        output_dir / "strong_compute_vs_comm.svg",
    )
    line_chart(
        "Weak Scaling: Timp Total",
        "Evolutia timpului de executie cand dimensiunea problemei creste proportional cu numarul de procese.",
        weak,
        "total_seconds",
        "Timp (secunde)",
        output_dir / "weak_time.svg",
    )
    stacked_bar_chart(
        "Weak Scaling: Calcul vs Comunicatie",
        "Impactul comunicarii intre procese asupra timpului total in scenariul de weak scaling.",
        weak,
        output_dir / "weak_compute_vs_comm.svg",
    )

    print(f"Plots written to {output_dir}", flush=True)


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
