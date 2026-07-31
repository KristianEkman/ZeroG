#!/usr/bin/env python3
"""
ZeroG End-to-End Single-File Training Pipeline.

Given a single PGN or EPD file path, this script automatically:
1. Verifies/builds required C binaries (builds/zerog and builds/nn_trainer).
2. Converts PGN to raw EPD (if input is .pgn).
3. Filters for quiet positions using ZeroG Quiescence Search (--tune-filter).
4. Trains the NNUE neural network and exports quantized weights.

Usage Examples:
  python3 train.py games/Commented20to26/2026-01.commented.[24987].pgn
  python3 train.py training/data/2026-01_raw.epd
  python3 train.py training/data/2026-01_quiet.epd --skip-filter
"""

import os
import sys
import argparse
import subprocess
import multiprocessing

def run_cmd(cmd, description):
    print(f"\n=======================================================")
    print(f"  {description}")
    print(f"  Command: {' '.join(cmd)}")
    print(f"=======================================================\n")
    res = subprocess.run(cmd)
    if res.returncode != 0:
        print(f"\n[Error] {description} failed with return code {res.returncode}.")
        sys.exit(res.returncode)

def main():
    parser = argparse.ArgumentParser(
        description="ZeroG Single-File NNUE Training Pipeline"
    )
    parser.add_argument("input_file", help="Path to input PGN or EPD file")
    parser.add_argument("-o", "--output", default="nn_weights.bin", help="Output network weights file (default: nn_weights.bin)")
    parser.add_argument("-w", "--weights", default="", help="Initial weights file to continue training from (optional)")
    parser.add_argument("-e", "--epochs", type=int, default=10, help="Number of training epochs (default: 10)")
    parser.add_argument("-b", "--batch-size", type=int, default=16384, help="Batch size (default: 16384)")
    parser.add_argument("-l", "--lr", type=float, default=0.001, help="Learning rate (default: 0.001)")
    parser.add_argument("--wd", type=float, default=1e-4, help="Weight decay coefficient (default: 0.0001)")
    parser.add_argument("-t", "--threads", type=int, default=multiprocessing.cpu_count(), help="Number of worker threads")
    parser.add_argument("--skip-filter", action="store_true", help="Skip quiescence filtering (use if input is already quiet EPD)")
    parser.add_argument("--scratch", action="store_true", help="Start training from scratch, ignoring existing output weights file")
    
    args = parser.parse_args()

    input_path = os.path.abspath(args.input_file)
    if not os.path.exists(input_path):
        print(f"[Error] Input file '{input_path}' not found.")
        sys.exit(1)

    root_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(root_dir)

    # 1. Build C binaries if missing
    zerog_bin = os.path.join("builds", "zerog")
    trainer_bin = os.path.join("builds", "nn_trainer")
    if not os.path.exists(zerog_bin) or not os.path.exists(trainer_bin):
        run_cmd(["make"], "Building C binaries with make")

    data_dir = os.path.join("training", "data")
    os.makedirs(data_dir, exist_ok=True)

    base_name = os.path.splitext(os.path.basename(input_path))[0]
    epd_target = input_path

    # 2. Convert PGN to raw EPD if needed
    if input_path.endswith(".pgn"):
        raw_epd = os.path.join(data_dir, f"{base_name}_raw.epd")
        convert_script = os.path.join("training", "convert_pgn_to_quiet_epd.py")
        run_cmd(["venv/bin/python3", convert_script, input_path, "-o", raw_epd], "Step 1: Extracting positions from PGN to raw EPD")
        epd_target = raw_epd

    # 3. Filter for quiet positions if not skipped
    quiet_epd = epd_target
    if not args.skip_filter:
        quiet_epd = os.path.join(data_dir, f"{base_name}_quiet.epd")
        run_cmd([zerog_bin, "--tune-filter", epd_target, quiet_epd], "Step 2: Filtering quiet positions with ZeroG Quiescence Search")

    # 4. Determine starting weights for continuous training
    weights_to_load = args.weights
    if not weights_to_load and not args.scratch:
        if os.path.exists(args.output):
            weights_to_load = args.output
            print(f"[Info] Existing weights file '{args.output}' found. Continuing training from existing weights.")

    # 5. Train NNUE model
    train_cmd = [
        trainer_bin,
        "-i", quiet_epd,
        "-o", args.output,
        "-e", str(args.epochs),
        "-b", str(args.batch_size),
        "-l", str(args.lr),
        "-d", str(args.wd),
        "-t", str(args.threads)
    ]
    if weights_to_load:
        train_cmd.extend(["-w", weights_to_load])

    run_cmd(train_cmd, "Step 3: Training NNUE Neural Network")

    print("\n=======================================================")
    print(f"  Training Pipeline Complete!")
    print(f"  Model saved to: {os.path.abspath(args.output)}")
    print(f"=======================================================\n")

if __name__ == "__main__":
    main()
