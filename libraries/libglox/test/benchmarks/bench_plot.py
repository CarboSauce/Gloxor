#!/usr/bin/env python3

import argparse
import json
import re
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ALLOCATOR_PATTERN = re.compile(
    r"BM_(Glox|KHeap|Mimalloc)_",
    re.IGNORECASE,
)

FASTPATH_PATTERN = re.compile(r"SingleThreaded_FastPath")
SIZE_PATTERN = re.compile(r"/(\d+)(?:/|$)")


def load_google_benchmark_json(json_path: Path) -> pd.DataFrame:
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    benchmarks = data.get("benchmarks", [])

    if not benchmarks:
        raise ValueError(
            f"No benchmark entries found in {json_path.name}"
        )

    return pd.DataFrame(benchmarks)


def identify_allocator(name: str, filename: str) -> str:
    """
    Prefer the allocator encoded in the benchmark name.
    Fall back to the JSON filename for older benchmark output.
    """

    match = ALLOCATOR_PATTERN.search(name)

    if match:
        allocator = match.group(1)
        return allocator.capitalize()

    filename_lower = filename.lower()

    if "kheap" in filename_lower:
        return "KHeap"

    if "glox" in filename_lower:
        return "Glox"

    if "mimalloc" in filename_lower:
        return "Mimalloc"

    return Path(filename).stem


def parse_benchmark_jsons(results_dir: Path) -> pd.DataFrame | None:
    data_frames = []

    for json_file in sorted(results_dir.glob("*.json")):
        try:
            df = load_google_benchmark_json(json_file)

            if "name" not in df.columns:
                print(
                    f"Warning: No benchmark names in '{json_file.name}'"
                )
                continue

            # Keep only single-threaded fast-path benchmarks.
            fastpath_df = df[
                df["name"].astype(str).str.contains(
                    FASTPATH_PATTERN,
                    na=False,
                )
            ].copy()

            if fastpath_df.empty:
                print(
                    f"Warning: No SingleThreaded_FastPath benchmarks "
                    f"in '{json_file.name}'"
                )
                continue

            # Extract allocation size from names such as:
            # BM_Glox_SingleThreaded_FastPath/64
            # BM_KHeap_SingleThreaded_FastPath/128
            fastpath_df["size"] = (
                fastpath_df["name"]
                .astype(str)
                .str.extract(SIZE_PATTERN)[0]
                .astype(float)
            )

            fastpath_df = fastpath_df.dropna(subset=["size"])

            # Identify allocator per benchmark entry.
            # This supports multiple allocators in one JSON file.
            fastpath_df["allocator"] = fastpath_df["name"].apply(
                lambda name: identify_allocator(
                    str(name),
                    json_file.name,
                )
            )

            data_frames.append(fastpath_df)

        except Exception as e:
            print(
                f"Warning: Failed to parse '{json_file.name}': {e}"
            )

    if not data_frames:
        return None

    return pd.concat(data_frames, ignore_index=True)


def plot_comparison(
    df: pd.DataFrame,
    output_file: Path,
) -> None:
    plt.figure(figsize=(10, 6))

    for allocator, group in df.groupby("allocator"):
        group = group.sort_values("size")

        # Average repeated measurements for each allocation size.
        grouped = (
            group.groupby("size", as_index=False)["real_time"]
            .mean()
        )

        plt.plot(
            grouped["size"],
            grouped["real_time"],
            marker="o",
            linewidth=2,
            label=allocator,
        )

    plt.xscale("log", base=2)

    plt.xlabel("Allocation Size (Bytes)")
    plt.ylabel("Latency (ns / op)")
    plt.title("Fast-Path Allocation + Deallocation Latency")

    plt.grid(
        True,
        which="both",
        linestyle="--",
        alpha=0.5,
    )

    plt.legend()
    plt.tight_layout()

    plt.savefig(output_file, dpi=300)

    print(
        f"[+] Successfully saved plot to: '{output_file}'"
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Parse Google Benchmark JSON results and compare "
            "Glox, KHeap, and Mimalloc."
        )
    )

    parser.add_argument(
        "directory",
        type=Path,
        help="Directory containing benchmark JSON results",
    )

    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("allocator_comparison.png"),
        help="Output path for the generated plot",
    )

    args = parser.parse_args()

    if not args.directory.is_dir():
        print(
            f"Error: Directory '{args.directory}' does not exist."
        )
        return

    df = parse_benchmark_jsons(args.directory)

    if df is None or df.empty:
        print(
            f"Error: No valid fast-path benchmark data found "
            f"in '{args.directory}'."
        )
        return

    print("\nFound benchmark results:")

    for allocator, group in df.groupby("allocator"):
        sizes = sorted(group["size"].unique())

        print(
            f"  {allocator}: "
            f"{len(sizes)} allocation sizes "
            f"({', '.join(str(int(s)) for s in sizes)})"
        )

    plot_comparison(df, args.output)


if __name__ == "__main__":
    main()
