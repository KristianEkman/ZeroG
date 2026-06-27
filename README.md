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
  - *Lockless Thread-Safety*: Utilizes atomic operations (`_Atomic uint64_t key`) with acquire/release memory semantics to prevent race conditions during concurrent access.
  - *Integrity Verification*: XORs the stored key with a hash of all payload fields (`best_move`, `score`, `depth`, `generation`, `bound`, `occupied`). Any concurrent write corruption is instantly detected as a key mismatch, guaranteeing data integrity.

### 5. Search Engine (`src/search/`)
Determines the best move using search algorithm logic.
- **Lazy SMP Multi-Threading (`threads.h` / `threads.c`)**: Spawns multiple helper threads to run iterative deepening independently.
  - *Search Diversity*: Threads start searching at slightly different offset depths (`1 + (thread_id % 3)`) to explore different parts of the search space concurrently while sharing the transposition table.
  - *Per-Thread Heuristics*: Maintains independent killer moves, history scores, and countermove tables per thread to avoid write contention.
- **Iterative Deepening**: Incrementally searches deeper plies, allowing the engine to return the best-move-so-far if time expires.
- **Aspiration Windows**: Limits search scope around the previous depth's score, widening window boundaries only if search fails high or low.
- **Principal Variation Search (PVS)**: Optimizes the alpha-beta search window by searching the principal variation (PV) first with a full window, and subsequent moves with a null/zero window.
- **Quiescence Search**: Extends search at leaf nodes to evaluate captures and promotions to prevent the horizon effect. Includes **SEE Pruning** to discard losing captures (SEE < 0) when not in check.
- **Move Ordering**:
  - *PV / TT Move*: Searches the best move from the transposition table or previous iteration first.
  - *Static Exchange Evaluation (SEE)*: Orders captures dynamically using SEE values to evaluate material exchange viability (promising exchanges first, bad captures deferred), replacing simple MVV-LVA ordering.
  - *Killer Moves*: Prioritizes quiet moves that caused a beta-cutoff in helper plies.
  - *Countermove Heuristic*: Prioritizes the refutation move matching the opponent's previous move (`countermoves[side][from][to]`) when sorting quiet moves.
  - *History Heuristics*: Orders quiet moves using a dynamically updated table of historical search cutoffs, rewarding moves causing beta-cutoffs and penalizing other tried quiet moves.
  - *History Heuristic Aging*: Ages (halves) history heuristic scores instead of clearing them at the beginning of a search, preserving historical positional context across moves.
  - *Castling*: Grants a minor bonus to castle moves to prioritize king safety.
- **Pruning, Extensions & Safety Features**:
  - *Null-Move Pruning (NMP)*: Passes the move to detect quick fail-high branches, bypassing search branches if the opponent cannot exploit the pass.
  - *History-Based Late Move Reductions (HLMR)*: Reduces the search depth of quiet moves that appear late in the move list, dynamically adjusting the reduction based on history scores. Promising quiet moves are reduced less (or not at all), while moves with bad history are reduced more. Reductions are bypassed for tactical moves (captures/promotions), check-giving moves, and shallow depths ($d < 4$).
  - *Singular Extensions (SE)*: Extends the search depth by 1 ply when a move (typically the transposition table best move) is significantly better than all alternative moves. Run at depth $\ge 8$, a reduced-depth verification search ensures no other move can score within a depth-dependent margin of the best move.
  - *Passed Pawn Push Extensions*: Extends search depth by 1 ply when a passed pawn reaches the 7th rank (for White) or 2nd rank (for Black) to prevent horizon-effect miscalculations near promotion.
  - *One-Reply Extensions*: Extends search depth by 1 ply at internal nodes (`ply > 0` and `depth > 0`) when a position has exactly 1 legal move, resolving forced sequences efficiently.
  - *Reverse Futility Pruning (RFP)*: Prunes search branches at shallow depths ($d \le 3$) if the static evaluation minus a depth-dependent margin is still greater than or equal to beta.
  - *Futility Pruning*: Prunes quiet moves at shallow depths ($d \le 1$) if the static evaluation plus a depth-dependent margin fails to exceed alpha.
  - *Late Move Pruning (LMP)*: Prunes quiet moves that appear late in the search queue at shallow depths ($d \le 4$) using a depth-dependent legal move count threshold.
  - *Check Extensions*: Automatically extends search depth by 1 ply when the king is in check during PVS, verifying forcing lines more deeply.
  - *Mate Distance Pruning*: Speeds up search by capping alpha/beta boundaries when a forced mate is found.
  - *Draw & Repetition Detection*: Immediately returns draw evaluations (0) on repetition or 50-move rule limits.

### 6. Evaluation Engine (`src/eval/`)
Scores a given board position statically.
- **Tapered Game Phase Interpolation**: Dynamically scales all evaluation terms (PSTs, mobility, pawn structures, passed pawns, etc.) from Middlegame to Endgame values using a tapered interpolation based on the remaining non-pawn material, rather than using a binary endgame detection heuristic.
- **Neural Network (NNUE) Evaluation (`src/nn/nn.c`)**:
  - Activates when the `UseNN` option is enabled and a weights file (`nn_weights.bin`) is loaded, overriding classical evaluation.
  - *Thread-Safe Activations*: Stack-allocates activation scratch buffers on a per-thread basis to support thread-safe concurrent evaluation during parallel search.
- **Material Value**: Applies standard piece weights (Pawn: 100, Knight: 320, Bishop: 330, Rook: 500, Queen: 900).
- **Piece-Square Tables (PST)**: Encourages positional play (e.g., centralizing knights, active rooks, king safety).
- **Bishop Pair & Mobility**: Rewards possessing the bishop pair and evaluates piece mobility areas dynamically based on game phase.
- **Passed Pawns**: Identifies passed pawns efficiently using precalculated lookup bitboards. Scores them based on their rank (advancement) and game phase (tapered towards endgame), and applies secondary positional heuristics:
  - *Protection*: Bonus if the passed pawn is defended by another friendly pawn.
  - *Blockade*: Reduces the bonus if any piece blockades the pawn on the square directly in front of it.
  - *Rook/Queen Alignment*: Grants a bonus if a friendly rook/queen supports the pawn from behind on the same file, or applies a penalty if an enemy rook/queen aligns behind it.
- **Pawn Structure**: Assesses pawn weaknesses to reward healthy structures:
  - *Isolated Pawns*: Applies a penalty if a pawn has no friendly pawns on adjacent files (with an increased penalty if the file is semi-open).
  - *Doubled Pawns*: Applies a penalty if multiple friendly pawns occupy the same file.
- **Rooks on Open & Semi-Open Files**: Awards positional bonuses to rooks placed on active files to encourage vertical penetration:
  - *Open File*: A file with no pawns of either color (+20 cp in middlegame, +15 cp in endgame).
  - *Semi-Open File*: A file with no friendly pawns but containing enemy pawns (+10 cp in middlegame, +7 cp in endgame).

### 7. UCI Protocol Engine (`src/uci/`)
Implements the industry-standard Universal Chess Interface (UCI) protocol, enabling ChessAI2027 to interface with chess GUIs (like Arena, Cute Chess, or ChessBase).
- **Asynchronous Search Loop**: Spawns a background thread to process searching asynchronously, allowing the main loop to listen for standard `stop` and `quit` commands.
- **Commands Handled**: Supports standard instructions including `uci`, `isready`, `ucinewgame`, `position`, `go` (including `wtime`, `btime`, `winc`, `binc`, `movestogo`), `setoption`, `stop`, and `quit`.
- **Configurable Options**: Exposes engine internal variables as UCI spin/numeric options for search tuning (e.g., in cutechess-cli or SPSA scripts):
  - `Hash`: Transposition table size (MB).
  - `Threads`: Number of concurrent search threads (default: 1, range: 1–64).
  - `UseNN`: Checkbox/check option to toggle Neural Network evaluation (default: false).
  - `LMR_Base`: Controls LMR reduction aggressiveness.
  - `LMR_Min_Depth`: Minimum remaining depth to apply LMR.
  - `LMR_History_Divisor`: Scaling factor for history-based reduction adjustments.
  - `Futility_Margin` & `Futility_Max_Depth`: Controls the margin and depth boundary of Futility Pruning.
  - `RFP_Margin`: Margin for Reverse Futility Pruning.
  - `NMP_Min_Depth`: Minimum remaining depth for Null-Move Pruning.
  - `Singular_Margin`: Margin for Singular Extensions.
  - `Aspiration_Window`: Margins for aspiration search boundaries.

### 8. Time Control (`src/search/time_control.h` / `src/search/time_control.c`)
Manages search time budgets dynamically to optimize strength and prevent clock flag-outs.
- **Game Phase Budgeting**: Adjusts estimated moves-to-go dynamically between 20 (endgame) and 40 (opening/middlegame) using non-pawn piece counts.
- **Only Legal Move Exit**: Terminates the search immediately after depth 1 if only one legal move exists at the root, saving time for complex positions.
- **Dynamic Soft Limit Scaling**: Reduces the soft limit by up to 50% if the best move at the root is highly stable across multiple depths, or expands it by 150% if the best move fluctuates or scores drop.
- **Next-Depth Node Growth Prediction**: Predicts if the next depth iteration will exceed the soft limit by tracking the effective branching factor, avoiding wasted computation on incomplete depths.
- **UCI movestogo Support**: Parses and budgets remaining time according to the moves left until the next time control.

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

- **Run CPU Profiler**:
  ```bash
  make profile
  ```
  This builds the benchmark target with optimizations and debug symbols enabled (`-O3 -g`), then profiles the run using `valgrind/callgrind` (on Linux) or macOS `sample` (on macOS).

### Running Tests
The engine includes exhaustive tests built using the Unity C test framework.

- **Run All Tests**:
  ```bash
  make test
  ```
  This compiles and executes all test runners:
  1. `test_runner` (Core board features, attacks, and encoding)
  2. `fen_test_runner` (FEN parsing validation)
  3. `movegen_test_runner` (Move generator and deep perft counts)
  4. `eval_test_runner` (Static evaluation calculations)
  5. `search_test_runner` (Search depth and pruning)
  6. `nn_test_runner` (Neural network NNUE evaluation and incremental update check)
  7. `tuning_test_runner` (EPD tuning filter and CSV feature exporter)
  8. `uci_test_runner` (UCI protocol engine state loop commands)

- **Run Specific Tests**:
  You can run individual suites by executing `make test_fen`, `make test_movegen`, `make test_eval`, `make test_search`, `make test_nn`, `make test_tuning`, or `make test_uci`.

### Generating Test Coverage Reports
You can generate comprehensive test coverage reports utilizing native LLVM code coverage tools (Apple Clang/Xcode Command Line Tools).

- **Terminal Summary**:
  ```bash
  make coverage
  ```
  This recompiles with profiling instrumentation, executes all test suites, merges counter traces, and displays a summary table of region, function, line, and branch coverage for the `src/` directory directly in your terminal.

- **Interactive HTML Dashboard**:
  ```bash
  make coverage_html
  ```
  Generates the summary and exports a line-by-line annotated view of the codebase showing execution counts to `builds/coverage_html/index.html`. Open it in any browser to inspect covered paths.

---

## Engine Tuning

ChessAI2027 supports two distinct tuning methodologies to optimize its playing strength: offline **Texel Tuning** for static evaluation terms, and online **SPSA Tuning** for search parameters.

### 1. Offline Evaluation Tuning (Texel Method)
Texel tuning minimizes the mean squared error (MSE) between the static evaluation of quiet positions and the actual game outcomes ($1.0$ for White win, $0.5$ for draw, $0.0$ for Black win) using a Sigmoid function to map evaluation scores to win probabilities:

$$E = \frac{1}{1 + 10^{-K \cdot S / 400}}$$

#### Workflow:
1. **Filter Quiet Positions**: Run a quiescence search (Q-search) to filter out tactical/non-quiet positions from raw training game data:
   ```bash
   ./builds/chessai2027 --tune-filter traindata.epd quiet_traindata.epd
   ```
2. **Export Features**: Extract evaluation counts/coefficients (pawns, rooks on open files, bishop pair, mobility, etc.) to a CSV for fast matrix operations:
   ```bash
   ./builds/chessai2027 --tune-export quiet_traindata.epd quiet_traindata_features.csv
   ```
3. **Run Optimization**: Optimize constants using L-BFGS-B gradient descent:
   ```bash
   python3 tune.py
   ```
   This script calculates the optimal scaling factor $K$, fits the parameters, normalizes them (so the pawn value is exactly 100), and outputs them directly to `src/eval/eval_constants.h`.

---

### 2. Online Search Parameter Tuning (SPSA)
Simultaneous Perturbation Stochastic Approximation (SPSA) is used to tune search parameters (such as LMR reduction bases, pruning margins, and aspiration windows) through automated fast self-play matches.

#### Workflow:
1. **Run SPSA Script**: Execute SPSA optimization, which runs matches of candidate configurations using `cutechess-cli`:
   ```bash
   python3 spsa.py --games 100 --iterations 50 --concurrency 4 --tc 10+0.01
   ```
   - **Perturbations**: The script perturbs parameters by $\pm c_k$ to test Candidate A vs Candidate B.
   - **Hyperparameters**: Automatically decays perturbation step size ($c_k$) and learning rate ($a_k$) across iterations.
2. **Resumption**: In case of interruption, resume SPSA by passing the `--resume` flag:
   ```bash
   python3 spsa.py --resume --games 100 --iterations 50 --concurrency 4
   ```
   State is stored in `spsa_state.json`.
3. **Apply Tuned Parameters**:
   - Update the static variables at the top of [search.c](file:///Users/kristianekman/ChessAi2027/src/search/search.c).
   - Update the reported option defaults in [uci_command.c](file:///Users/kristianekman/ChessAi2027/src/uci/uci_command.c).
   - Recompile the engine:
     ```bash
     make clean && make
     ```

---

## Usage

By default, when executed without arguments, ChessAI2027 automatically enters **UCI mode** and listens to standard input commands:

```bash
./builds/chessai2027
```

### Command-Line Arguments

ChessAI2027 also supports specialized modes for tuning dataset preparation and feature extraction:

- **Filter Quiet Positions**:
  ```bash
  ./builds/chessai2027 --tune-filter <input_epd_file> <output_epd_file>
  ```
  Parses a raw EPD file containing evaluations, runs a quiescence search to verify if a position is quiet (Q-search score matches static evaluation within $10$ cp), maps the score to simulated results, and outputs filtered records.

- **Export Feature Coefficients**:
  ```bash
  ./builds/chessai2027 --tune-export <quiet_epd_file> <output_csv_file>
  ```
  Processes quiet positions to extract their linear evaluation representation (a vector of feature coefficients and the static constant score offset), writing them to a CSV file for rapid offline Texel tuning optimization.


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
* `-w`, `--weights <file>`: Initial binary weights file to continue training from (optional).
* `-e`, `--epochs <num>`: Total training epochs (default: `30`).
* `-l`, `--lr <value>`: Initial Stochastic Gradient Descent (SGD) learning rate (default: `0.01`).
* `-v`, `--val-split <val>`: Validation dataset fraction between `0.0` and `1.0` (default: `0.1` / 10%).
* `-h`, `--help`: Prints command line options help menu.

