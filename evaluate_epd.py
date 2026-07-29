#!/usr/bin/env python3
import os
import sys
import argparse
import subprocess
import shutil
import re
import threading
import math
import time
import multiprocessing
import atexit

# Global variable inside each worker process
engine_process = None

def find_stockfish(custom_path=None):
    """
    Attempts to locate the Stockfish executable.
    Checks:
    1. The custom path if provided.
    2. The system PATH via shutil.which.
    3. Common macOS Homebrew installation locations.
    """
    if custom_path:
        if os.path.exists(custom_path) and os.access(custom_path, os.X_OK):
            return custom_path
        resolved = shutil.which(custom_path)
        if resolved:
            return resolved
        return None
    
    # Try PATH
    resolved = shutil.which("stockfish")
    if resolved:
        return resolved
    
    # Try typical Mac Homebrew paths
    mac_paths = [
        "/opt/homebrew/bin/stockfish",
        "/usr/local/bin/stockfish"
    ]
    for path in mac_paths:
        if os.path.exists(path) and os.access(path, os.X_OK):
            return path
            
    return None

def cleanup_worker():
    """Closes the Stockfish process cleanly on worker exit."""
    global engine_process
    if engine_process is not None:
        try:
            engine_process.stdin.write("quit\n")
            engine_process.stdin.flush()
            engine_process.terminate()
            engine_process.wait(timeout=1)
        except Exception:
            pass

def init_worker(stockfish_path):
    """Initializes the persistent Stockfish process in each worker."""
    global engine_process
    atexit.register(cleanup_worker)
    try:
        engine_process = subprocess.Popen(
            [stockfish_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1
        )
        # Initialize UCI
        engine_process.stdin.write("uci\n")
        engine_process.stdin.flush()
        while True:
            line = engine_process.stdout.readline()
            if not line:
                break
            if line.strip() == "uciok":
                break
    except Exception as e:
        engine_process = None

def process_position(task):
    """
    Worker task to evaluate a single position.
    Returns (idx, output_extra, error_message).
    """
    idx, fen, original_extra, target_depth, target_movetime, perspective, mate_to_cp = task
    global engine_process
    
    if engine_process is None or engine_process.poll() is not None:
        return idx, None, "Stockfish engine process is not running or failed to initialize.", None
    
    try:
        # Check readiness
        engine_process.stdin.write("isready\n")
        engine_process.stdin.flush()
        while True:
            line = engine_process.stdout.readline()
            if not line:
                return idx, None, "Engine process closed stdout while waiting for readyok.", None
            if line.strip() == "readyok":
                break
        
        # Send position FEN
        engine_process.stdin.write(f"position fen {fen}\n")
        
        # Send search command
        if target_movetime:
            engine_process.stdin.write(f"go movetime {target_movetime}\n")
        else:
            engine_process.stdin.write(f"go depth {target_depth}\n")
        engine_process.stdin.flush()
        
        score_type = None
        score_val = None
        info_depth = None
        
        while True:
            line = engine_process.stdout.readline()
            if not line:
                break
            line_str = line.strip()
            if line_str.startswith("info "):
                parts = line_str.split()
                # Extract depth
                if "depth" in parts:
                    try:
                        d_idx = parts.index("depth")
                        info_depth = int(parts[d_idx + 1])
                    except (ValueError, IndexError):
                        pass
                # Extract score
                if "score" in parts:
                    try:
                        s_idx = parts.index("score")
                        score_type = parts[s_idx + 1] # 'cp' or 'mate'
                        score_val = int(parts[s_idx + 2])
                    except (ValueError, IndexError):
                        pass
            elif line_str.startswith("bestmove"):
                break
        
        if score_type is None or score_val is None:
            return idx, None, "No score returned by Stockfish search.", None
        
        # Parse FEN to get active player
        fen_parts = fen.split()
        active_player = 'w'
        if len(fen_parts) > 1:
            active_player = fen_parts[1].lower()
            
        # White-perspective score for sigmoid (Stockfish returns side-to-move)
        score_white = -score_val if active_player == 'b' else score_val

        # If perspective is white and it is black's turn, invert the score
        if perspective == 'white' and active_player == 'b':
            score_val = -score_val
            
        # Check for extreme evaluations (abs(score) >= 1000 centipawns or mate)
        if score_type == "mate" or (score_type == "cp" and abs(score_val) >= 1000):
            return idx, None, "skip_extreme", None
            
        # Format evaluation score
        if score_type == "mate":
            if mate_to_cp:
                sign = 1 if score_val >= 0 else -1
                # Convert mate in X moves to high centipawn (e.g. mate in 1 -> 29999)
                cp_val = sign * (30000 - abs(score_val))
                score_str = f"score {cp_val};"
            else:
                score_str = f"score mate {score_val};"
        else:
            score_str = f"score {score_val};"
            
        # Strip old score and depth from original extra
        cleaned_extra = original_extra
        cleaned_extra = re.sub(r'\bscore\s+(?:mate\s+)?-?\d+;?', '', cleaned_extra)
        cleaned_extra = re.sub(r'\bdepth\s+\d+;?', '', cleaned_extra)
        cleaned_extra = re.sub(r'\s+', ' ', cleaned_extra).strip()
        cleaned_extra = re.sub(r';\s*;', ';', cleaned_extra)
        cleaned_extra = cleaned_extra.strip(';').strip()
        
        # Append new score and depth
        new_depth = info_depth or target_depth
        if cleaned_extra:
            output_extra = f"{cleaned_extra}; {score_str} depth {new_depth};"
        else:
            output_extra = f"{score_str} depth {new_depth};"
            
        return idx, output_extra, None, score_white
        
    except Exception as e:
        return idx, None, str(e), None

def extract_fen_and_extra(line):
    """
    Parses an EPD line and returns (fen, extra_opcodes).
    """
    line = line.strip()
    if not line:
        return None, None
    parts = line.split()
    if len(parts) < 4:
        return None, None
    
    # First 4 fields are always part of the FEN
    fen_parts = parts[:4]
    remaining = parts[4:]
    
    # 5th and 6th fields are optionally part of FEN (halfmove clock, fullmove number)
    if len(remaining) >= 2:
        if remaining[0].isdigit() and remaining[1].isdigit():
            fen_parts.extend(remaining[:2])
            remaining = remaining[2:]
        elif remaining[0].isdigit():
            fen_parts.append(remaining[0])
            remaining = remaining[1:]
            
    fen = " ".join(fen_parts)
    extra = " ".join(remaining)
    return fen, extra

def main():
    parser = argparse.ArgumentParser(description="Evaluate chess FEN positions in an EPD file using Stockfish.")
    parser.add_argument("-i", "--input", default="quiet_training_positions.epd", help="Input EPD file (default: quiet_training_positions.epd)")
    parser.add_argument("-o", "--output", default="quiet_training_positions_evaluated.epd", help="Output EPD file (default: quiet_training_positions_evaluated.epd)")
    parser.add_argument("-s", "--stockfish", help="Path to Stockfish executable (default: search in PATH or common macOS Homebrew paths)")
    parser.add_argument("-d", "--depth", type=int, default=10, help="Search depth for Stockfish (default: 10)")
    parser.add_argument("-t", "--movetime", type=int, help="Search time in milliseconds (optional, overrides depth)")
    parser.add_argument("-c", "--concurrency", type=int, default=multiprocessing.cpu_count(), help="Number of parallel Stockfish processes (default: number of CPU cores)")
    parser.add_argument("-p", "--perspective", choices=["side", "white"], default="side", help="Perspective of output scores: 'side' (side-to-move, default) or 'white' (always from White's perspective)")
    parser.add_argument("--mate-to-cp", action="store_true", help="Convert mate scores to centipawns (e.g. mate in 2 -> 29998)")
    
    args = parser.parse_args()
    
    # 1. Locate Stockfish
    stockfish_path = find_stockfish(args.stockfish)
    if not stockfish_path:
        print("Error: Could not locate Stockfish executable.", file=sys.stderr)
        print("Please specify the path to Stockfish using the -s/--stockfish argument,", file=sys.stderr)
        print("or install it (e.g. 'brew install stockfish').", file=sys.stderr)
        sys.exit(1)
        
    print(f"Using Stockfish: {stockfish_path}")
    print(f"Input file:      {args.input}")
    print(f"Output file:     {args.output}")
    if args.movetime:
        print(f"Search mode:     movetime = {args.movetime} ms")
    else:
        print(f"Search mode:     depth = {args.depth}")
    print(f"Concurrency:     {args.concurrency}")
    print(f"Perspective:     {args.perspective}")
    print(f"Mate-to-CP:      {args.mate_to_cp}")
    
    # 2. Read EPD lines
    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' does not exist.", file=sys.stderr)
        sys.exit(1)
        
    with open(args.input, "r") as f:
        lines = f.readlines()
        
    print(f"Total lines read: {len(lines)}")
    
    # Prepare tasks
    tasks = []
    skipped = 0
    for idx, line in enumerate(lines):
        line = line.strip()
        if not line:
            continue
        fen, extra = extract_fen_and_extra(line)
        if not fen:
            skipped += 1
            continue
        tasks.append((idx, fen, extra, args.depth, args.movetime, args.perspective, args.mate_to_cp))
        
    if skipped > 0:
        print(f"Skipped {skipped} malformed/empty lines.")
        
    print(f"Positions to evaluate: {len(tasks)}")
    if not tasks:
        print("No valid positions found. Exiting.")
        sys.exit(0)
        
    # Start timer
    start_time = time.time()
    
    # 3. Process concurrently
    # Pre-allocate output list to preserve original order
    results_ordered = [None] * len(lines)
    
    # Start pool
    pool = multiprocessing.Pool(
        processes=args.concurrency,
        initializer=init_worker,
        initargs=(stockfish_path,)
    )
    
    completed = 0
    errors = 0
    skipped_extreme = 0
    
    try:
        # imap yields results in the same order as input tasks
        # Chunksize is set to 1 to ensure work is distributed dynamically
        for idx, output_extra, err, sf_score_white in pool.imap(process_position, tasks, chunksize=1):
            completed += 1
            original_line = lines[idx].strip()
            fen, _ = extract_fen_and_extra(original_line)
            
            if err:
                if err == "skip_extreme":
                    skipped_extreme += 1
                    results_ordered[idx] = ""
                else:
                    errors += 1
                    # If error, write the original line with a comment
                    results_ordered[idx] = f"{original_line} # Error: {err}\n"
            else:
                # If input was pipe-delimited (tune_filter format), rewrite
                # the result column with sigmoid(Stockfish_score).
                if '|' in original_line and sf_score_white is not None:
                    K = 1.13
                    sigmoid_result = 1.0 / (1.0 + math.pow(10.0, -K * sf_score_white / 400.0))
                    # Extract the "score X; depth Y;" opcodes added by Stockfish eval
                    sd_match = re.search(r'score\s+-?\d+;\s*depth\s+\d+;', output_extra)
                    score_depth = sd_match.group(0) if sd_match else output_extra
                    results_ordered[idx] = f"{fen} | {sigmoid_result:.6f} | {sf_score_white}; {score_depth}\n"
                else:
                    results_ordered[idx] = f"{fen} {output_extra}\n"
                
            # Progress printout
            if completed % 100 == 0 or completed == len(tasks):
                elapsed = time.time() - start_time
                rate = completed / elapsed if elapsed > 0 else 0
                eta = (len(tasks) - completed) / rate if rate > 0 else 0
                eta_min = int(eta // 60)
                eta_sec = int(eta % 60)
                print(f"Progress: {completed}/{len(tasks)} ({completed/len(tasks)*100:.1f}%) | "
                      f"Speed: {rate:.1f} pos/s | "
                      f"ETA: {eta_min}m {eta_sec}s | "
                      f"Errors: {errors} | "
                      f"Skipped Extreme: {skipped_extreme}", end="\r")
        
        print() # Newline after progress loop
        
    except KeyboardInterrupt:
        print("\nEvaluation interrupted by user. Saving partial results...")
        pool.terminate()
        pool.join()
        # Save whatever we have so far
        with open(args.output, "w") as f:
            for line in results_ordered:
                if line is not None:
                    f.write(line)
        sys.exit(1)
    finally:
        pool.close()
        pool.join()
        
    # 4. Save results
    with open(args.output, "w") as f:
        for line in results_ordered:
            # Skip lines that were not processed (in case of exit/error)
            if line is not None:
                f.write(line)
                
    elapsed = time.time() - start_time
    print(f"Finished evaluation in {elapsed:.1f} seconds.")
    print(f"Results written to '{args.output}'.")
    if skipped_extreme > 0:
        print(f"Skipped {skipped_extreme} positions with extreme evaluations (abs(score) >= 1000 or mate).")
    if errors > 0:
        print(f"Warning: {errors} positions encountered errors during evaluation.")

if __name__ == "__main__":
    main()
