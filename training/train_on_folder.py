#!/usr/bin/env python3
import os
import sys
import random
import subprocess
import argparse

def main():
    parser = argparse.ArgumentParser(
        description="ZeroG Neural Network Folder Trainer. "
                    "Shuffles EPD files in a directory and trains on them in a single process to maintain Adam optimizer states."
    )
    parser.add_argument("-d", "--dir", default="games/Commented20to26", help="Directory containing EPD files")
    parser.add_argument("-o", "--output", default="nn_weights.bin", help="Output weights file")
    parser.add_argument("-w", "--weights", default="", help="Initial weights file to continue training from (optional)")
    parser.add_argument("-e", "--epochs", type=int, default=1, help="Number of training epochs per EPD file (default: 1)")
    parser.add_argument("-b", "--batch-size", type=int, default=512, help="Batch size (default: 512)")
    parser.add_argument("-t", "--threads", type=int, default=8, help="Number of worker threads (default: 8)")
    parser.add_argument("-l", "--lr", type=float, default=0.0002, help="Learning rate (default: 0.0002)")
    parser.add_argument("-v", "--val-split", type=float, default=0.1, help="Validation split (default: 0.1)")
    parser.add_argument("--val-file", default="", help="Static validation EPD file (optional)")
    parser.add_argument("--wd", type=float, default=1e-4, help="Weight decay (default: 1e-4)")

    args = parser.parse_args()

    if not os.path.isdir(args.dir):
        print(f"Error: Directory '{args.dir}' does not exist.")
        sys.exit(1)

    # 1. Collect all EPD files in the folder
    epd_files = [os.path.join(args.dir, f) for f in os.listdir(args.dir) if f.endswith(".epd")]
    if not epd_files:
        print(f"Error: No .epd files found in '{args.dir}'")
        sys.exit(1)

    print(f"Found {len(epd_files)} EPD files in '{args.dir}'.")

    # 2. Shuffle EPD files randomly to minimize sequential learning biases
    random.seed(42)
    random.shuffle(epd_files)
    print("Shuffled EPD files randomly.")

    # 3. Write file list to a temporary list file
    list_file_path = "temp_epd_list.txt"
    with open(list_file_path, "w") as f:
        for path in epd_files:
            f.write(path + "\n")
    print(f"Created file list at '{list_file_path}'.")

    # 4. Prepare command arguments for builds/nn_trainer
    cmd = [
        "./builds/nn_trainer",
        "-i", list_file_path,
        "-o", args.output,
        "-e", str(args.epochs),
        "-b", str(args.batch_size),
        "-t", str(args.threads),
        "-l", str(args.lr),
        "-v", str(args.val_split),
        "--wd", str(args.wd)
    ]
    if args.val_file:
        cmd.extend(["--val-file", args.val_file])
    if args.weights:
        cmd.extend(["-w", args.weights])

    print(f"Executing: {' '.join(cmd)}")
    sys.stdout.flush()

    # 5. Run the trainer
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error: Trainer exited with status {e.returncode}")
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        print("Training interrupted.")
    finally:
        # 6. Clean up temporary list file
        if os.path.exists(list_file_path):
            os.remove(list_file_path)
            print(f"Cleaned up '{list_file_path}'.")

if __name__ == "__main__":
    main()
