# Tuning Checklist for ChessAI2027

This file outlines the roadmap for implementing and running tuning of evaluation and search constants in ChessAI2027. We divide the work into two major tuning methodologies: **Texel Tuning (Offline)** for evaluation terms, and **SPSA (Online)** for search parameters.

---

## Part 1: Texel Tuning (Offline Evaluation Tuning)

Texel tuning minimizes the mean squared error between the static evaluation of quiet positions and the actual game outcomes.

### Task 1.1: Quiet Position Filtering & Dataset Preparation
*Goal: Prepare a dataset of quiet positions from game results.*
- [x] Parse `traindata.epd` (or similar EPD records) containing the position and game outcome (`1.0` for White win, `0.5` for draw, `0.0` for Black win).
- [x] Implement a UCI/CLI command or script inside the engine (e.g. `uci` or `main.c`) to filter positions:
  - Run a quick quiescence search (Q-search) on the position.
  - If Q-search score differs from the static evaluation by more than a small margin (e.g., 5-10 centipawns), discard the position.
  - Output quiet positions in FEN/EPD format along with their game result (e.g., `fen ... | result ...`).
- [x] Save the filtered dataset to `quiet_traindata.epd`.


### Task 1.2: Feature/Evaluation Exporter
*Goal: Extract raw evaluation term counts for each position to speed up optimization.*
- [x] Instead of evaluating every position from scratch during each optimizer iteration, write a feature extractor that outputs:
  - The game result ($R_i$)
  - A vector of feature coefficients/counts (e.g., number of pawns, rooks on open files, bishop pair active, mobility counts) for both White and Black.
- [x] Save this extracted linear representation (matrix format) to a JSON or CSV file to be loaded by Python.

### Task 1.3: Python Texel Tuning Script
*Goal: Write the gradient descent optimizer.*
- [x] Create a Python script `tune.py` in the root directory.
- [x] Implement the Sigmoid win-probability function:
  $$E = \frac{1}{1 + 10^{-K \cdot S / 400}}$$
- [x] Implement the MSE loss function:
  $$Loss = \frac{1}{N} \sum_{i=1}^N (R_i - E_i)^2$$
- [x] Use a numerical optimization library (like `scipy.optimize` or `PyTorch` / `TensorFlow`) or write a custom Gradient Descent solver to find the optimal values for:
  - Piece values (pawn, knight, bishop, rook, queen)
  - Mobility bonuses (defined in [eval_mobility.c](file:///Users/kristianekman/ChessAi2027/src/eval/eval_mobility.c))
  - Pawn structure constants (defined in [pawn_eval.c](file:///Users/kristianekman/ChessAi2027/src/eval/pawn_eval.c))
- [x] Export the tuned constants directly as a C header file (e.g. `eval_constants.h`).

---

## Part 2: SPSA Tuning (Online Search/Eval Tuning)

SPSA (Simultaneous Perturbation Stochastic Approximation) is ideal for tuning search parameters (like LMR thresholds, pruning margins in [search.c](file:///Users/kristianekman/ChessAi2027/src/search/search.c)) by playing fast self-play games.

### Task 2.1: UCI Option Parameterization
*Goal: Allow parameters to be configured dynamically via UCI command-line or UCI options.*
- [x] Expose target search/evaluation constants as UCI options. For example, in [uci/uci.c] (or similar entrypoint):
  ```
  option name LMR_Base type spin default 5 min 0 max 20
  option name Futility_Margin type spin default 100 min 0 max 500
  ```
- [x] Ensure the search code in [search.c](file:///Users/kristianekman/ChessAi2027/src/search/search.c) references these dynamic option variables.

### Task 2.2: SPSA Driver Script
*Goal: Implement the SPSA feedback loop.*
- [x] Create a Python script `spsa.py` that manages the tuning iterations.
- [x] In each iteration:
  - Generate a random perturbation vector $\mathbf{\Delta}$ of $\pm c_k$.
  - Launch a `cutechess-cli` match (similar to [selfplay.sh](file:///Users/kristianekman/ChessAi2027/selfplay.sh)) playing Candidate A ($\mathbf{w} + \mathbf{\Delta}$) vs Candidate B ($\mathbf{w} - \mathbf{\Delta}$).
  - Parse the match PGN or output to determine the score.
  - Calculate the gradient approximation and update the parameter vector $\mathbf{w}$.
- [x] Add support for resuming tuning runs in case of interruption.

---

## Part 3: Verification & Validation

- [ ] Run benchmark validation tests to ensure the engine compiles and functions normally with new parameters.
- [ ] Run a Cutechess match of 1000+ games using [selfplay.sh](file:///Users/kristianekman/ChessAi2027/selfplay.sh) to confirm the tuned parameters yield a positive Elo difference.
