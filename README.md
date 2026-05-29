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
- **Iterative Deepening**: Incrementally searches deeper plies, allowing the engine to return the best-move-so-far if time expires.
- **Aspiration Windows**: Limits search scope around the previous depth's score, widening window boundaries only if search fails high or low.
- **Principal Variation Search (PVS)**: Optimizes the alpha-beta search window by searching the principal variation (PV) first with a full window, and subsequent moves with a null/zero window.
- **Quiescence Search**: Extends search at leaf nodes to evaluate only captures and promotions, preventing the horizon effect.
- **Move Ordering**:
  - *PV / TT Move*: Searches the best move from the transposition table or previous iteration first.
  - *MVV-LVA (Most Valuable Victim, Least Valuable Attacker)*: Prioritizes captures that win material.
  - *Killer Moves*: Prioritizes quiet moves that caused a beta-cutoff in helper plies.
- **Pruning, Extensions & Safety Features**:
  - *Null-Move Pruning (NMP)*: Passes the move to detect quick fail-high branches, bypassing search branches if the opponent cannot exploit the pass.
  - *Late Move Reductions (LMR)*: Reduces the search depth of quiet moves that appear late in the move list. If a reduced search fails high, it is re-searched at full depth. Reduces less in PV nodes and for killer moves, and bypasses reduction for tactical moves (captures/promotions), check-giving moves, and shallow depths ($d < 5$).
  - *Singular Extensions (SE)*: Extends the search depth by 1 ply when a move (typically the transposition table best move) is significantly better than all alternative moves. Run at depth $\ge 8$, a reduced-depth verification search ensures no other move can score within a depth-dependent margin of the best move.
  - *Reverse Futility Pruning (RFP)*: Prunes search branches at shallow depths ($d \le 3$) if the static evaluation minus a depth-dependent margin is still greater than or equal to beta.
  - *Futility Pruning*: Prunes quiet moves at shallow depths ($d \le 2$) if the static evaluation plus a depth-dependent margin fails to exceed alpha.
  - *Mate Distance Pruning*: Speeds up search by capping alpha/beta boundaries when a forced mate is found.
  - *Draw & Repetition Detection*: Immediately returns draw evaluations (0) on repetition or 50-move rule limits.

### 6. Evaluation Engine (`src/eval/`)
Scores a given board position statically.
- **Material Value**: Applies standard piece weights (Pawn: 100, Knight: 320, Bishop: 330, Rook: 500, Queen: 900).
- **Piece-Square Tables (PST)**: Encourages positional play (e.g., centralizing knights, active rooks, king safety).
- **Dynamic King Safety**: Selects middle-game or end-game king PSTs based on a dynamic endgame detection heuristic (absence of queens, or queens with minimal minor pieces).

### 7. UCI Protocol Engine (`src/uci/`)
Implements the industry-standard Universal Chess Interface (UCI) protocol, enabling ChessAI2027 to interface with chess GUIs (like Arena, Cute Chess, or ChessBase).
- **Asynchronous Search Loop**: Spawns a background thread to process searching asynchronously, allowing the main loop to listen for standard `stop` and `quit` commands.
- **Commands Handled**: Supports standard instructions including `uci`, `isready`, `ucinewgame`, `position`, `go` (including `wtime`, `btime`, `winc`, `binc`, `movestogo`), `setoption`, `stop`, and `quit`.

### 8. Time Control (`src/search/time_control.h` / `src/search/time_control.c`)
Manages search time budgets dynamically to optimize strength and prevent clock flag-outs.
- **Game Phase Budgeting**: Adjusts estimated moves-to-go dynamically between 20 (endgame) and 40 (opening/middlegame) using non-pawn piece counts.
- **Only Legal Move Exit**: Terminates the search immediately after depth 1 if only one legal move exists at the root, saving time for complex positions.
- **Dynamic Soft Limit Scaling**: Reduces the soft limit by up to 50% if the best move at the root is highly stable across multiple depths, or expands it by 150% if the best move fluctuates or scores drop.
- **Next-Depth Node Growth Prediction**: Predicts if the next depth iteration will exceed the soft limit by tracking the effective branching factor, avoiding wasted computation on incomplete depths.
- **UCI movestogo Support**: Parses and budgets remaining time according to the moves left until the next time control.

### 9. Neural Network Module (`src/nn/`)
A custom Feedforward Neural Network implemented from scratch in C99, optimized for evaluation.
- **Cache-Optimized Layout**: Organizes all weights, biases, activations, pre-activations, and backprop deltas in contiguous memory buffers to maximize L1/L2 cache hit rates.
- **SIMD / NEON Vectorization**: Uses hand-crafted ARM NEON intrinsics (with generic C fallbacks) to vectorize forward pass, backpropagation, and SGD weight update loops.
- **Loop Swapping**: Swaps loop execution order during backpropagation to access weights contiguously, enabling efficient vectorized memory loading.
- **Safe Weight Serialization**: Implements binary serialization (`nn_save`/`nn_load`) with header magic number verification and network architecture validation to prevent corruption.
- **Perspective-Aware Feature Extraction**: Maps a position to a 768-element binary vector (`nn_extract_features`) from the side-to-move's perspective (vertical mirroring + piece color flipping for Black).
- **Standalone C Trainer**: Implements `nn_trainer` to train the evaluation network on centipawn-labeled positions directly in C, utilizing SGD, Fisher-Yates shuffling, validation splitting, and learning rate decay.


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

### Benchmarking and Profiling
The engine includes benchmarking and profiling tools to measure and optimize move generation speed.

- **Run Performance Benchmark**:
  ```bash
  make bench_perft
  ```

- **Run Neural Network Benchmark**:
  ```bash
  make bench_nn
  ```

- **Run CPU Profiler (Chess Engine)**:
  ```bash
  make profile
  ```
  This builds the benchmark target with optimizations and debug symbols enabled (`-O3 -g`), then profiles the run using `valgrind/callgrind` (on Linux) or macOS `sample` (on macOS).

- **Run CPU Profiler (Neural Network)**:
  ```bash
  make profile_nn
  ```
  This compiles the neural network benchmark target with optimizations and debug symbols enabled, then profiles it using macOS `sample` or Callgrind.

### Running Tests
The engine includes exhaustive tests built using the Unity C test framework.

- **Run All Tests**:
  ```bash
  make test
  ```
  This compiles and executes four distinct test runners:
  1. `test_runner` (Core board features, attacks, and encoding)
  2. `fen_test_runner` (FEN parsing validation)
  3. `movegen_test_runner` (Move generator and deep perft counts)
  4. `nn_test_runner` (Neural network initialization, forward/backward propagation, XOR convergence, and weight loading/saving verification)

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

---

## Neural Network Training Data Generation
The engine supports a complete pipeline for generating and labeling training data to train the custom evaluation neural network. This pipeline consists of the following steps: position harvesting during self-play, concatenating and deduplicating harvested positions, labeling unique positions using Stockfish, and training the neural network.

### Step 1: Harvesting Quiet Positions (Self-Play)
During engine self-play matches, quiet (non-tactical) positions visited are automatically recorded if the `SaveQuietPositionsFile` option is set.

The `selfplay.sh` script acts as an entry point for `selfplay.py`, which manages the `cutechess-cli` tournament and outputs a real-time progress dashboard.

Run the self-play script with the `--savefen` flag pointing to the output EPD file:
```bash
./selfplay.sh --savefen quiet_training_positions.epd
```
This launches a match of `NEW` vs `OLD` using `cutechess-cli`, appending valid quiet positions to the specified file with the engine's original search score.

#### Key self-play options:
* `--savefen <file>`: Save harvested quiet training positions.
* `--pgnout <file>`: Output PGN file containing all games played.
* `-games`, `--games <int>`: Total number of games to play (default: `20000`).
* `-concurrency`, `--concurrency <int>`: Concurrency level / thread count (default: `4`).
* `-tc`, `--tc <str>`: Time control configuration (default: `3+0.01`).

### Step 2: Concatenating and Deduplicating Positions
If you have harvested training positions across multiple runs (e.g., `quiet_training_positions.epd` and `quiet_training_positions_2.epd`), you can concatenate them and remove duplicate positions.

1. **Concatenate EPD files**:
   ```bash
   cat quiet_training_positions.epd quiet_training_positions_2.epd > quiet_training_positions_combined.epd
   ```

2. **Deduplicate the combined positions** (requires compiling `epd_dedup` first):
   ```bash
   # Compile the tool
   make epd_dedup

   # Run deduplication
   ./builds/epd_dedup -i quiet_training_positions_combined.epd -o traindata.epd
   ```
   *Alternatively, you can run the default Makefile rule:*
   ```bash
   make dedup
   ```

#### Deduplication CLI Options:
* `-i`, `--input <file>`: Input EPD file (default: `quiet_training_positions_evaluated.epd`).
* `-o`, `--output <file>`: Output deduplicated EPD file (default: `quiet_training_positions_evaluated_dedup.epd`).

### Step 3: Labeling Positions with Stockfish
Once you have generated/deduplicated an EPD file (e.g. `traindata.epd`), run the `evaluate_epd.py` script to evaluate and label all positions using Stockfish. This runs multiple Stockfish processes in parallel to process the file rapidly:

```bash
./evaluate_epd.py -i traindata.epd -o traindata_evaluated.epd -d 10 -c 4
```

#### Key Script Options:
* `-i`, `--input`: Path to the input harvested EPD file (default: `quiet_training_positions.epd`).
* `-o`, `--output`: Path to save the labeled EPD file (default: `quiet_training_positions_evaluated.epd`).
* `-d`, `--depth`: Target search depth for Stockfish (default: `10`).
* `-t`, `--movetime`: Search time limit in milliseconds per position (optional, overrides depth).
* `-c`, `--concurrency`: Number of concurrent Stockfish instances to run (default: number of CPU cores).
* `-p`, `--perspective`: Evaluation score perspective: `side` (side-to-move, default) or `white` (normalized to White's perspective).
* `--mate-to-cp`: Automatically converts mate scores to high centipawn values (e.g. mate in 2 -> `29998` score).

### Step 4: Training the Neural Network (C Trainer)
Once positions are harvested and labeled, train the custom feedforward neural network directly in C99 using the standalone training executable.

1. **Compile the trainer target**:
   ```bash
   make nn_trainer
   ```

2. **Run the training process on the labeled EPD file**:
   ```bash
   ./builds/nn_trainer -i traindata_evaluated.epd -o nn_weights.bin -e 30
   ```
   *Alternatively, run the default training rule (uses default filenames):*
   ```bash
   make train_nn
   ```

#### Trainer CLI Options:
* `-i`, `--input <file>`: Path to the centipawn-labeled EPD file (default: `quiet_training_positions_evaluated.epd`).
* `-o`, `--output <file>`: Destination path for the trained binary weights file (default: `nn_weights.bin`).
* `-e`, `--epochs <num>`: Total training epochs (default: `30`).
* `-l`, `--lr <value>`: Initial Stochastic Gradient Descent (SGD) learning rate (default: `0.01`).
* `-v`, `--val-split <val>`: Validation dataset fraction between `0.0` and `1.0` (default: `0.1` / 10%).
* `-h`, `--help`: Prints command line options help menu.
