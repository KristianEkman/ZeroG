# ChessAI2027

ChessAI2027 is a high-performance chess engine written in C99, featuring a hybrid board representation, magic bitboards, Zobrist hashing, transposition tables, and a Universal Chess Interface (UCI) protocol engine loop.

---

## Project Structure

- `src/` — Core implementation source files.
- `test/` — Comprehensive Unity-based unit test suites.
- `builds/` — Output directory for compiled targets and test runners.

---

## Core Modules

### 1. Board Representation (`src/boards.h` / `src/boards.c`)
Defines the fundamental data structures, piece and square encodings, and game state.
- **Hybrid Board**: Uses 12 bitboards for coordinates (`pieces[color][type]`), occupancy bitboards for fast blockers lookup (`occ[2]`, `occAll`), and a 64-element mailbox array (`board[64]`) for instant piece-on-square lookups.
- **Attack Tables**: Generates attack tables during initialization (`bitboard_init`) for Knights, Kings, and Pawns.
- **Magic Bitboards**: Implements highly optimized magic bitboard multipliers for Rook and Bishop attack ray generation.

### 2. FEN Parsing & Serialization (`src/fen.h` / `src/fen.c`)
Responsible for reading and writing board layouts using Forsyth-Edwards Notation (FEN).
- `fen_parse`: Parses standard FEN strings, configuring occupancy, pieces, side to move, castling rights, en passant, fifty-move rule, and fullmove counters.
- `fen_serialize`: Serializes the current board state into a standard FEN string.

### 3. Move Generation (`src/movegen/`)
Handles generating pseudo-legal and legal moves, as well as altering the board state.
- **Legality Verification**: Filters pseudo-legal moves by playing them and checking if the friendly king is left in check (`movegen_legal`).
- **Make/Unmake**: Implements low-overhead `apply_move` and `undo_move` functions using a lightweight `Undo` struct to record history.
- **Perft Performance Testing**: Includes recursive tree-walking function `perft` used to validate move generation against standard reference positions.

### 4. Zobrist Hashing & Transposition Table (`src/hashing/`)
Optimizes search speed and prevents redundant evaluations of transposition positions.
- **Zobrist Hashing (`zobrist.c`)**: Generates high-quality, pseudo-random 64-bit keys representing the current board state (pieces, side-to-move, castling rights, and en passant square).
- **Transposition Table (`transposition_table.c`)**: Implements a cache for storing search details (score, depth, bounds, best move) using a depth-preferred replacement strategy.

### 5. Search Engine (`src/search/`)
Determines the best move using search algorithm logic.
- **Alpha-Beta Search**: Implements standard minimax search optimized with alpha-beta pruning.
- **Time Controls**: Manages soft and hard time limits to allocate search time dynamically depending on remaining time and increments.
- **Transposition Table Integration**: Queries the TT to fail-high/fail-low early and stores results to narrow alpha-beta windows.

### 6. UCI Protocol Engine (`src/uci/`)
Implements the industry-standard Universal Chess Interface (UCI) protocol, enabling ChessAI2027 to interface with chess GUIs (like Arena, Cute Chess, or ChessBase).
- **Asynchronous Search Loop**: Spawns a background thread to process searching asynchronously, allowing the main loop to listen for standard `stop` and `quit` commands.
- **Commands Handled**: Supports standard instructions including `uci`, `isready`, `ucinewgame`, `position`, `go`, `setoption`, `stop`, and `quit`.

---

## Build and Testing

### Compilation
The engine uses standard `gcc` and `make` pipelines.

- **Build Engine**:
  ```bash
  make
  ```
  This creates the executable `builds/chessai2027`.

- **Clean Build Directory**:
  ```bash
  make clean
  ```

### Running Tests
The engine includes exhaustive tests built using the Unity C test framework.

- **Run All Tests**:
  ```bash
  make test
  ```
  This compiles and executes three distinct test runners:
  1. `test_runner` (Core board features, attacks, and encoding)
  2. `fen_test_runner` (FEN parsing validation)
  3. `movegen_test_runner` (Move generator and deep perft counts)

---

## Usage

When executed, ChessAI2027 automatically enters **UCI mode** and listens to standard input commands:

```bash
./builds/chessai2027
```

### Example UCI Commands
To start a new game and search for a move:
```text
uci
isready
ucinewgame
position startpos moves e2e4 e7e5
go depth 6
```
To quit the engine:
```text
quit
```
