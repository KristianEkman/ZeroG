#!/usr/bin/env python3
import os
import sys
import argparse
import subprocess
import shutil
import re
import time
import threading

class StatusState:
    def __init__(self, total_games):
        self.total_games = total_games
        self.game_num = 0
        self.start_time = time.time()
        self.running = True
        self.lock = threading.Lock()

def find_cutechess(custom_path=None):
    """
    Attempts to locate the cutechess-cli executable.
    Checks:
    1. The custom path if provided.
    2. The default build path relative to this script directory:
       ../cutechess/cutechess/build/cutechess-cli
    3. The system PATH via shutil.which.
    """
    if custom_path:
        if os.path.exists(custom_path) and os.access(custom_path, os.X_OK):
            return custom_path
        resolved = shutil.which(custom_path)
        if resolved:
            return resolved
        return None

    # Check relative path from script dir (script dir is workspace root)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    rel_path = os.path.abspath(os.path.join(script_dir, "..", "cutechess", "cutechess", "build", "cutechess-cli"))
    if os.path.exists(rel_path) and os.access(rel_path, os.X_OK):
        return rel_path

    # Try PATH
    resolved = shutil.which("cutechess-cli")
    if resolved:
        return resolved

    return None

def format_time(seconds):
    """Formats seconds into HH:MM:SS or MM:SS."""
    h = int(seconds // 3600)
    m = int((seconds % 3600) // 60)
    s = int(seconds % 60)
    if h > 0:
        return f"{h:02d}:{m:02d}:{s:02d}"
    return f"{m:02d}:{s:02d}"

def print_status_line(game_num, total_games, elapsed, eta):
    """Prints a right-aligned status line at the bottom of the terminal."""
    if not sys.stdout.isatty():
        return
    
    cols, _ = shutil.get_terminal_size()
    pct = (game_num / total_games) * 100
    left_part = f"Progress: {game_num}/{total_games} ({pct:.1f}%) | Elapsed: {format_time(elapsed)}"
    
    if game_num == 0:
        right_part = "Left: --:--"
    else:
        right_part = f"Left: {format_time(eta)}"
        
    # Pad so the right part is aligned to the right side of the screen
    padding = cols - len(left_part) - len(right_part) - 1
    if padding > 0:
        status_line = f"{left_part}{' ' * padding}{right_part}"
    else:
        status_line = f"{left_part} | {right_part}"
        
    sys.stdout.write(f"\r\033[K{status_line}")
    sys.stdout.flush()

def status_updater_thread(state):
    """Background thread that updates elapsed time and ETA in real-time."""
    while True:
        time.sleep(0.5)
        with state.lock:
            if not state.running:
                break
            
            elapsed = time.time() - state.start_time
            if state.game_num == 0:
                eta = 0
            else:
                rate = state.game_num / elapsed if elapsed > 0 else 0
                remaining = state.total_games - state.game_num
                eta = remaining / rate if rate > 0 else 0
                
            print_status_line(state.game_num, state.total_games, elapsed, eta)

def main():
    parser = argparse.ArgumentParser(
        description="Run selfplay matches with progress monitoring and time left estimation.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("--pgnout", help="Path to write the PGN output file.")
    parser.add_argument("--savefen", help="Path to save quiet training positions in EPD format.")
    parser.add_argument("-games", "--games", type=int, default=300, help="Total number of games to play.")
    parser.add_argument("-concurrency", "--concurrency", type=int, default=4, help="Number of concurrent games.")
    parser.add_argument("-tc", "--tc", default="5+0.01", help="Time control for each engine.")
    parser.add_argument("-threads", "--threads", type=int, default=2, help="Number of search threads per engine.")
    parser.add_argument("--cutechess", help="Custom path to the cutechess-cli executable.")

    # Parse known arguments, capture anything else to forward to cutechess-cli
    args, unknown_args = parser.parse_known_args()

    # 1. Locate cutechess-cli
    cutechess_path = find_cutechess(args.cutechess)
    if not cutechess_path:
        print("Error: Could not locate cutechess-cli executable.", file=sys.stderr)
        print("Please compile cutechess in '../cutechess/' or ensure it is on your PATH.", file=sys.stderr)
        sys.exit(1)

    # 2. Check engine executables and add default engines if no custom engines are specified
    has_custom_engines = any(arg == "-engine" for arg in sys.argv)
    
    cmd = [cutechess_path]

    if not has_custom_engines:
        engines = [
            ("./zerog_prev", "OLD"),
            ("./builds/zerog", "NEW")
        ]
        resolved_engines = []
        for path, name in engines:
            resolved_path = path
            if not (os.path.exists(resolved_path) and os.access(resolved_path, os.X_OK)):
                if name == "OLD" and os.path.exists("./zerog_prev"):
                    resolved_path = "./zerog_prev"
                elif name == "NEW" and os.path.exists("./builds/zerog"):
                    resolved_path = "./builds/zerog"
                else:
                    print(f"Error: Engine executable for {name} not found or not executable at '{path}'.", file=sys.stderr)
                    print("Please build your engines first (e.g. 'make').", file=sys.stderr)
                    sys.exit(1)
            resolved_engines.append(resolved_path)

        old_engine_args = ["cmd=" + resolved_engines[0], "proto=uci", "name=OLD"]
        if args.threads > 1:
            old_engine_args.append(f"option.Threads={args.threads}")
        cmd.extend(["-engine"] + old_engine_args)

        new_engine_args = ["cmd=" + resolved_engines[1], "proto=uci", "name=NEW"]
        if args.savefen:
            abs_savefen = os.path.abspath(args.savefen)
            new_engine_args.append(f"option.SaveQuietPositionsFile={abs_savefen}")
        if args.threads > 1:
            new_engine_args.append(f"option.Threads={args.threads}")
        
        cmd.extend(["-engine"] + new_engine_args)

    # 3. Construct cutechess-cli tournament controls
    has_custom_each = any(arg == "-each" for arg in sys.argv)
    if not has_custom_each:
        cmd.extend(["-each", f"tc={args.tc}"])

    cmd.extend([
        "-games", str(args.games),
    ])

    has_custom_openings = any(arg == "-openings" for arg in sys.argv)
    if not has_custom_openings:
        cmd.extend(["-openings", "file=games/top_engine_games.pgn", "format=pgn", "plies=16", "order=random"])

    has_custom_repeat = any(arg == "-repeat" for arg in sys.argv)
    if not has_custom_repeat:
        cmd.extend(["-repeat"])

    cmd.extend([
        "-concurrency", str(args.concurrency),
    ])

    if args.pgnout:
        cmd.extend(["-pgnout", args.pgnout, "fi"])

    if unknown_args:
        cmd.extend(unknown_args)

    print(f"Starting selfplay match...")
    print(f"Command: {' '.join(cmd)}")
    print(f"Total Games: {args.games} | Concurrency: {args.concurrency} | TC: {args.tc}")
    if args.savefen:
        print(f"Quiet FENs will be saved to: {os.path.abspath(args.savefen)}")
    if args.pgnout:
        print(f"PGN games will be saved to: {os.path.abspath(args.pgnout)}")
    print("-" * 80)

    # 4. Run tournament and parse output
    start_time = time.time()
    game_pattern = re.compile(r"^Finished game (\d+) \(([^)]+)\): (.*)$")
    
    state = StatusState(args.games)
    
    # Start background updater thread if running interactively
    updater = None
    if sys.stdout.isatty():
        updater = threading.Thread(target=status_updater_thread, args=(state,), daemon=True)
        updater.start()
        
    try:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

        for line in iter(process.stdout.readline, ""):
            line_str = line.strip()
            match = game_pattern.match(line_str)
            
            with state.lock:
                if match:
                    state.game_num = int(match.group(1))
                    
                # Erase status line before printing new info
                if sys.stdout.isatty():
                    sys.stdout.write("\r\033[K")
                
                if match and not sys.stdout.isatty():
                    # For non-TTY output (redirected to file), append stats inline
                    elapsed = time.time() - start_time
                    rate = state.game_num / elapsed if elapsed > 0 else 0
                    remaining = args.games - state.game_num
                    eta = remaining / rate if rate > 0 else 0
                    pct = (state.game_num / args.games) * 100
                    progress_info = (
                        f" | Progress: {state.game_num}/{args.games} ({pct:.2f}%) "
                        f"| Speed: {rate:.2f} games/s "
                        f"| Elapsed: {format_time(elapsed)} "
                        f"| Time Left: {format_time(eta)}"
                    )
                    print(f"{line_str}{progress_info}", flush=True)
                else:
                    # Print normal line
                    print(line_str, flush=True)
                
                # Immediately redraw status line in TTY mode
                if sys.stdout.isatty():
                    elapsed = time.time() - start_time
                    if state.game_num == 0:
                        eta = 0
                    else:
                        rate = state.game_num / elapsed if elapsed > 0 else 0
                        remaining = args.games - state.game_num
                        eta = remaining / rate if rate > 0 else 0
                    print_status_line(state.game_num, args.games, elapsed, eta)

        process.wait()
        
        # Stop background updater
        with state.lock:
            state.running = False
            if sys.stdout.isatty():
                sys.stdout.write("\r\033[K")
                sys.stdout.flush()
                
        elapsed_total = time.time() - start_time
        print("-" * 80)
        print(f"Selfplay tournament finished in {format_time(elapsed_total)}.")
        sys.exit(process.returncode)

    except KeyboardInterrupt:
        with state.lock:
            state.running = False
            if sys.stdout.isatty():
                sys.stdout.write("\r\033[K")
                sys.stdout.flush()
        print("\n\nSelfplay tournament interrupted by user. Terminating process...")
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
        sys.exit(1)

if __name__ == "__main__":
    main()
