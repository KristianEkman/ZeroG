import chess
import chess.pgn
import chess.polyglot
import struct
from collections import defaultdict
import os

def generate_book(pgn_path, output_bin_path, max_plies=20):
    if not os.path.exists(pgn_path):
        print(f"Error: PGN file not found at {pgn_path}")
        return

    print(f"Reading PGN from {pgn_path}...")
    
    # Nested dictionary: key -> { raw_move -> count }
    position_moves = defaultdict(lambda: defaultdict(int))
    
    game_count = 0
    with open(pgn_path, "r", encoding="utf-8", errors="ignore") as pgn_file:
        while True:
            game = chess.pgn.read_game(pgn_file)
            if game is None:
                break
                
            game_count += 1
            if game_count % 100 == 0:
                print(f"Processing game {game_count}...")
                
            board = game.board()
            node = game
            ply = 0
            
            while node.variations and ply < max_plies:
                next_node = node.variations[0]
                move = next_node.move
                
                # Calculate Polyglot Zobrist hash
                key = chess.polyglot.zobrist_hash(board)
                
                # Encode Polyglot move
                from_sq = move.from_square
                to_sq = move.to_square
                promo = move.promotion
                
                promo_val = 0
                if promo == chess.KNIGHT:
                    promo_val = 1
                elif promo == chess.BISHOP:
                    promo_val = 2
                elif promo == chess.ROOK:
                    promo_val = 3
                elif promo == chess.QUEEN:
                    promo_val = 4
                    
                raw_move = (from_sq << 6) | to_sq | (promo_val << 12)
                
                position_moves[key][raw_move] += 1
                
                board.push(move)
                node = next_node
                ply += 1

    print(f"Processed {game_count} games.")
    print("Sorting and flattening entries...")
    
    # Flatten the collected entries and sort them by key (as required by Polyglot spec)
    entries = []
    for key, moves in position_moves.items():
        # Let's filter moves that are extremely rare or keep all?
        # Keeping all is good, but we can set the weight as the frequency/count.
        for raw_move, count in moves.items():
            entries.append((key, raw_move, count))
            
    # Sort by key ascending
    entries.sort(key=lambda x: x[0])
    
    print(f"Writing {len(entries)} entries to {output_bin_path}...")
    
    # Each entry is 16 bytes: key (8B), move (2B), weight (2B), learn (4B) in big-endian
    with open(output_bin_path, "wb") as bin_file:
        for key, raw_move, count in entries:
            # Clip count to fit in uint16
            weight = min(count, 0xFFFF)
            bin_file.write(struct.pack(">QHHI", key, raw_move, weight, 0))
            
    print("Done! Custom Polyglot book successfully generated.")

if __name__ == "__main__":
    generate_book("games/top_engine_games.pgn", "book.bin", max_plies=20)
