#!/usr/bin/env python3
"""
Position classifier for ZeroG Texel tuning.

Implements the "surprising AND stable" criterion for position selection:
- Classifies positions into representative, hard-but-stable, and noisy
- Applies evaluation-range rebalancing across score buckets
- Assigns per-position weights for the optimizer
- Reports dataset quality statistics

Input:  Pipe-delimited EPD from tune_filter (FEN | result | score | qscore Q)
Output: Pipe-delimited EPD with weight annotation for tune_export
"""
import argparse
import math
import os
import random
import sys
from collections import defaultdict


# ============================================================
# Score bucket definitions (centipawns, absolute value)
# ============================================================
SCORE_BUCKETS = [
    (0, 25,   "0-25cp"),
    (25, 75,  "25-75cp"),
    (75, 150, "75-150cp"),
    (150, 300, "150-300cp"),
    (300, 600, "300-600cp"),
    (600, 10000, "600+cp"),
]


def parse_line(line):
    """Parse a tune_filter output line.

    Format: FEN | result | score_white | qscore Q
    Returns: (fen, result, score_white, qscore) or None if malformed.
    """
    parts = line.split('|')
    if len(parts) < 3:
        return None

    fen = parts[0].strip()
    try:
        result = float(parts[1].strip())
    except ValueError:
        return None

    try:
        score_white = int(parts[2].strip().rstrip(';').strip())
    except ValueError:
        return None

    # Parse qscore from the extra field (optional)
    qscore = None
    if len(parts) >= 4:
        extra = parts[3].strip()
        if extra.startswith('qscore'):
            try:
                qscore = int(extra.split()[1])
            except (IndexError, ValueError):
                pass

    return fen, result, score_white, qscore


def get_score_bucket(score_white):
    """Return the bucket index for a given score."""
    abs_score = abs(score_white)
    for i, (lo, hi, _) in enumerate(SCORE_BUCKETS):
        if lo <= abs_score < hi:
            return i
    return len(SCORE_BUCKETS) - 1


def classify_position(score_white, qscore, hard_threshold=100, stable_threshold=40):
    """Classify a position based on the score/qscore discrepancy.

    Categories:
    - 'representative': |score - qscore| < hard_threshold  (normal positions)
    - 'hard':           |score - qscore| >= hard_threshold  (hard-example mining)
    - 'unstable':       would need verification score — currently not available,
                        so all non-representative are treated as 'hard'.

    When verification scores become available (Phase 2), this will also filter
    out unstable positions where |verification - deep| > stable_threshold.
    """
    if qscore is None:
        return 'representative'

    delta_hard = abs(score_white - qscore)
    if delta_hard >= hard_threshold:
        return 'hard'
    return 'representative'


def main():
    parser = argparse.ArgumentParser(
        description="Classify and weight positions for Texel tuning",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("-i", "--input", required=True,
                        help="Input EPD file (from tune_filter)")
    parser.add_argument("-o", "--output", required=True,
                        help="Output EPD file (weighted, for tune_export)")
    parser.add_argument("--hard-threshold", type=int, default=100,
                        help="Centipawn threshold for 'hard' classification")
    parser.add_argument("--representative-pct", type=float, default=0.70,
                        help="Target percentage of representative positions")
    parser.add_argument("--hard-pct", type=float, default=0.20,
                        help="Target percentage of hard-but-stable positions")
    parser.add_argument("--rare-pct", type=float, default=0.10,
                        help="Target percentage of rare/underrepresented positions")
    parser.add_argument("--max-bucket-ratio", type=float, default=3.0,
                        help="Max oversized-bucket / target-bucket ratio before capping")
    parser.add_argument("--hard-weight", type=float, default=1.5,
                        help="Weight multiplier for hard positions")
    parser.add_argument("--max-weight", type=float, default=3.0,
                        help="Maximum per-position weight")
    parser.add_argument("--seed", type=int, default=42,
                        help="Random seed for reproducible sampling")
    parser.add_argument("--no-rebalance", action="store_true",
                        help="Skip score-range rebalancing")
    args = parser.parse_args()

    random.seed(args.seed)

    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' not found.", file=sys.stderr)
        sys.exit(1)

    # ============================================================
    # Pass 1: Read and classify all positions
    # ============================================================
    print(f"Reading positions from {args.input}...")
    positions = []  # list of (fen, result, score_white, qscore, class, bucket)
    class_counts = defaultdict(int)
    bucket_counts = defaultdict(int)
    parse_errors = 0

    with open(args.input, 'r') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue

            parsed = parse_line(line)
            if parsed is None:
                parse_errors += 1
                continue

            fen, result, score_white, qscore = parsed
            pos_class = classify_position(score_white, qscore,
                                          hard_threshold=args.hard_threshold)
            bucket = get_score_bucket(score_white)
            positions.append((fen, result, score_white, qscore, pos_class, bucket))
            class_counts[pos_class] += 1
            bucket_counts[bucket] += 1

    total = len(positions)
    if total == 0:
        print("Error: No valid positions found.", file=sys.stderr)
        sys.exit(1)

    print(f"\n  Total positions: {total:,}")
    if parse_errors > 0:
        print(f"  Parse errors:    {parse_errors:,}")
    print(f"\n  Class distribution:")
    for cls in ['representative', 'hard', 'unstable']:
        cnt = class_counts.get(cls, 0)
        pct = cnt / total * 100
        print(f"    {cls:20s}: {cnt:>10,} ({pct:5.1f}%)")

    print(f"\n  Score bucket distribution:")
    for i, (_, _, label) in enumerate(SCORE_BUCKETS):
        cnt = bucket_counts.get(i, 0)
        pct = cnt / total * 100
        print(f"    {label:20s}: {cnt:>10,} ({pct:5.1f}%)")

    # ============================================================
    # Pass 2: Compute weights
    # ============================================================
    print("\nComputing position weights...")

    # 2a. Class-based weighting
    # Hard positions get a higher base weight to boost their gradient contribution
    weights = []
    for _, _, score_white, qscore, pos_class, bucket in positions:
        if pos_class == 'hard':
            w = args.hard_weight
        else:
            w = 1.0
        weights.append(w)

    # 2b. Score-range rebalancing
    # Cap oversized buckets to prevent them from dominating
    if not args.no_rebalance:
        ideal_per_bucket = total / len(SCORE_BUCKETS)

        for i, (_, _, label) in enumerate(SCORE_BUCKETS):
            cnt = bucket_counts.get(i, 0)
            if cnt == 0:
                continue
            ratio = cnt / ideal_per_bucket
            if ratio > args.max_bucket_ratio:
                # Scale down weights for positions in this oversized bucket
                scale = args.max_bucket_ratio / ratio
                for j, (_, _, _, _, _, b) in enumerate(positions):
                    if b == i:
                        weights[j] *= scale

    # 2c. Clamp weights
    for j in range(len(weights)):
        weights[j] = min(weights[j], args.max_weight)
        weights[j] = max(weights[j], 0.1)  # Don't zero out anything completely

    # ============================================================
    # Pass 3: Write output
    # ============================================================
    print(f"\nWriting classified positions to {args.output}...")

    total_weight = sum(weights)
    class_weight_sums = defaultdict(float)

    with open(args.output, 'w') as f:
        for j, (fen, result, score_white, qscore, pos_class, bucket) in enumerate(positions):
            w = weights[j]
            class_weight_sums[pos_class] += w

            # Output format matches tune_export input:
            # FEN | result | score_white | weight W
            f.write(f"{fen} | {result:.6f} | {score_white} | weight {w:.4f}\n")

    # ============================================================
    # Report
    # ============================================================
    print(f"\n  Output statistics:")
    print(f"    Total positions: {total:,}")
    print(f"    Total weight:    {total_weight:,.1f}")
    print(f"    Effective N:     {total_weight:,.0f} (vs raw {total:,})")
    print(f"\n  Weighted class distribution:")
    for cls in ['representative', 'hard']:
        w = class_weight_sums.get(cls, 0)
        pct = w / total_weight * 100 if total_weight > 0 else 0
        print(f"    {cls:20s}: weight {w:>10,.1f} ({pct:5.1f}%)")

    # Weight histogram
    weight_hist = defaultdict(int)
    for w in weights:
        bucket_w = round(w, 1)
        weight_hist[bucket_w] += 1
    print(f"\n  Weight histogram:")
    for w_val in sorted(weight_hist.keys()):
        cnt = weight_hist[w_val]
        bar = '#' * min(50, int(cnt / total * 200))
        print(f"    w={w_val:4.1f}: {cnt:>8,} {bar}")

    print(f"\nDone! Output: {args.output}")


if __name__ == "__main__":
    main()
