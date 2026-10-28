#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import matplotlib.pyplot as plt
import pandas as pd


def load_google_benchmark_json(json_path: Path) -> pd.DataFrame:
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    benchmarks = data.get("benchmarks", [])
    if not benchmarks:
        raise ValueError(f"No benchmark entries found in {json_path.name}")

    return pd.DataFrame(benchmarks)


def parse_benchmark_jsons(results_dir: Path) -> pd.DataFrame | None:
    data_frames = []

    for json_file in sorted(results_dir.glob("*.json")):
        allocator_label = json_file.stem  # 'Glox' or 'Mimalloc'

        try:
            df = load_google_benchmark_json(json_file)

            # Filter specifically for single-threaded fast-path latency benchmarks
            fastpath_df = df[
                df["name"].str.contains("SingleThreaded_FastPath")
            ].copy()

            if fastpath_df.empty:
                fastpath_df = df.copy()

            # Extract numeric block size (e.g., "BM_Glox_SingleThreaded_FastPath/64" -> 64)
            fastpath_df["size"] = (
                fastpath_df["name"].str.extract(r"/(\d+)").astype(float)
            )
            fastpath_df = fastpath_df.dropna(subset=["size"])
            fastpath_df["allocator"] = allocator_label

            data_frames.append(fastpath_df)
        except Exception as e:
            print(f"Warning: Failed to parse '{json_file.name}': {e}")

    if not data_frames:
        return None

    return pd.concat(data_frames, ignore_index=True)


def plot_comparison(df: pd.DataFrame, output_file: Path) -> None:
    plt.figure(figsize=(10, 6))

    for allocator, group in df.groupby("allocator"):
        group = group.sort_values("size")
        plt.plot(
            group["size"],
            group["real_time"],
            marker="o",
            linewidth=2,
            label=allocator,
        )

    plt.xscale("log", base=2)
    plt.xlabel("Allocation Size (Bytes)")
    plt.ylabel("Latency (ns / op)")
    plt.title("Fast-Path Allocation + Deallocation Latency")
    plt.grid(True, which="both", linestyle="--", alpha=0.5)
    plt.legend()
    plt.tight_layout()

    plt.savefig(output_file, dpi=300)
    print(f"[+] Successfully saved plot to: '{output_file}'")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Parse benchmark JSON directory and plot allocator performance."
    )
    parser.add_argument(
        "directory",
        type=Path,
        help="Directory containing JSON benchmark results",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("allocator_comparison.png"),
        help="Output path for the generated image plot",
    )

    args = parser.parse_args()

    if not args.directory.is_dir():
        print(f"Error: Directory '{args.directory}' does not exist.")
        return

    df = parse_benchmark_jsons(args.directory)
    if df is None or df.empty:
        print(
            f"Error: No valid benchmark JSON data found in '{args.directory}'."
        )
        return

    plot_comparison(df, args.output)


if __name__ == "__main__":
    main()
