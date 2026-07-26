#!/usr/bin/env python3
"""
Convert commented PGN files (with {score/depth time} move comments) 
to ZeroG EPD training data files.
Supports single or multiple input PGN files and wildcard patterns.
"""

import sys
import os
import argparse
import re
import math
import time
import glob

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

def convert_single_pgn(input_pgn, output_epd, max_cp=1000, min_depth=22, min_elo=3100, elo_mode='active', deduplicate=False, file_idx=1, total_files=1, global_seen_fens=None):
    if not os.path.exists(input_pgn):
        print(f"[{file_idx}/{total_files}] Error: Input PGN file '{input_pgn}' not found.", file=sys.stderr)
        return None

    out_dir = os.path.dirname(output_epd)
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir, exist_ok=True)

    print(f"[{file_idx}/{total_files}] Converting PGN '{input_pgn}' -> '{output_epd}'...")

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

                    # Standard ZeroG EPD Format:
                    # <FEN> | <simulated_result> | <white_score>; score <score_val>; depth <depth_val>;
                    epd_line = f"{fen} | {simulated_result:.6f} | {score_white}; score {score_cp}; depth {depth};\n"
                    outfile.write(epd_line)

                    exported_positions += 1
                    board.push(node.move)

                curr_time = time.time()
                if total_games % 1000 == 0 or (curr_time - last_log_time) >= 5.0:
                    elapsed = curr_time - start_time
                    print(f"   [{file_idx}/{total_files}] Processed {total_games:,} games ({exported_positions:,} total EPD positions exported so far) in {elapsed:.1f}s...")
                    last_log_time = curr_time

    except Exception as e:
        print(f"[{file_idx}/{total_files}] Error processing '{input_pgn}': {e}", file=sys.stderr)
        return None

    elapsed = time.time() - start_time
    print(f"   [{file_idx}/{total_files}] Finished '{os.path.basename(input_pgn)}': {total_games:,} games, {exported_positions:,} EPD positions exported in {elapsed:.1f}s")

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

def convert_pgn_to_epd(input_pgns, output_epd=None, max_cp=1000, min_depth=22, min_elo=3100, elo_mode='active', deduplicate=False, global_dedup=False):
    if isinstance(input_pgns, str):
        input_pgns = [input_pgns]

    # Resolve glob wildcards
    resolved_inputs = []
    for item in input_pgns:
        matched = glob.glob(item)
        if matched:
            resolved_inputs.extend(sorted(matched))
        else:
            resolved_inputs.append(item)

    if not resolved_inputs:
        print("Error: No input PGN files found.", file=sys.stderr)
        sys.exit(1)

    # Legacy 2-argument handling check: input_pgns = ['in.pgn', 'out.epd']
    if len(resolved_inputs) == 2 and output_epd is None:
        first, second = resolved_inputs[0], resolved_inputs[1]
        if second.endswith('.epd') or not os.path.exists(second):
            resolved_inputs = [first]
            output_epd = second

    total_files = len(resolved_inputs)
    print(f"Starting conversion for {total_files} PGN file(s)...")
    print(f"Filters: min-depth={min_depth}, min-elo={min_elo} (mode: {elo_mode}), max-cp={max_cp}")
    if deduplicate:
        print("Deduplication enabled.")

    global_seen_fens = set() if (deduplicate and (global_dedup or total_files > 1)) else None

    total_stats = {
        'total_files': total_files,
        'successful_files': 0,
        'total_games': 0,
        'exported_positions': 0,
        'skipped_book': 0,
        'skipped_no_eval': 0,
        'skipped_extreme': 0,
        'skipped_low_depth': 0,
        'skipped_low_elo': 0,
        'skipped_duplicates': 0,
        'start_time': time.time()
    }

    for idx, input_pgn in enumerate(resolved_inputs, start=1):
        if output_epd:
            if os.path.isdir(output_epd) or total_files > 1:
                base_name = os.path.splitext(os.path.basename(input_pgn))[0]
                target_epd = os.path.join(output_epd, base_name + ".epd")
            else:
                target_epd = output_epd
        else:
            target_epd = os.path.splitext(input_pgn)[0] + ".epd"

        res = convert_single_pgn(
            input_pgn=input_pgn,
            output_epd=target_epd,
            max_cp=max_cp,
            min_depth=min_depth,
            min_elo=min_elo,
            elo_mode=elo_mode,
            deduplicate=deduplicate,
            file_idx=idx,
            total_files=total_files,
            global_seen_fens=global_seen_fens
        )

        if res:
            total_stats['successful_files'] += 1
            total_stats['total_games'] += res['total_games']
            total_stats['exported_positions'] += res['exported_positions']
            total_stats['skipped_book'] += res['skipped_book']
            total_stats['skipped_no_eval'] += res['skipped_no_eval']
            total_stats['skipped_extreme'] += res['skipped_extreme']
            total_stats['skipped_low_depth'] += res['skipped_low_depth']
            total_stats['skipped_low_elo'] += res['skipped_low_elo']
            total_stats['skipped_duplicates'] += res['skipped_duplicates']

    total_time = time.time() - total_stats['start_time']
    print("\n" + "=" * 60)
    print(f"Batch Conversion Complete Summary ({total_stats['successful_files']}/{total_stats['total_files']} files succeeded)")
    print(f"  Total Games Processed:      {total_stats['total_games']:,}")
    print(f"  Exported EPD Positions:     {total_stats['exported_positions']:,}")
    print(f"  Skipped Book Moves:         {total_stats['skipped_book']:,}")
    print(f"  Skipped No-Eval Moves:      {total_stats['skipped_no_eval']:,}")
    print(f"  Skipped Low Depth (< {min_depth}):   {total_stats['skipped_low_depth']:,}")
    print(f"  Skipped Low Elo (< {min_elo}):     {total_stats['skipped_low_elo']:,}")
    print(f"  Skipped Extreme/Mate Moves: {total_stats['skipped_extreme']:,}")
    if deduplicate:
        print(f"  Skipped Duplicates:         {total_stats['skipped_duplicates']:,}")
    print(f"  Total Elapsed Time:         {total_time:.1f}s")
    print("=" * 60)

def main():
    parser = argparse.ArgumentParser(
        description="Convert commented PGN files to ZeroG EPD format.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  python3 convert_pgn_to_epd.py input.pgn output.epd
  python3 convert_pgn_to_epd.py input1.pgn input2.pgn -o /path/to/outdir
  python3 convert_pgn_to_epd.py games/Commented20to26/*.pgn --min-depth 22 --min-elo 3100
"""
    )
    parser.add_argument("input_pgns", nargs="+", help="Path to input commented PGN file(s) or glob pattern(s)")
    parser.add_argument("-o", "--output", help="Output EPD file (for single input) or output directory")
    parser.add_argument("--max-cp", type=int, default=1000, help="Maximum centipawn score magnitude to include (default: 1000)")
    parser.add_argument("--min-depth", type=int, default=22, help="Minimum search depth required to export position (default: 22)")
    parser.add_argument("--min-elo", type=int, default=3100, help="Minimum engine Elo required to export position (default: 3100)")
    parser.add_argument("--elo-mode", choices=['active', 'both', 'either'], default='active', help="Elo filter mode: 'active' engine, 'both' engines, or 'either' engine (default: active)")
    parser.add_argument("--dedup", action="store_true", help="Deduplicate positions by FEN")
    parser.add_argument("--global-dedup", action="store_true", help="Deduplicate positions across ALL input files")

    args = parser.parse_args()

    convert_pgn_to_epd(
        input_pgns=args.input_pgns,
        output_epd=args.output,
        max_cp=args.max_cp,
        min_depth=args.min_depth,
        min_elo=args.min_elo,
        elo_mode=args.elo_mode,
        deduplicate=args.dedup,
        global_dedup=args.global_dedup
    )

if __name__ == '__main__':
    main()


