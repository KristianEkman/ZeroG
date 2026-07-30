#!/usr/bin/env python3
import sys
import math

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 convert_epd.py <input_epd> <output_epd>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    print(f"Converting {input_path} to tuning-compatible format...")

    # K = 1.13 is the standard scaling constant for mapping evaluation scores
    K = 1.13
    count = 0

    with open(input_path, 'r') as infile, open(output_path, 'w') as outfile:
        for line in infile:
            line = line.strip()
            if not line:
                continue

            # Expecting something like: "... score 110; depth 12;"
            parts = line.split('score ')
            if len(parts) < 2:
                continue

            fen_part = parts[0].strip()
            fen_tokens = fen_part.split()
            if len(fen_tokens) < 4:
                continue

            # Ensure we have a valid 6-token FEN
            active_player = 'w'
            if len(fen_tokens) > 1:
                active_player = fen_tokens[1].lower()

            # Reconstruct FEN using up to 6 tokens (pad if missing)
            while len(fen_tokens) < 6:
                if len(fen_tokens) == 4:
                    fen_tokens.append("0")
                elif len(fen_tokens) == 5:
                    fen_tokens.append("1")
            fen = " ".join(fen_tokens[:6])

            # Parse score
            score_sub = parts[1].split(';')[0].strip()
            try:
                score_val = int(score_sub)
            except ValueError:
                continue

            # Parse depth
            depth_val = 12
            if 'depth ' in parts[1]:
                try:
                    depth_val = int(parts[1].split('depth ')[1].split(';')[0].strip())
                except ValueError:
                    pass

            # Convert score to White's perspective
            score_white = score_val if active_player == 'w' else -score_val

            # Texel sigmoid mapping
            simulated_result = 1.0 / (1.0 + math.pow(10.0, -K * score_white / 400.0))

            # Format expected by ZeroG tune-export:
            # FEN | simulated_result | score_white; score stockfish_score; depth depth;
            outfile.write(f"{fen} | {simulated_result:.6f} | {score_white}; score {score_val}; depth {depth_val};\n")
            count += 1

    print(f"Done! Successfully converted {count} positions to {output_path}")

if __name__ == '__main__':
    main()
