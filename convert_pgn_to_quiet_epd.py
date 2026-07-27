#!/usr/bin/env python3
"""
Convert commented PGN files to quiet ZeroG EPD training data.
Extracts positions from PGN files, filters them by depth/Elo/etc.,
and then runs them through the compiled ZeroG engine's quiescence-search
filter (--tune-filter) to export quiet positions.
"""

import sys
import os
import argparse
import subprocess
import glob
import tempfile
import re
import math
import time

try:
    import chess.pgn
except ImportError:
    print("Error: python-chess package is required. Run 'pip install python-chess' or use python in venv.", file=sys.stderr)
    sys.exit(1)

# Regex to match score and depth from comments like "+0.25/34 37.89s", "-0.13/30 18.06s", "+M5/20 1.00s"
COMMENT_REGEX = re.compile(r'([-+]?M?\d+(?:\.\d+)?)/(\d+)')
TEXEL_K = 1.13

def parse_elo(val):
    if not val:
        return 0
    m = re.search(r'\d+', str(val))
    return int(m.group(0)) if m else 0

def convert_single_pgn(input_pgn, output_epd, max_cp=1000, min_depth=22, min_elo=3100, elo_mode='active', deduplicate=False, max_games=None, file_idx=1, total_files=1, global_seen_fens=None):
    if not os.path.exists(input_pgn):
        print(f"[{file_idx}/{total_files}] Error: Input PGN file '{input_pgn}' not found.", file=sys.stderr)
        return None

    out_dir = os.path.dirname(output_epd)
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir, exist_ok=True)

    print(f"[{file_idx}/{total_files}] Extracting PGN '{input_pgn}' -> '{output_epd}'...")

    total_games = 0
    exported_positions = 0
    skipped_book = 0
    skipped_no_eval = 0
    skipped_extreme = 0
    skipped_low_depth = 0
    skipped_low_elo = 0
    skipped_duplicates = 0

    local_seen = set() if (deduplicate and global_seen_fens is None) else None
    seen_fens = global_seen_fens if global_seen_fens is not None else local_seen

    start_time = time.time()
    last_log_time = start_time

    try:
        with open(input_pgn, 'r', encoding='utf-8', errors='ignore') as infile, \
             open(output_epd, 'w', encoding='utf-8') as outfile:

            while True:
                if max_games is not None and total_games >= max_games:
                    break

                game = chess.pgn.read_game(infile)
                if not game:
                    break

                total_games += 1
                board = game.board()

                white_elo = parse_elo(game.headers.get("WhiteElo", ""))
                black_elo = parse_elo(game.headers.get("BlackElo", ""))

                for node in game.mainline():
                    comment = node.comment.strip() if node.comment else ""

                    if not comment or comment == 'book':
                        skipped_book += 1
                        board.push(node.move)
                        continue

                    m = COMMENT_REGEX.search(comment)
                    if not m:
                        skipped_no_eval += 1
                        board.push(node.move)
                        continue

                    score_str, depth_str = m.group(1), m.group(2)

                    # Skip mate scores
                    if 'M' in score_str:
                        skipped_extreme += 1
                        board.push(node.move)
                        continue

                    try:
                        score_pawn = float(score_str)
                        score_cp = int(round(score_pawn * 100))
                        depth = int(depth_str)
                    except ValueError:
                        skipped_no_eval += 1
                        board.push(node.move)
                        continue

                    # Filter by minimum depth
                    if min_depth > 0 and depth < min_depth:
                        skipped_low_depth += 1
                        board.push(node.move)
                        continue

                    # Skip extreme scores (|score| >= max_cp)
                    if abs(score_cp) >= max_cp:
                        skipped_extreme += 1
                        board.push(node.move)
                        continue

                    fen = board.fen()
                    active_player = fen.split()[1]

                    # Filter by minimum Elo
                    if min_elo > 0:
                        if elo_mode == 'both':
                            if (white_elo > 0 and white_elo < min_elo) or (black_elo > 0 and black_elo < min_elo):
                                skipped_low_elo += 1
                                board.push(node.move)
                                continue
                        elif elo_mode == 'either':
                            if max(white_elo, black_elo) > 0 and max(white_elo, black_elo) < min_elo:
                                skipped_low_elo += 1
                                board.push(node.move)
                                continue
                        else: # 'active' player
                            act_elo = white_elo if active_player == 'w' else black_elo
                            if act_elo > 0 and act_elo < min_elo:
                                skipped_low_elo += 1
                                board.push(node.move)
                                continue

                    if deduplicate and seen_fens is not None:
                        if fen in seen_fens:
                            skipped_duplicates += 1
                            board.push(node.move)
                            continue
                        seen_fens.add(fen)

                    score_white = score_cp if active_player == 'w' else -score_cp
                    simulated_result = 1.0 / (1.0 + math.pow(10.0, -TEXEL_K * score_white / 400.0))

                    # Standard ZeroG EPD format expected by --tune-filter:
                    # <FEN> | <simulated_result> | <white_score>; score <score_val>; depth <depth_val>;
                    epd_line = f"{fen} | {simulated_result:.6f} | {score_white}; score {score_cp}; depth {depth};\n"
                    outfile.write(epd_line)

                    exported_positions += 1
                    board.push(node.move)

                curr_time = time.time()
                if total_games % 1000 == 0 or (curr_time - last_log_time) >= 5.0:
                    elapsed = curr_time - start_time
                    print(f"   [{file_idx}/{total_files}] Processed {total_games:,} games ({exported_positions:,} positions exported) in {elapsed:.1f}s...")
                    last_log_time = curr_time

    except Exception as e:
        print(f"[{file_idx}/{total_files}] Error extracting PGN '{input_pgn}': {e}", file=sys.stderr)
        return None

    elapsed = time.time() - start_time
    print(f"   [{file_idx}/{total_files}] Finished '{os.path.basename(input_pgn)}': {total_games:,} games, {exported_positions:,} raw positions extracted in {elapsed:.1f}s")

    return {
        'total_games': total_games,
        'exported_positions': exported_positions,
        'skipped_book': skipped_book,
        'skipped_no_eval': skipped_no_eval,
        'skipped_extreme': skipped_extreme,
        'skipped_low_depth': skipped_low_depth,
        'skipped_low_elo': skipped_low_elo,
        'skipped_duplicates': skipped_duplicates,
        'elapsed': elapsed,
        'output_epd': output_epd
    }

def main():
    parser = argparse.ArgumentParser(
        description="Convert commented PGN files to quiet ZeroG EPD format using the engine's quiescence filter.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  python3 convert_pgn_to_quiet_epd.py input.pgn output.epd
  python3 convert_pgn_to_quiet_epd.py input1.pgn input2.pgn -o /path/to/outdir
  python3 convert_pgn_to_quiet_epd.py games/Commented20to26/*.pgn --min-depth 22 --min-elo 3100
"""
    )
    parser.add_argument("input_pgns", nargs="+", help="Path to input commented PGN file(s) or glob pattern(s)")
    parser.add_argument("-o", "--output", help="Output quiet EPD file (for single input) or output directory")
    parser.add_argument("--engine", default="./builds/zerog", help="Path to the compiled zerog binary (default: ./builds/zerog)")
    parser.add_argument("--max-cp", type=int, default=1000, help="Maximum centipawn score magnitude to include (default: 1000)")
    parser.add_argument("--min-depth", type=int, default=22, help="Minimum search depth required to export position (default: 22)")
    parser.add_argument("--min-elo", type=int, default=3100, help="Minimum engine Elo required to export position (default: 3100)")
    parser.add_argument("--elo-mode", choices=['active', 'both', 'either'], default='active', help="Elo filter mode: 'active' engine, 'both' engines, or 'either' engine (default: active)")
    parser.add_argument("--dedup", action="store_true", help="Deduplicate positions by FEN")
    parser.add_argument("--global-dedup", action="store_true", help="Deduplicate positions across ALL input files")
    parser.add_argument("--max-games", type=int, default=None, help="Maximum number of games to process per PGN file (useful for testing)")

    args = parser.parse_args()

    # Locate and validate the compiled zerog engine binary
    engine_path = args.engine
    if not os.path.exists(engine_path):
        print(f"Error: ZeroG engine binary not found at '{engine_path}'.", file=sys.stderr)
        print("Please compile the engine first by running 'make' or 'make BUILD=release'.", file=sys.stderr)
        sys.exit(1)
    if not os.access(engine_path, os.X_OK):
        print(f"Error: ZeroG engine binary at '{engine_path}' is not executable.", file=sys.stderr)
        sys.exit(1)

    # Resolve glob wildcards
    resolved_inputs = []
    for item in args.input_pgns:
        matched = glob.glob(item)
        if matched:
            resolved_inputs.extend(sorted(matched))
        else:
            resolved_inputs.append(item)

    if not resolved_inputs:
        print("Error: No input PGN files found.", file=sys.stderr)
        sys.exit(1)

    # Legacy 2-argument handling check: input_pgns = ['in.pgn', 'out.epd']
    output_epd = args.output
    if len(resolved_inputs) == 2 and output_epd is None:
        first, second = resolved_inputs[0], resolved_inputs[1]
        if second.endswith('.epd') or not os.path.exists(second):
            resolved_inputs = [first]
            output_epd = second

    total_files = len(resolved_inputs)
    print(f"Starting quiet conversion for {total_files} PGN file(s)...")
    print(f"Filters: min-depth={args.min_depth}, min-elo={args.min_elo} (mode: {args.elo_mode}), max-cp={args.max_cp}")
    if args.dedup:
        print("Deduplication enabled.")

    global_seen_fens = set() if (args.dedup and (args.global_dedup or total_files > 1)) else None

    successful_files = 0
    total_positions_extracted = 0

    for idx, input_pgn in enumerate(resolved_inputs, start=1):
        if output_epd:
            if os.path.isdir(output_epd) or total_files > 1:
                base_name = os.path.splitext(os.path.basename(input_pgn))[0]
                target_epd = os.path.join(output_epd, base_name + ".epd")
            else:
                target_epd = output_epd
        else:
            target_epd = os.path.splitext(input_pgn)[0] + ".epd"

        # Create temporary file for raw EPD positions
        temp_fd, temp_path = tempfile.mkstemp(suffix='.epd')
        os.close(temp_fd)

        try:
            # 1. Extract raw EPD positions to temp file
            res = convert_single_pgn(
                input_pgn=input_pgn,
                output_epd=temp_path,
                max_cp=args.max_cp,
                min_depth=args.min_depth,
                min_elo=args.min_elo,
                elo_mode=args.elo_mode,
                deduplicate=args.dedup,
                max_games=args.max_games,
                file_idx=idx,
                total_files=total_files,
                global_seen_fens=global_seen_fens
            )

            if not res or res['exported_positions'] == 0:
                print(f"[{idx}/{total_files}] No positions extracted from '{os.path.basename(input_pgn)}' to filter.")
                continue

            total_positions_extracted += res['exported_positions']

            # Make sure target directory exists
            target_dir = os.path.dirname(target_epd)
            if target_dir and not os.path.exists(target_dir):
                os.makedirs(target_dir, exist_ok=True)

            # 2. Run the engine's --tune-filter to filter quiet positions
            print(f"[{idx}/{total_files}] Filtering quiet positions via engine: {temp_path} -> {target_epd}")
            subprocess.run([engine_path, "--tune-filter", temp_path, target_epd], check=True)
            successful_files += 1

        except subprocess.CalledProcessError as e:
            print(f"[{idx}/{total_files}] Error running engine filter: {e}", file=sys.stderr)
        except Exception as e:
            print(f"[{idx}/{total_files}] Error processing file: {e}", file=sys.stderr)
        finally:
            if os.path.exists(temp_path):
                os.unlink(temp_path)

    print("\n" + "=" * 60)
    print(f"Batch Quiet Conversion Complete ({successful_files}/{total_files} files filtered successfully)")
    print(f"Total raw positions extracted from PGN: {total_positions_extracted:,}")
    print("=" * 60)

if __name__ == '__main__':
    main()
